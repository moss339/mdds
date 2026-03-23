#include "discovery_server.h"
#include "transport.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

namespace mdds {

DiscoveryServer::DiscoveryServer(uint16_t port)
    : port_(port)
    , running_(false) {

    server_endpoint_.address_ = "0.0.0.0";
    server_endpoint_.port_ = port;
    server_endpoint_.type_ = TransportType::UDP;
}

DiscoveryServer::~DiscoveryServer() {
    stop();
}

bool DiscoveryServer::start() {
    if (running_) {
        return false;
    }

    // Create UDP transport
    transport_ = TransportFactory::create_udp_transport(0);

    Endpoint local_endpoint("0.0.0.0", port_, TransportType::UDP);
    if (!transport_->init(local_endpoint)) {
        return false;
    }

    server_endpoint_ = transport_->get_local_endpoint();
    running_ = true;

    // Start receive thread
    worker_thread_ = std::thread([this]() { receive_loop(); });

    return true;
}

void DiscoveryServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    if (transport_) {
        transport_->close();
        transport_.reset();
    }
}

Endpoint DiscoveryServer::get_server_endpoint() const {
    return server_endpoint_;
}

size_t DiscoveryServer::get_topic_count() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return topic_registry_.size();
}

size_t DiscoveryServer::get_publisher_count(const std::string& topic_name) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = topic_registry_.find(topic_name);
    if (it != topic_registry_.end()) {
        return it->second.publishers.size();
    }
    return 0;
}

size_t DiscoveryServer::get_subscriber_count(const std::string& topic_name) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = topic_registry_.find(topic_name);
    if (it != topic_registry_.end()) {
        return it->second.subscribers.size();
    }
    return 0;
}

void DiscoveryServer::handle_register_publisher(const RegisterPublisherMsg& msg,
                                                const Endpoint& from) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    EndpointInfo info;
    info.participant_id = msg.participant_id;
    info.topic_id = msg.topic_id;
    info.topic_name = msg.topic_name;
    info.type_name = msg.type_name;
    info.endpoint = from;
    info.qos_flags = msg.qos_flags;

    TopicEntry& entry = topic_registry_[msg.topic_name];
    entry.publishers.push_back(info);

    // Notify subscriber about new publisher
    if (!entry.subscribers.empty()) {
        notify_match(msg.topic_name, entry.publishers, entry.subscribers);
    }

    std::cout << "[DiscoveryServer] Registered publisher for topic: " << msg.topic_name
              << " from " << from.to_string() << std::endl;
}

void DiscoveryServer::handle_register_subscriber(const RegisterSubscriberMsg& msg,
                                                  const Endpoint& from) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    EndpointInfo info;
    info.participant_id = msg.participant_id;
    info.topic_id = msg.topic_id;
    info.topic_name = msg.topic_name;
    info.type_name = msg.type_name;
    info.endpoint = from;
    info.qos_flags = msg.qos_flags;

    TopicEntry& entry = topic_registry_[msg.topic_name];
    entry.subscribers.push_back(info);

    // Notify publisher about new subscriber
    if (!entry.publishers.empty()) {
        notify_match(msg.topic_name, entry.publishers, entry.subscribers);
    }

    std::cout << "[DiscoveryServer] Registered subscriber for topic: " << msg.topic_name
              << " from " << from.to_string() << std::endl;
}

void DiscoveryServer::handle_unregister(const UnregisterMsg& msg, const Endpoint& from) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = topic_registry_.find(msg.topic_name);
    if (it == topic_registry_.end()) {
        return;
    }

    TopicEntry& entry = it->second;

    // Remove publisher
    for (auto pub_it = entry.publishers.begin(); pub_it != entry.publishers.end(); ++pub_it) {
        if (pub_it->participant_id == msg.participant_id) {
            entry.publishers.erase(pub_it);
            break;
        }
    }

    // Remove subscriber
    for (auto sub_it = entry.subscribers.begin(); sub_it != entry.subscribers.end(); ++sub_it) {
        if (sub_it->participant_id == msg.participant_id) {
            entry.subscribers.erase(sub_it);
            break;
        }
    }

    // Clean up empty topic
    if (entry.publishers.empty() && entry.subscribers.empty()) {
        topic_registry_.erase(it);
    }

    std::cout << "[DiscoveryServer] Unregistered endpoint for topic: " << msg.topic_name
              << " from " << from.to_string() << std::endl;
}

void DiscoveryServer::notify_match(const std::string& topic_name,
                                  const std::vector<EndpointInfo>& publishers,
                                  const std::vector<EndpointInfo>& subscribers) {
    // Notify each subscriber about all publishers
    for (const auto& subscriber : subscribers) {
        MatchResponseMsg msg;
        msg.topic_id = subscriber.topic_id;
        msg.topic_name = topic_name;
        msg.publishers = publishers;
        msg.result = 0;  // Success

        send_match_response(subscriber, topic_name, publishers);
    }

    // Notify each publisher about all subscribers
    for (const auto& publisher : publishers) {
        MatchResponseMsg msg;
        msg.topic_id = publisher.topic_id;
        msg.topic_name = topic_name;
        msg.publishers = subscribers;
        msg.result = 0;  // Success

        send_match_response(publisher, topic_name, subscribers);
    }
}

void DiscoveryServer::send_match_response(const EndpointInfo& endpoint,
                                        const std::string& topic_name,
                                        const std::vector<EndpointInfo>& matched_endpoints) {
    MatchResponseMsg msg;
    msg.topic_id = endpoint.topic_id;
    msg.topic_name = topic_name;
    msg.publishers = matched_endpoints;
    msg.result = 0;

    // Serialize
    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(DiscoveryType) + sizeof(msg) + 256);

    size_t offset = 0;

    DiscoveryType type = DiscoveryType::MATCH_RESPONSE;
    std::memcpy(buffer.data() + offset, &type, sizeof(type));
    offset += sizeof(type);

    std::memcpy(buffer.data() + offset, &msg, sizeof(msg));
    offset += sizeof(msg);

    buffer.resize(offset);

    transport_->send(buffer.data(), buffer.size(), endpoint.endpoint);

    std::cout << "[DiscoveryServer] Sent MATCH_RESPONSE to " << endpoint.endpoint.to_string()
              << " for topic: " << topic_name << std::endl;
}

void DiscoveryServer::check_matches(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = topic_registry_.find(topic_name);
    if (it == topic_registry_.end()) {
        return;
    }

    notify_match(topic_name, it->second.publishers, it->second.subscribers);
}

void DiscoveryServer::receive_loop() {
    std::vector<uint8_t> buffer(4096);
    Endpoint sender;

    while (running_) {
        size_t received = 0;
        if (transport_->receive(buffer.data(), buffer.size(), &received, &sender)) {
            if (received > sizeof(DiscoveryType)) {
                DiscoveryType type;
                std::memcpy(&type, buffer.data(), sizeof(type));

                size_t offset = sizeof(type);

                switch (type) {
                    case DiscoveryType::REGISTER_PUBLISHER: {
                        if (received >= offset + sizeof(RegisterPublisherMsg)) {
                            RegisterPublisherMsg msg;
                            std::memcpy(&msg, buffer.data() + offset, sizeof(msg));
                            offset += sizeof(msg);

                            // Read topic_name
                            if (received >= offset + sizeof(size_t)) {
                                size_t name_len;
                                std::memcpy(&name_len, buffer.data() + offset, sizeof(name_len));
                                offset += sizeof(name_len);
                                if (received >= offset + name_len) {
                                    msg.topic_name = std::string(
                                        reinterpret_cast<const char*>(buffer.data() + offset),
                                        name_len);
                                    offset += name_len;
                                }
                                // Read type_name
                                if (received >= offset + sizeof(size_t)) {
                                    size_t type_len;
                                    std::memcpy(&type_len, buffer.data() + offset, sizeof(type_len));
                                    offset += sizeof(type_len);
                                    if (received >= offset + type_len) {
                                        msg.type_name = std::string(
                                            reinterpret_cast<const char*>(buffer.data() + offset),
                                            type_len);
                                    }
                                }
                                handle_register_publisher(msg, sender);
                            }
                        }
                        break;
                    }

                    case DiscoveryType::REGISTER_SUBSCRIBER: {
                        if (received >= offset + sizeof(RegisterSubscriberMsg)) {
                            RegisterSubscriberMsg msg;
                            std::memcpy(&msg, buffer.data() + offset, sizeof(msg));
                            offset += sizeof(msg);

                            // Read topic_name
                            if (received >= offset + sizeof(size_t)) {
                                size_t name_len;
                                std::memcpy(&name_len, buffer.data() + offset, sizeof(name_len));
                                offset += sizeof(name_len);
                                if (received >= offset + name_len) {
                                    msg.topic_name = std::string(
                                        reinterpret_cast<const char*>(buffer.data() + offset),
                                        name_len);
                                    offset += name_len;
                                }
                                // Read type_name
                                if (received >= offset + sizeof(size_t)) {
                                    size_t type_len;
                                    std::memcpy(&type_len, buffer.data() + offset, sizeof(type_len));
                                    offset += sizeof(type_len);
                                    if (received >= offset + type_len) {
                                        msg.type_name = std::string(
                                            reinterpret_cast<const char*>(buffer.data() + offset),
                                            type_len);
                                    }
                                }
                                handle_register_subscriber(msg, sender);
                            }
                        }
                        break;
                    }

                    case DiscoveryType::UNREGISTER: {
                        if (received >= offset + sizeof(UnregisterMsg)) {
                            UnregisterMsg msg;
                            std::memcpy(&msg, buffer.data() + offset, sizeof(msg));
                            offset += sizeof(msg);

                            if (received >= offset + sizeof(size_t)) {
                                size_t name_len;
                                std::memcpy(&name_len, buffer.data() + offset, sizeof(name_len));
                                offset += sizeof(name_len);
                                if (received >= offset + name_len) {
                                    msg.topic_name = std::string(
                                        reinterpret_cast<const char*>(buffer.data() + offset),
                                        name_len);
                                }
                                handle_unregister(msg, sender);
                            }
                        }
                        break;
                    }

                    default:
                        break;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

}  // namespace mdds
