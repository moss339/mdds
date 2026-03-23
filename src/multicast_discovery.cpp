#include "multicast_discovery.h"
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

namespace mdds {

MulticastDiscovery::MulticastDiscovery(DomainId domain_id, ParticipantId participant_id)
    : domain_id_(domain_id)
    , participant_id_(participant_id)
    , sock_(-1)
    , multicast_ip_(DEFAULT_MULTICAST_IP)
    , multicast_port_(DEFAULT_MULTICAST_PORT) {
}

MulticastDiscovery::~MulticastDiscovery() {
    stop();
}

void MulticastDiscovery::set_match_callback(MatchCallback callback) {
    std::lock_guard<std::mutex> lock(topic_mutex_);
    match_callback_ = std::move(callback);
}

void MulticastDiscovery::add_local_publisher(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(local_topics_mutex_);
    local_publishers_.push_back(topic_name);
}

void MulticastDiscovery::add_local_subscriber(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(local_topics_mutex_);
    local_subscribers_.push_back(topic_name);
}

void MulticastDiscovery::remove_topic(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(local_topics_mutex_);
    auto it = std::find(local_publishers_.begin(), local_publishers_.end(), topic_name);
    if (it != local_publishers_.end()) local_publishers_.erase(it);
    it = std::find(local_subscribers_.begin(), local_subscribers_.end(), topic_name);
    if (it != local_subscribers_.end()) local_subscribers_.erase(it);
}

bool MulticastDiscovery::start() {
    if (running_.load()) return false;

    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0) {
        return false;
    }

    int reuse = 1;
    if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        close(sock_);
        sock_ = -1;
        return false;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip_.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        close(sock_);
        sock_ = -1;
        return false;
    }

    int ttl = 1;
    if (setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        close(sock_);
        sock_ = -1;
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(multicast_port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock_);
        sock_ = -1;
        return false;
    }

    running_ = true;
    receive_thread_ = std::thread([this]() { receive_loop(); });
    announce_thread_ = std::thread([this]() { announce_loop(); });

    return true;
}

void MulticastDiscovery::stop() {
    if (!running_.load()) return;

    running_ = false;

    if (sock_ >= 0) {
        send_bye();
        shutdown(sock_, SHUT_RDWR);
        close(sock_);
        sock_ = -1;
    }

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    if (announce_thread_.joinable()) {
        announce_thread_.join();
    }
}

void MulticastDiscovery::receive_loop() {
    uint8_t buffer[1024];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    while (running_.load()) {
        ssize_t len = recvfrom(sock_, buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&sender_addr, &addr_len);

        if (len > 0) {
            std::string from_addr = inet_ntoa(sender_addr.sin_addr);
            uint16_t from_port = ntohs(sender_addr.sin_port);
            handle_message(buffer, static_cast<size_t>(len), from_addr, from_port);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void MulticastDiscovery::announce_loop() {
    while (running_.load()) {
        send_announce();
        cleanup_stale_peers();

        for (int i = 0; i < ANNOUNCE_INTERVAL_SEC && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void MulticastDiscovery::send_announce() {
    std::lock_guard<std::mutex> lock(local_topics_mutex_);

    for (const auto& topic : local_publishers_) {
        send_announce_topic(topic, NodeRole::PUBLISHER);
    }
    for (const auto& topic : local_subscribers_) {
        send_announce_topic(topic, NodeRole::SUBSCRIBER);
    }
}

void MulticastDiscovery::send_announce_topic(const std::string& topic_name, NodeRole role) {
    uint8_t buffer[256];
    std::memset(buffer, 0, sizeof(buffer));

    MulticastHeader* header = reinterpret_cast<MulticastHeader*>(buffer);
    header->magic = MDDS_MAGIC;
    header->version = 0x01;
    header->type = static_cast<uint8_t>(MulticastMsgType::ANNOUNCE);
    header->flags = 0;
    header->domain_id = domain_id_;
    header->sender_id = participant_id_;

    uint8_t* payload = buffer + sizeof(MulticastHeader);
    size_t offset = 0;

    std::strncpy(reinterpret_cast<char*>(payload), topic_name.c_str(), 32);
    offset += 32;

    payload[offset++] = static_cast<uint8_t>(role);
    payload[offset++] = static_cast<uint8_t>(TransportType::UDP);

    std::memcpy(payload + offset, &participant_id_, 4);
    offset += 4;

    header->length = static_cast<uint16_t>(offset);

    send_multicast(buffer, sizeof(MulticastHeader) + offset);
}

void MulticastDiscovery::send_bye() {
    uint8_t buffer[sizeof(MulticastHeader) + 8];
    std::memset(buffer, 0, sizeof(buffer));

    MulticastHeader* header = reinterpret_cast<MulticastHeader*>(buffer);
    header->magic = MDDS_MAGIC;
    header->version = 0x01;
    header->type = static_cast<uint8_t>(MulticastMsgType::BYE);
    header->flags = 0;
    header->domain_id = domain_id_;
    header->length = 4;
    header->sender_id = participant_id_;

    std::memcpy(buffer + sizeof(MulticastHeader), &participant_id_, 4);

    send_multicast(buffer, sizeof(buffer));
}

bool MulticastDiscovery::send_multicast(const void* data, size_t size) {
    if (sock_ < 0) return false;

    struct sockaddr_in dest_addr;
    std::memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(multicast_port_);
    dest_addr.sin_addr.s_addr = inet_addr(multicast_ip_.c_str());

    ssize_t sent = sendto(sock_, data, size, 0,
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    return sent == static_cast<ssize_t>(size);
}

void MulticastDiscovery::handle_message(const uint8_t* data, size_t len,
                                       const std::string& from_addr, uint16_t from_port) {
    if (len < sizeof(MulticastHeader)) return;

    const MulticastHeader* header = reinterpret_cast<const MulticastHeader*>(data);

    if (header->magic != MDDS_MAGIC) return;
    if (header->domain_id != domain_id_) return;
    if (header->sender_id == participant_id_) return;

    const uint8_t* payload = data + sizeof(MulticastHeader);
    size_t payload_len = header->length;

    switch (static_cast<MulticastMsgType>(header->type)) {
        case MulticastMsgType::ANNOUNCE: {
            if (payload_len < 33) return;

            char topic_name[33] = {0};
            std::memcpy(topic_name, payload, 32);
            NodeRole role = static_cast<NodeRole>(payload[32]);
            uint32_t sender_id = participant_id_;
            std::memcpy(&sender_id, payload + 33, 4);

            {
                std::lock_guard<std::mutex> lock(peers_mutex_);
                PeerEntry& peer = peers_[sender_id];
                peer.participant_id = sender_id;
                peer.address = from_addr;
                peer.port = from_port;
                peer.transport = TransportType::UDP;
                peer.last_announce = std::chrono::steady_clock::now();

                if (role == NodeRole::PUBLISHER) {
                    if (std::find(peer.published_topics.begin(),
                                  peer.published_topics.end(), topic_name)
                        == peer.published_topics.end()) {
                        peer.published_topics.push_back(topic_name);
                    }
                } else if (role == NodeRole::SUBSCRIBER) {
                    if (std::find(peer.subscribed_topics.begin(),
                                  peer.subscribed_topics.end(), topic_name)
                        == peer.subscribed_topics.end()) {
                        peer.subscribed_topics.push_back(topic_name);
                    }
                }
            }

            check_and_notify_match(topic_name);
            break;
        }

        case MulticastMsgType::BYE: {
            if (payload_len < 4) return;
            uint32_t bye_id = 0;
            std::memcpy(&bye_id, payload, 4);

            {
                std::lock_guard<std::mutex> lock(peers_mutex_);
                peers_.erase(bye_id);
            }
            break;
        }

        default:
            break;
    }
}

void MulticastDiscovery::check_and_notify_match(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(topic_mutex_);

    std::vector<PeerEntry> pubs;
    std::vector<PeerEntry> subs;

    {
        std::lock_guard<std::mutex> lock_peers(peers_mutex_);
        for (const auto& pair : peers_) {
            const PeerEntry& peer = pair.second;
            if (!peer.is_alive()) continue;

            for (const auto& t : peer.published_topics) {
                if (t == topic_name) pubs.push_back(peer);
            }
            for (const auto& t : peer.subscribed_topics) {
                if (t == topic_name) subs.push_back(peer);
            }
        }
    }

    if (match_callback_ && (!pubs.empty() || !subs.empty())) {
        match_callback_(topic_name, pubs, subs);
    }
}

void MulticastDiscovery::cleanup_stale_peers() {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto it = peers_.begin(); it != peers_.end(); ) {
        if (!it->second.is_alive()) {
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<PeerEntry> MulticastDiscovery::get_publishers(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::vector<PeerEntry> result;
    for (const auto& pair : peers_) {
        const PeerEntry& peer = pair.second;
        if (!peer.is_alive()) continue;
        for (const auto& t : peer.published_topics) {
            if (t == topic_name) {
                result.push_back(peer);
                break;
            }
        }
    }
    return result;
}

std::vector<PeerEntry> MulticastDiscovery::get_subscribers(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::vector<PeerEntry> result;
    for (const auto& pair : peers_) {
        const PeerEntry& peer = pair.second;
        if (!peer.is_alive()) continue;
        for (const auto& t : peer.subscribed_topics) {
            if (t == topic_name) {
                result.push_back(peer);
                break;
            }
        }
    }
    return result;
}

}  // namespace mdds
