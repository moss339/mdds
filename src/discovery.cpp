#include "discovery.h"
#include "transport.h"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

namespace mdds {

Discovery::Discovery(DomainId domain_id, ParticipantId participant_id,
                    const Endpoint& server_endpoint)
    : domain_id_(domain_id)
    , participant_id_(participant_id)
    , server_endpoint_(server_endpoint)
    , running_(false) {
}

Discovery::~Discovery() {
    stop();
}

bool Discovery::register_publisher(const std::string& topic_name,
                                  const std::string& type_name,
                                  TopicId topic_id,
                                  const Endpoint& endpoint,
                                  const QoSConfig& qos) {
    RegisterPublisherMsg msg;
    msg.participant_id = participant_id_;
    msg.topic_id = topic_id;
    msg.topic_name = topic_name;
    msg.type_name = type_name;
    msg.qos_flags = qos.to_flags();

    // Serialize message - calculate exact size needed
    // Format: DiscoveryType + RegisterPublisherMsg + topic_name_len + topic_name + type_name_len + type_name
    const size_t topic_name_len = msg.topic_name.size();
    const size_t type_name_len = msg.type_name.size();
    const size_t total_size = sizeof(DiscoveryType) + sizeof(RegisterPublisherMsg) +
                              sizeof(size_t) + topic_name_len +
                              sizeof(size_t) + type_name_len;

    std::vector<uint8_t> buffer(total_size);

    size_t offset = 0;

    // Discovery type (convert to network byte order for portability)
    DiscoveryType type = DiscoveryType::REGISTER_PUBLISHER;
    uint8_t type_val = static_cast<uint8_t>(type);
    std::memcpy(buffer.data() + offset, &type_val, sizeof(type_val));
    offset += sizeof(type_val);

    // Message fields (convert multi-byte values to network byte order)
    uint32_t participant_id_n = htonl(msg.participant_id);
    uint32_t topic_id_n = htonl(msg.topic_id);
    std::memcpy(buffer.data() + offset, &participant_id_n, sizeof(participant_id_n));
    offset += sizeof(participant_id_n);
    std::memcpy(buffer.data() + offset, &topic_id_n, sizeof(topic_id_n));
    offset += sizeof(topic_id_n);

    // qos_flags
    std::memcpy(buffer.data() + offset, &msg.qos_flags, sizeof(msg.qos_flags));
    offset += sizeof(msg.qos_flags);

    // topic_name length and data
    size_t name_len_n = htonl(static_cast<uint32_t>(topic_name_len));
    std::memcpy(buffer.data() + offset, &name_len_n, sizeof(name_len_n));
    offset += sizeof(name_len_n);
    if (topic_name_len > 0) {
        std::memcpy(buffer.data() + offset, msg.topic_name.data(), topic_name_len);
        offset += topic_name_len;
    }

    // type_name length and data
    name_len_n = htonl(static_cast<uint32_t>(type_name_len));
    std::memcpy(buffer.data() + offset, &name_len_n, sizeof(name_len_n));
    offset += sizeof(name_len_n);
    if (type_name_len > 0) {
        std::memcpy(buffer.data() + offset, msg.type_name.data(), type_name_len);
        offset += type_name_len;
    }

    buffer.resize(offset);

    // Send to discovery server
    return transport_->send(buffer.data(), buffer.size(), server_endpoint_);
}

bool Discovery::register_subscriber(const std::string& topic_name,
                                   const std::string& type_name,
                                   TopicId topic_id,
                                   const Endpoint& endpoint,
                                   const QoSConfig& qos) {
    RegisterSubscriberMsg msg;
    msg.participant_id = participant_id_;
    msg.topic_id = topic_id;
    msg.topic_name = topic_name;
    msg.type_name = type_name;
    msg.qos_flags = qos.to_flags();

    // Serialize message - calculate exact size needed
    const size_t topic_name_len = msg.topic_name.size();
    const size_t type_name_len = msg.type_name.size();
    const size_t total_size = sizeof(DiscoveryType) + sizeof(RegisterSubscriberMsg) +
                              sizeof(size_t) + topic_name_len +
                              sizeof(size_t) + type_name_len;

    std::vector<uint8_t> buffer(total_size);

    size_t offset = 0;

    // Discovery type
    DiscoveryType type = DiscoveryType::REGISTER_SUBSCRIBER;
    uint8_t type_val = static_cast<uint8_t>(type);
    std::memcpy(buffer.data() + offset, &type_val, sizeof(type_val));
    offset += sizeof(type_val);

    // Message fields (convert to network byte order)
    uint32_t participant_id_n = htonl(msg.participant_id);
    uint32_t topic_id_n = htonl(msg.topic_id);
    std::memcpy(buffer.data() + offset, &participant_id_n, sizeof(participant_id_n));
    offset += sizeof(participant_id_n);
    std::memcpy(buffer.data() + offset, &topic_id_n, sizeof(topic_id_n));
    offset += sizeof(topic_id_n);

    // qos_flags
    std::memcpy(buffer.data() + offset, &msg.qos_flags, sizeof(msg.qos_flags));
    offset += sizeof(msg.qos_flags);

    // topic_name length and data
    size_t name_len_n = htonl(static_cast<uint32_t>(topic_name_len));
    std::memcpy(buffer.data() + offset, &name_len_n, sizeof(name_len_n));
    offset += sizeof(name_len_n);
    if (topic_name_len > 0) {
        std::memcpy(buffer.data() + offset, msg.topic_name.data(), topic_name_len);
        offset += topic_name_len;
    }

    // type_name length and data
    name_len_n = htonl(static_cast<uint32_t>(type_name_len));
    std::memcpy(buffer.data() + offset, &name_len_n, sizeof(name_len_n));
    offset += sizeof(name_len_n);
    if (type_name_len > 0) {
        std::memcpy(buffer.data() + offset, msg.type_name.data(), type_name_len);
        offset += type_name_len;
    }

    buffer.resize(offset);

    return transport_->send(buffer.data(), buffer.size(), server_endpoint_);
}

bool Discovery::unregister(TopicId topic_id, const std::string& topic_name) {
    UnregisterMsg msg;
    msg.participant_id = participant_id_;
    msg.topic_id = topic_id;
    msg.topic_name = topic_name;

    // Serialize - calculate exact size
    const size_t topic_name_len = msg.topic_name.size();
    const size_t total_size = sizeof(DiscoveryType) + sizeof(UnregisterMsg) +
                              sizeof(size_t) + topic_name_len;

    std::vector<uint8_t> buffer(total_size);

    size_t offset = 0;

    // Discovery type
    DiscoveryType type = DiscoveryType::UNREGISTER;
    uint8_t type_val = static_cast<uint8_t>(type);
    std::memcpy(buffer.data() + offset, &type_val, sizeof(type_val));
    offset += sizeof(type_val);

    // Message fields (convert to network byte order)
    uint32_t participant_id_n = htonl(msg.participant_id);
    uint32_t topic_id_n = htonl(msg.topic_id);
    std::memcpy(buffer.data() + offset, &participant_id_n, sizeof(participant_id_n));
    offset += sizeof(participant_id_n);
    std::memcpy(buffer.data() + offset, &topic_id_n, sizeof(topic_id_n));
    offset += sizeof(topic_id_n);

    // topic_name length and data
    size_t name_len_n = htonl(static_cast<uint32_t>(topic_name_len));
    std::memcpy(buffer.data() + offset, &name_len_n, sizeof(name_len_n));
    offset += sizeof(name_len_n);
    if (topic_name_len > 0) {
        std::memcpy(buffer.data() + offset, msg.topic_name.data(), topic_name_len);
        offset += topic_name_len;
    }

    buffer.resize(offset);

    return transport_->send(buffer.data(), buffer.size(), server_endpoint_);
}

bool Discovery::wait_for_match(const std::string& topic_name, int timeout_ms) {
    std::unique_lock<std::mutex> lock(match_mutex_);

    auto deadline = std::chrono::steady_clock::now() +
                   std::chrono::milliseconds(timeout_ms);

    while (matched_publishers_.find(topic_name) == matched_publishers_.end() &&
           matched_subscribers_.find(topic_name) == matched_subscribers_.end()) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();

        if (remaining <= 0) {
            return false;  // Timeout
        }

        match_cv_.wait_for(lock, std::chrono::milliseconds(remaining));
    }

    return true;
}

std::vector<EndpointInfo> Discovery::get_matched_publishers(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(match_mutex_);
    auto it = matched_publishers_.find(topic_name);
    if (it != matched_publishers_.end()) {
        return it->second;
    }
    return {};
}

std::vector<EndpointInfo> Discovery::get_matched_subscribers(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(match_mutex_);
    auto it = matched_subscribers_.find(topic_name);
    if (it != matched_subscribers_.end()) {
        return it->second;
    }
    return {};
}

void Discovery::set_match_callback(std::function<void(const std::string& topic_name)> callback) {
    std::lock_guard<std::mutex> lock(match_mutex_);
    match_callback_ = std::move(callback);
}

bool Discovery::start() {
    if (running_) {
        return false;
    }

    // Create UDP transport for discovery
    transport_ = TransportFactory::create_udp_transport(domain_id_);

    Endpoint local_endpoint("0.0.0.0", 0, TransportType::UDP);
    if (!transport_->init(local_endpoint)) {
        return false;
    }

    running_ = true;

    // Start receive thread
    receive_thread_ = std::thread([this]() { receive_loop(); });

    return true;
}

void Discovery::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    if (transport_) {
        transport_->close();
        transport_.reset();
    }
}

void Discovery::handle_match_response(const MatchResponseMsg& msg) {
    std::lock_guard<std::mutex> lock(match_mutex_);

    if (msg.result == 0) {  // Success
        matched_publishers_[msg.topic_name] = msg.publishers;
    }

    // Notify waiting threads
    match_cv_.notify_all();

    // Invoke callback
    if (match_callback_) {
        match_callback_(msg.topic_name);
    }
}

void Discovery::receive_loop() {
    std::vector<uint8_t> buffer(4096);
    Endpoint sender;

    while (running_) {
        size_t received = 0;
        if (transport_->receive(buffer.data(), buffer.size(), &received, &sender)) {
            if (received > 0) {
                // Parse discovery message
                if (received >= sizeof(DiscoveryType)) {
                    DiscoveryType type;
                    std::memcpy(&type, buffer.data(), sizeof(type));

                    if (type == DiscoveryType::MATCH_RESPONSE) {
                        size_t offset = sizeof(type);

                        // Parse MatchResponseMsg - verify we have enough data
                        // Format: topic_id (4) + result (1) + topic_name_len (4) + topic_name + publishers array
                        if (received >= offset + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(size_t)) {
                            MatchResponseMsg msg;

                            // topic_id (network byte order)
                            uint32_t topic_id_n;
                            std::memcpy(&topic_id_n, buffer.data() + offset, sizeof(topic_id_n));
                            msg.topic_id = ntohl(topic_id_n);
                            offset += sizeof(topic_id_n);

                            // result
                            std::memcpy(&msg.result, buffer.data() + offset, sizeof(msg.result));
                            offset += sizeof(msg.result);

                            // topic_name_len (network byte order)
                            uint32_t name_len_n;
                            std::memcpy(&name_len_n, buffer.data() + offset, sizeof(name_len_n));
                            uint32_t topic_name_len = ntohl(name_len_n);
                            offset += sizeof(topic_name_len);

                            // topic_name
                            if (topic_name_len > 0 && received >= offset + topic_name_len) {
                                msg.topic_name = std::string(reinterpret_cast<const char*>(buffer.data() + offset), topic_name_len);
                                offset += topic_name_len;
                            } else {
                                continue;  // Invalid message
                            }

                            // Publishers array - parse vector of EndpointInfo
                            if (received >= offset + sizeof(size_t)) {
                                std::memcpy(&name_len_n, buffer.data() + offset, sizeof(name_len_n));
                                uint32_t publisher_count = ntohl(name_len_n);
                                offset += sizeof(name_len_n);

                                // Skip publishers parsing for now as EndpointInfo contains complex structures
                                // In a full implementation, we would parse each EndpointInfo here
                                (void)publisher_count;  // Will be used in full implementation
                            }

                            handle_match_response(msg);
                        }
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

}  // namespace mdds
