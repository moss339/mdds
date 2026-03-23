#ifndef MDDS_DISCOVERY_H
#define MDDS_DISCOVERY_H

#include "types.h"
#include "qos.h"
#include "transport.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace mdds {

// ========== Discovery Client ==========

class Discovery {
public:
    Discovery(DomainId domain_id, ParticipantId participant_id,
              const Endpoint& server_endpoint);
    ~Discovery();

    // Disable copy
    Discovery(const Discovery&) = delete;
    Discovery& operator=(const Discovery&) = delete;

    // Register publisher
    bool register_publisher(const std::string& topic_name,
                           const std::string& type_name,
                           TopicId topic_id,
                           const Endpoint& endpoint,
                           const QoSConfig& qos);

    // Register subscriber
    bool register_subscriber(const std::string& topic_name,
                            const std::string& type_name,
                            TopicId topic_id,
                            const Endpoint& endpoint,
                            const QoSConfig& qos);

    // Unregister
    bool unregister(TopicId topic_id, const std::string& topic_name);

    // Wait for match response
    bool wait_for_match(const std::string& topic_name, int timeout_ms);

    // Get matched publishers for a topic
    std::vector<EndpointInfo> get_matched_publishers(const std::string& topic_name);

    // Get matched subscribers for a topic
    std::vector<EndpointInfo> get_matched_subscribers(const std::string& topic_name);

    // Set callback for match notifications
    void set_match_callback(std::function<void(const std::string& topic_name)> callback);

    // Start discovery client
    bool start();

    // Stop discovery client
    void stop();

    // Check if discovery is running
    bool is_running() const { return running_; }

private:
    void handle_match_response(const MatchResponseMsg& msg);
    void receive_loop();

    DomainId domain_id_;
    ParticipantId participant_id_;
    Endpoint server_endpoint_;
    std::unique_ptr<Transport> transport_;
    std::thread receive_thread_;
    std::atomic<bool> running_{false};

    // Matched endpoints by topic name - protected by match_mutex_
    std::mutex match_mutex_;
    std::unordered_map<std::string, std::vector<EndpointInfo>> matched_publishers_;
    std::unordered_map<std::string, std::vector<EndpointInfo>> matched_subscribers_;

    // Match condition variable - uses the SAME match_mutex_ to avoid race conditions
    std::condition_variable match_cv_;

    // Callback
    std::function<void(const std::string& topic_name)> match_callback_;
};

// ========== Topic Entry (internal) ==========

struct TopicEntry {
    std::vector<EndpointInfo> publishers;
    std::vector<EndpointInfo> subscribers;
};

}  // namespace mdds

#endif  // MDDS_DISCOVERY_H
