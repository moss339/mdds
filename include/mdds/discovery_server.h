#ifndef MDDS_DISCOVERY_SERVER_H
#define MDDS_DISCOVERY_SERVER_H

#include "types.h"
#include "qos.h"
#include "transport.h"
#include "discovery.h"
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace moss {
namespace mdds {

// ========== Discovery Server ==========

class DiscoveryServer {
public:
    DiscoveryServer(uint16_t port = DEFAULT_DISCOVERY_PORT);
    ~DiscoveryServer();

    // Disable copy
    DiscoveryServer(const DiscoveryServer&) = delete;
    DiscoveryServer& operator=(const DiscoveryServer&) = delete;

    // Start the discovery server
    bool start();

    // Stop the discovery server
    void stop();

    // Check if server is running
    bool is_running() const { return running_; }

    // Manually trigger match check for a topic
    void check_matches(const std::string& topic_name);

    // Get server endpoint
    Endpoint get_server_endpoint() const;

    // Get topic registry (for debugging)
    size_t get_topic_count() const;
    size_t get_publisher_count(const std::string& topic_name) const;
    size_t get_subscriber_count(const std::string& topic_name) const;

private:
    void handle_register_publisher(const RegisterPublisherMsg& msg, const Endpoint& from);
    void handle_register_subscriber(const RegisterSubscriberMsg& msg, const Endpoint& from);
    void handle_unregister(const UnregisterMsg& msg, const Endpoint& from);
    void notify_match(const std::string& topic_name,
                      const std::vector<EndpointInfo>& publishers,
                      const std::vector<EndpointInfo>& subscribers);
    void send_match_response(const EndpointInfo& endpoint, const std::string& topic_name,
                            const std::vector<EndpointInfo>& matched_endpoints);
    void receive_loop();

    // Topic registry: topic_name -> TopicEntry
    std::unordered_map<std::string, TopicEntry> topic_registry_;
    mutable std::mutex registry_mutex_;

    std::unique_ptr<Transport> transport_;
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    uint16_t port_;
    Endpoint server_endpoint_;
};

}  // namespace mdds

}  // namespace moss
#endif  // MDDS_DISCOVERY_SERVER_H
