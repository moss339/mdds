#ifndef MDDS_MULTICAST_DISCOVERY_H
#define MDDS_MULTICAST_DISCOVERY_H

#include "types.h"
#include "transport.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <functional>

namespace moss {
namespace mdds {

constexpr const char* DEFAULT_MULTICAST_IP = "239.255.0.1";
constexpr uint16_t DEFAULT_MULTICAST_PORT = 7411;
constexpr int PEER_TIMEOUT_SEC = 15;
constexpr int ANNOUNCE_INTERVAL_SEC = 5;

enum class MulticastMsgType : uint8_t {
    ANNOUNCE = 0x10,
    QUERY = 0x11,
    RESPONSE = 0x12,
    BYE = 0x13
};

enum class NodeRole : uint8_t {
    PUBLISHER = 0x01,
    SUBSCRIBER = 0x02
};

#pragma pack(push, 1)
struct MulticastHeader {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint16_t domain_id;
    uint16_t length;
    uint32_t sender_id;
};
#pragma pack(pop)

struct PeerEntry {
    uint32_t participant_id;
    std::string address;
    uint16_t port;
    TransportType transport;
    std::vector<std::string> published_topics;
    std::vector<std::string> subscribed_topics;
    std::chrono::steady_clock::time_point last_announce;

    bool is_alive() const {
        return std::chrono::steady_clock::now() - last_announce
               < std::chrono::seconds(PEER_TIMEOUT_SEC);
    }
};

class MulticastDiscovery {
public:
    using MatchCallback = std::function<void(const std::string& topic_name,
                                            const std::vector<PeerEntry>& publishers,
                                            const std::vector<PeerEntry>& subscribers)>;

    MulticastDiscovery(DomainId domain_id, ParticipantId participant_id);
    ~MulticastDiscovery();

    void set_match_callback(MatchCallback callback);

    void add_local_publisher(const std::string& topic_name);
    void add_local_subscriber(const std::string& topic_name);
    void remove_topic(const std::string& topic_name);

    bool start();
    void stop();

    std::vector<PeerEntry> get_publishers(const std::string& topic_name);
    std::vector<PeerEntry> get_subscribers(const std::string& topic_name);

    bool is_running() const { return running_.load(); }

private:
    void receive_loop();
    void announce_loop();
    void send_announce();
    void send_announce_topic(const std::string& topic_name, NodeRole role);
    void send_bye();
    void cleanup_stale_peers();
    void check_and_notify_match(const std::string& topic_name);

    bool send_multicast(const void* data, size_t size);
    void handle_message(const uint8_t* data, size_t len, const std::string& from_addr, uint16_t from_port);

    DomainId domain_id_;
    ParticipantId participant_id_;

    int sock_;
    std::string multicast_ip_;
    uint16_t multicast_port_;

    std::atomic<bool> running_{false};
    std::thread receive_thread_;
    std::thread announce_thread_;

    std::unordered_map<uint32_t, PeerEntry> peers_;
    std::mutex peers_mutex_;

    std::vector<std::string> local_publishers_;
    std::vector<std::string> local_subscribers_;
    std::mutex local_topics_mutex_;

    MatchCallback match_callback_;

    std::unordered_map<std::string, std::vector<PeerEntry>> topic_publishers_;
    std::unordered_map<std::string, std::vector<PeerEntry>> topic_subscribers_;
    std::mutex topic_mutex_;
};

}  // namespace mdds

}  // namespace moss
#endif  // MDDS_MULTICAST_DISCOVERY_H
