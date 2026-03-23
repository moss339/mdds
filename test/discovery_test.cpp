/**
 * @file discovery_test.cpp
 * @brief Unit tests for Discovery
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <algorithm>

// ============================================================================
// MDDS Types (minimal copy for testing without depending on implementation)
// ============================================================================

namespace mdds {

using DomainId = uint8_t;
using TopicId = uint32_t;
using ParticipantId = uint32_t;
using SequenceNumber = uint32_t;

constexpr uint32_t MDDS_MAGIC = 0x4D444453;
constexpr uint16_t MDDS_VERSION = 0x0001;
constexpr size_t MDDS_HEADER_SIZE = 24;
constexpr size_t MDDS_MAX_PAYLOAD_SIZE = 64 * 1024;
constexpr uint16_t DEFAULT_DISCOVERY_PORT = 7412;

enum class TransportType : uint8_t {
    UDP = 0x01,
    TCP = 0x02,
    SHM = 0x03
};

enum class MddsError : int {
    OK = 0,
    INVALID_PARAM = 1,
    NOT_FOUND = 2,
    ALREADY_EXISTS = 3,
    NO_MEMORY = 4,
    TIMEOUT = 5,
    NOT_INITIALIZED = 6,
    ALREADY_STARTED = 7,
    NOT_STARTED = 8,
    DISCOVERY_TIMEOUT = 100,
    TOPIC_MISMATCH = 101,
    NO_PUBLISHER = 102,
    NO_SUBSCRIBER = 103,
    MATCH_FAILED = 104,
    SEND_FAILED = 200,
    RECEIVE_FAILED = 201,
    CONNECTION_LOST = 202,
    SERIALIZE_FAILED = 300,
    DESERIALIZE_FAILED = 301,
    BUFFER_OVERFLOW = 302,
};

// Endpoint
struct Endpoint {
    std::string address_;
    uint16_t port_;
    TransportType type_;

    Endpoint() : port_(0), type_(TransportType::UDP) {}
    Endpoint(const std::string& addr, uint16_t port, TransportType type)
        : address_(addr), port_(port), type_(type) {}

    bool operator==(const Endpoint& other) const {
        return address_ == other.address_ && port_ == other.port_;
    }

    std::string to_string() const {
        return address_ + ":" + std::to_string(port_);
    }
};

// Endpoint Info
struct EndpointInfo {
    ParticipantId participant_id;
    TopicId topic_id;
    std::string topic_name;
    std::string type_name;
    Endpoint endpoint;
    uint8_t qos_flags;

    std::string to_string() const {
        return "EndpointInfo[pid=" + std::to_string(participant_id) +
               ", topic=" + topic_name +
               ", endpoint=" + endpoint.to_string() + "]";
    }
};

// Simplified QoS flags
enum class QoSFlags : uint8_t {
    BEST_EFFORT     = 0x01,
    RELIABLE        = 0x02,
    VOLATILE        = 0x10,
    TRANSIENT_LOCAL = 0x20,
};

// Simplified QoS config
struct QoSConfig {
    QoSFlags reliability = QoSFlags::BEST_EFFORT;
    QoSFlags durability = QoSFlags::VOLATILE;

    bool is_compatible(const QoSConfig& other) const {
        if (durability == QoSFlags::TRANSIENT_LOCAL &&
            other.durability == QoSFlags::VOLATILE) {
            return true;
        }
        return durability == other.durability;
    }
};

// Topic entry in registry
struct TopicEntry {
    std::vector<EndpointInfo> publishers;
    std::vector<EndpointInfo> subscribers;
};

// Simplified Discovery class
class Discovery {
public:
    Discovery();
    ~Discovery();

    // Register publisher
    bool register_publisher(const EndpointInfo& info);

    // Register subscriber
    bool register_subscriber(const EndpointInfo& info);

    // Unregister endpoint
    bool unregister(ParticipantId participant_id, TopicId topic_id);

    // Get matched publishers for a subscriber
    std::vector<EndpointInfo> get_matched_publishers(const EndpointInfo& subscriber);

    // Get matched subscribers for a publisher
    std::vector<EndpointInfo> get_matched_subscribers(const EndpointInfo& publisher);

    // Check if topic has any publishers
    bool has_publishers(const std::string& topic_name);

    // Get topic entry count
    size_t get_topic_count() const;

private:
    std::map<std::string, TopicEntry> topic_registry_;
    std::map<TopicId, std::string> topic_id_to_name_;
};

// Discovery implementation
Discovery::Discovery() {
}

Discovery::~Discovery() {
}

bool Discovery::register_publisher(const EndpointInfo& info) {
    TopicEntry& entry = topic_registry_[info.topic_name];
    entry.publishers.push_back(info);
    topic_id_to_name_[info.topic_id] = info.topic_name;
    return true;
}

bool Discovery::register_subscriber(const EndpointInfo& info) {
    TopicEntry& entry = topic_registry_[info.topic_name];
    entry.subscribers.push_back(info);
    topic_id_to_name_[info.topic_id] = info.topic_name;
    return true;
}

bool Discovery::unregister(ParticipantId participant_id, TopicId topic_id) {
    auto it = topic_id_to_name_.find(topic_id);
    if (it == topic_id_to_name_.end()) {
        return false;
    }

    std::string topic_name = it->second;
    TopicEntry& entry = topic_registry_[topic_name];

    // Remove publisher
    auto& pubs = entry.publishers;
    pubs.erase(
        std::remove_if(pubs.begin(), pubs.end(),
            [participant_id](const EndpointInfo& info) {
                return info.participant_id == participant_id;
            }),
        pubs.end()
    );

    // Remove subscriber
    auto& subs = entry.subscribers;
    subs.erase(
        std::remove_if(subs.begin(), subs.end(),
            [participant_id](const EndpointInfo& info) {
                return info.participant_id == participant_id;
            }),
        subs.end()
    );

    return true;
}

std::vector<EndpointInfo> Discovery::get_matched_publishers(const EndpointInfo& subscriber) {
    std::vector<EndpointInfo> result;
    auto it = topic_registry_.find(subscriber.topic_name);
    if (it != topic_registry_.end()) {
        result = it->second.publishers;
    }
    return result;
}

std::vector<EndpointInfo> Discovery::get_matched_subscribers(const EndpointInfo& publisher) {
    std::vector<EndpointInfo> result;
    auto it = topic_registry_.find(publisher.topic_name);
    if (it != topic_registry_.end()) {
        result = it->second.subscribers;
    }
    return result;
}

bool Discovery::has_publishers(const std::string& topic_name) {
    auto it = topic_registry_.find(topic_name);
    if (it != topic_registry_.end()) {
        return !it->second.publishers.empty();
    }
    return false;
}

size_t Discovery::get_topic_count() const {
    return topic_registry_.size();
}

}  // namespace mdds

// ============================================================================
// Test Framework (simple assert-based)
// ============================================================================

static int g_test_count = 0;
static int g_test_pass = 0;
static int g_test_fail = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_test_count++; \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_test_pass++; \
    } else { \
        printf("  [FAIL] %s\n", msg); \
        g_test_fail++; \
    } \
} while(0)

#define TEST_ASSERT_EQ(actual, expected, msg) do { \
    g_test_count++; \
    if ((actual) == (expected)) { \
        printf("  [PASS] %s\n", msg); \
        g_test_pass++; \
    } else { \
        printf("  [FAIL] %s (got %ld, expected %ld)\n", msg, (long)(actual), (long)(expected)); \
        g_test_fail++; \
    } \
} while(0)

#define TEST_ASSERT_NE(a, b, msg) do { \
    g_test_count++; \
    if ((a) != (b)) { \
        printf("  [PASS] %s\n", msg); \
        g_test_pass++; \
    } else { \
        printf("  [FAIL] %s (got same value)\n", msg); \
        g_test_fail++; \
    } \
} while(0)

#define TEST_ASSERT_TRUE(cond, msg) TEST_ASSERT((cond), msg)
#define TEST_ASSERT_FALSE(cond, msg) TEST_ASSERT(!(cond), msg)

// ============================================================================
// Test Cases
// ============================================================================

void test_discovery_creation() {
    printf("[Test] Discovery creation\n");

    mdds::Discovery discovery;
    TEST_ASSERT_TRUE(true, "Discovery instance created");
}

void test_register_publisher() {
    printf("[Test] Register publisher\n");

    mdds::Discovery discovery;

    mdds::EndpointInfo pub_info;
    pub_info.participant_id = 1;
    pub_info.topic_id = 100;
    pub_info.topic_name = "SensorTopic";
    pub_info.type_name = "SensorData";

    bool result = discovery.register_publisher(pub_info);
    TEST_ASSERT_TRUE(result, "Publisher registered successfully");
}

void test_register_subscriber() {
    printf("[Test] Register subscriber\n");

    mdds::Discovery discovery;

    mdds::EndpointInfo sub_info;
    sub_info.participant_id = 2;
    sub_info.topic_id = 101;
    sub_info.topic_name = "SensorTopic";
    sub_info.type_name = "SensorData";

    bool result = discovery.register_subscriber(sub_info);
    TEST_ASSERT_TRUE(result, "Subscriber registered successfully");
}

void test_topic_matching() {
    printf("[Test] Topic matching\n");

    mdds::Discovery discovery;

    // Register publisher
    mdds::EndpointInfo pub_info;
    pub_info.participant_id = 1;
    pub_info.topic_id = 100;
    pub_info.topic_name = "SensorTopic";
    pub_info.type_name = "SensorData";
    discovery.register_publisher(pub_info);

    // Register subscriber with same topic
    mdds::EndpointInfo sub_info;
    sub_info.participant_id = 2;
    sub_info.topic_id = 101;
    sub_info.topic_name = "SensorTopic";
    sub_info.type_name = "SensorData";
    discovery.register_subscriber(sub_info);

    // Get matched publishers for subscriber
    auto matched_pubs = discovery.get_matched_publishers(sub_info);
    TEST_ASSERT_EQ(matched_pubs.size(), 1UL, "Subscriber found 1 matching publisher");
    TEST_ASSERT_EQ(matched_pubs[0].participant_id, pub_info.participant_id,
                   "Matched publisher has correct participant ID");

    // Get matched subscribers for publisher
    auto matched_subs = discovery.get_matched_subscribers(pub_info);
    TEST_ASSERT_EQ(matched_subs.size(), 1UL, "Publisher found 1 matching subscriber");
    TEST_ASSERT_EQ(matched_subs[0].participant_id, sub_info.participant_id,
                   "Matched subscriber has correct participant ID");
}

void test_multiple_publishers_same_topic() {
    printf("[Test] Multiple publishers same topic\n");

    mdds::Discovery discovery;

    // Register 3 publishers for same topic
    for (int i = 1; i <= 3; i++) {
        mdds::EndpointInfo pub_info;
        pub_info.participant_id = i;
        pub_info.topic_id = 100 + i;
        pub_info.topic_name = "SharedTopic";
        pub_info.type_name = "SensorData";
        discovery.register_publisher(pub_info);
    }

    // Register subscriber
    mdds::EndpointInfo sub_info;
    sub_info.participant_id = 100;
    sub_info.topic_id = 200;
    sub_info.topic_name = "SharedTopic";
    sub_info.type_name = "SensorData";
    discovery.register_subscriber(sub_info);

    // Should find all 3 publishers
    auto matched_pubs = discovery.get_matched_publishers(sub_info);
    TEST_ASSERT_EQ(matched_pubs.size(), 3UL, "Subscriber found all 3 publishers");
}

void test_multiple_subscribers_same_topic() {
    printf("[Test] Multiple subscribers same topic\n");

    mdds::Discovery discovery;

    // Register publisher
    mdds::EndpointInfo pub_info;
    pub_info.participant_id = 1;
    pub_info.topic_id = 100;
    pub_info.topic_name = "SharedTopic";
    pub_info.type_name = "SensorData";
    discovery.register_publisher(pub_info);

    // Register 3 subscribers
    for (int i = 1; i <= 3; i++) {
        mdds::EndpointInfo sub_info;
        sub_info.participant_id = 10 + i;
        sub_info.topic_id = 200 + i;
        sub_info.topic_name = "SharedTopic";
        sub_info.type_name = "SensorData";
        discovery.register_subscriber(sub_info);
    }

    // Should find all 3 subscribers
    auto matched_subs = discovery.get_matched_subscribers(pub_info);
    TEST_ASSERT_EQ(matched_subs.size(), 3UL, "Publisher found all 3 subscribers");
}

void test_unregister_publisher() {
    printf("[Test] Unregister publisher\n");

    mdds::Discovery discovery;

    // Register publisher
    mdds::EndpointInfo pub_info;
    pub_info.participant_id = 1;
    pub_info.topic_id = 100;
    pub_info.topic_name = "SensorTopic";
    pub_info.type_name = "SensorData";
    discovery.register_publisher(pub_info);

    // Register subscriber
    mdds::EndpointInfo sub_info;
    sub_info.participant_id = 2;
    sub_info.topic_id = 101;
    sub_info.topic_name = "SensorTopic";
    sub_info.type_name = "SensorData";
    discovery.register_subscriber(sub_info);

    // Verify match exists
    auto matched_pubs = discovery.get_matched_publishers(sub_info);
    TEST_ASSERT_EQ(matched_pubs.size(), 1UL, "Before unregister: 1 publisher matched");

    // Unregister publisher
    bool result = discovery.unregister(pub_info.participant_id, pub_info.topic_id);
    TEST_ASSERT_TRUE(result, "Unregister returned true");

    // Verify no match
    matched_pubs = discovery.get_matched_publishers(sub_info);
    TEST_ASSERT_EQ(matched_pubs.size(), 0UL, "After unregister: 0 publishers matched");
}

void test_unregister_subscriber() {
    printf("[Test] Unregister subscriber\n");

    mdds::Discovery discovery;

    // Register publisher
    mdds::EndpointInfo pub_info;
    pub_info.participant_id = 1;
    pub_info.topic_id = 100;
    pub_info.topic_name = "SensorTopic";
    pub_info.type_name = "SensorData";
    discovery.register_publisher(pub_info);

    // Register subscriber
    mdds::EndpointInfo sub_info;
    sub_info.participant_id = 2;
    sub_info.topic_id = 101;
    sub_info.topic_name = "SensorTopic";
    sub_info.type_name = "SensorData";
    discovery.register_subscriber(sub_info);

    // Verify match exists
    auto matched_subs = discovery.get_matched_subscribers(pub_info);
    TEST_ASSERT_EQ(matched_subs.size(), 1UL, "Before unregister: 1 subscriber matched");

    // Unregister subscriber
    bool result = discovery.unregister(sub_info.participant_id, sub_info.topic_id);
    TEST_ASSERT_TRUE(result, "Unregister returned true");

    // Verify no match
    matched_subs = discovery.get_matched_subscribers(pub_info);
    TEST_ASSERT_EQ(matched_subs.size(), 0UL, "After unregister: 0 subscribers matched");
}

void test_different_topics_no_match() {
    printf("[Test] Different topics do not match\n");

    mdds::Discovery discovery;

    // Register publisher for TopicA
    mdds::EndpointInfo pub_info;
    pub_info.participant_id = 1;
    pub_info.topic_id = 100;
    pub_info.topic_name = "TopicA";
    pub_info.type_name = "SensorData";
    discovery.register_publisher(pub_info);

    // Register subscriber for TopicB
    mdds::EndpointInfo sub_info;
    sub_info.participant_id = 2;
    sub_info.topic_id = 101;
    sub_info.topic_name = "TopicB";
    sub_info.type_name = "SensorData";
    discovery.register_subscriber(sub_info);

    // Should not find any matches
    auto matched_pubs = discovery.get_matched_publishers(sub_info);
    TEST_ASSERT_EQ(matched_pubs.size(), 0UL, "No publishers matched for different topic");

    auto matched_subs = discovery.get_matched_subscribers(pub_info);
    TEST_ASSERT_EQ(matched_subs.size(), 0UL, "No subscribers matched for different topic");
}

void test_has_publishers() {
    printf("[Test] has_publishers check\n");

    mdds::Discovery discovery;

    // Initially no publishers
    TEST_ASSERT_FALSE(discovery.has_publishers("SensorTopic"),
                     "No publishers for non-existent topic");

    // Register publisher
    mdds::EndpointInfo pub_info;
    pub_info.participant_id = 1;
    pub_info.topic_id = 100;
    pub_info.topic_name = "SensorTopic";
    pub_info.type_name = "SensorData";
    discovery.register_publisher(pub_info);

    // Now should have publishers
    TEST_ASSERT_TRUE(discovery.has_publishers("SensorTopic"),
                    "Publishers exist for registered topic");

    // Unregister
    discovery.unregister(pub_info.participant_id, pub_info.topic_id);

    // Should no longer have publishers
    TEST_ASSERT_FALSE(discovery.has_publishers("SensorTopic"),
                     "No publishers after unregister");
}

void test_topic_count() {
    printf("[Test] Topic count\n");

    mdds::Discovery discovery;

    TEST_ASSERT_EQ(discovery.get_topic_count(), 0UL, "Initially 0 topics");

    // Add publisher for TopicA
    mdds::EndpointInfo pub_a;
    pub_a.participant_id = 1;
    pub_a.topic_id = 100;
    pub_a.topic_name = "TopicA";
    pub_a.type_name = "SensorData";
    discovery.register_publisher(pub_a);
    TEST_ASSERT_EQ(discovery.get_topic_count(), 1UL, "After first topic: 1 topic");

    // Add subscriber for TopicB
    mdds::EndpointInfo sub_b;
    sub_b.participant_id = 2;
    sub_b.topic_id = 101;
    sub_b.topic_name = "TopicB";
    sub_b.type_name = "SensorData";
    discovery.register_subscriber(sub_b);
    TEST_ASSERT_EQ(discovery.get_topic_count(), 2UL, "After second topic: 2 topics");

    // Add subscriber for TopicA (same as publisher)
    mdds::EndpointInfo sub_a;
    sub_a.participant_id = 3;
    sub_a.topic_id = 102;
    sub_a.topic_name = "TopicA";
    sub_a.type_name = "SensorData";
    discovery.register_subscriber(sub_a);
    TEST_ASSERT_EQ(discovery.get_topic_count(), 2UL, "After adding same topic: still 2 topics");
}

void test_qos_compatibility() {
    printf("[Test] QoS compatibility\n");

    mdds::QoSConfig config_transient;
    config_transient.durability = mdds::QoSFlags::TRANSIENT_LOCAL;

    mdds::QoSConfig config_volatile;
    config_volatile.durability = mdds::QoSFlags::VOLATILE;

    // TRANSIENT_LOCAL publisher should be compatible with VOLATILE subscriber
    TEST_ASSERT_TRUE(config_transient.is_compatible(config_volatile),
                    "TRANSIENT_LOCAL compatible with VOLATILE");

    // Same durability should be compatible
    TEST_ASSERT_TRUE(config_transient.is_compatible(config_transient),
                    "TRANSIENT_LOCAL compatible with TRANSIENT_LOCAL");
    TEST_ASSERT_TRUE(config_volatile.is_compatible(config_volatile),
                    "VOLATILE compatible with VOLATILE");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("=== MDDS Discovery Tests ===\n\n");

    test_discovery_creation();
    test_register_publisher();
    test_register_subscriber();
    test_topic_matching();
    test_multiple_publishers_same_topic();
    test_multiple_subscribers_same_topic();
    test_unregister_publisher();
    test_unregister_subscriber();
    test_different_topics_no_match();
    test_has_publishers();
    test_topic_count();
    test_qos_compatibility();

    printf("\n=== Results: %d/%d passed ===\n", g_test_pass, g_test_count);

    return g_test_fail > 0 ? 1 : 0;
}
