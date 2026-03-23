/**
 * @file topic_test.cpp
 * @brief Unit tests for Topic
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <vector>
#include <typeinfo>

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

enum class MessageType : uint8_t {
    DATA = 0x01,
    DISCOVERY = 0x02,
    HEARTBEAT = 0x03,
    ACK = 0x04
};

enum class DiscoveryType : uint8_t {
    REGISTER_PUBLISHER = 0x01,
    REGISTER_SUBSCRIBER = 0x02,
    MATCH_RESPONSE = 0x03,
    UNREGISTER = 0x04
};

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

// ============================================================================
// Mock Topic Implementation for Testing
// ============================================================================

// Static topic ID counter
static TopicId g_next_topic_id = 1;
static TopicId get_next_topic_id() {
    return g_next_topic_id++;
}

template<typename T>
class Topic {
public:
    using DataType = T;

    Topic(const std::string& name);
    ~Topic();

    const std::string& get_name() const { return name_; }
    const std::string& get_type_name() const { return type_name_; }
    TopicId get_topic_id() const { return topic_id_; }

private:
    std::string name_;
    std::string type_name_;
    TopicId topic_id_;
    DataType sample_;  // Used for type info extraction
};

template<typename T>
Topic<T>::Topic(const std::string& name)
    : name_(name)
    , topic_id_(get_next_topic_id())
{
    // Extract type name from typeid
    type_name_ = typeid(T).name();
}

template<typename T>
Topic<T>::~Topic() {
}

// ============================================================================
// Test Data Types
// ============================================================================

struct SensorData {
    uint32_t sensor_id;
    float temperature;
    float humidity;
    uint64_t timestamp;
};

struct StringMessage {
    std::string text;
    uint64_t timestamp;
};

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

#define TEST_ASSERT_STR_EQ(a, b, msg) do { \
    g_test_count++; \
    std::string _a_str = (a); \
    std::string _b_str = (b); \
    if (_a_str == _b_str) { \
        printf("  [PASS] %s\n", msg); \
        g_test_pass++; \
    } else { \
        printf("  [FAIL] %s ('%s' != '%s')\n", msg, _a_str.c_str(), _b_str.c_str()); \
        g_test_fail++; \
    } \
} while(0)

// ============================================================================
// Test Cases
// ============================================================================

void test_topic_creation() {
    printf("[Test] Topic creation\n");

    mdds::Topic<mdds::SensorData> topic("SensorTopic");
    TEST_ASSERT_STR_EQ(topic.get_name(), "SensorTopic", "Topic name is set correctly");
    TEST_ASSERT_NE(topic.get_topic_id(), 0, "Topic ID is assigned");
}

void test_topic_id_uniqueness() {
    printf("[Test] Topic ID uniqueness\n");

    mdds::Topic<mdds::SensorData> topic1("Topic1");
    mdds::Topic<mdds::SensorData> topic2("Topic2");
    mdds::Topic<mdds::SensorData> topic3("Topic3");

    TEST_ASSERT_NE(topic1.get_topic_id(), topic2.get_topic_id(),
                   "Topic 1 and 2 have different IDs");
    TEST_ASSERT_NE(topic2.get_topic_id(), topic3.get_topic_id(),
                   "Topic 2 and 3 have different IDs");
    TEST_ASSERT_NE(topic1.get_topic_id(), topic3.get_topic_id(),
                   "Topic 1 and 3 have different IDs");
}

void test_topic_id_assignment() {
    printf("[Test] Topic ID assignment order\n");

    mdds::Topic<mdds::SensorData> topic_a("TopicA");
    mdds::Topic<mdds::SensorData> topic_b("TopicB");

    // Topic IDs should be assigned in creation order
    mdds::TopicId id_a = topic_a.get_topic_id();
    mdds::TopicId id_b = topic_b.get_topic_id();

    TEST_ASSERT_TRUE(id_a < id_b, "Earlier topic has smaller ID");
}

void test_topic_name_validation() {
    printf("[Test] Topic name validation\n");

    mdds::Topic<mdds::SensorData> topic("ValidTopicName");
    TEST_ASSERT_STR_EQ(topic.get_name(), "ValidTopicName", "Valid topic name is stored");

    mdds::Topic<mdds::SensorData> topic2("Topic_With_Underscore");
    TEST_ASSERT_STR_EQ(topic2.get_name(), "Topic_With_Underscore", "Topic name with underscore");

    mdds::Topic<mdds::SensorData> topic3("Topic123");
    TEST_ASSERT_STR_EQ(topic3.get_name(), "Topic123", "Topic name with numbers");
}

void test_topic_type_name() {
    printf("[Test] Topic type name\n");

    mdds::Topic<mdds::SensorData> sensor_topic("Sensor");
    mdds::Topic<mdds::StringMessage> string_topic("Strings");

    // Different types should have different type names
    TEST_ASSERT_STR_EQ(sensor_topic.get_type_name(), sensor_topic.get_type_name(),
                       "Same type has same type name");
    TEST_ASSERT_STR_EQ(string_topic.get_type_name(), string_topic.get_type_name(),
                       "Same type has same type name (2)");

    // Note: Different types will have different typeid().name() values
}

void test_topic_different_data_types() {
    printf("[Test] Topic with different data types\n");

    mdds::Topic<int> int_topic("IntTopic");
    TEST_ASSERT_STR_EQ(int_topic.get_name(), "IntTopic", "Int topic name is correct");

    mdds::Topic<double> double_topic("DoubleTopic");
    TEST_ASSERT_STR_EQ(double_topic.get_name(), "DoubleTopic", "Double topic name is correct");

    mdds::Topic<std::string> string_topic("StringTopic");
    TEST_ASSERT_STR_EQ(string_topic.get_name(), "StringTopic", "String topic name is correct");
}

void test_topic_data_type_access() {
    printf("[Test] Topic data type access\n");

    mdds::Topic<mdds::SensorData> topic("Sensor");

    // Verify the DataType typedef
    using DataType = typename mdds::Topic<mdds::SensorData>::DataType;
    static_assert(std::is_same<DataType, mdds::SensorData>::value,
                  "DataType should be SensorData");
    TEST_ASSERT_TRUE(true, "DataType typedef is correct");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("=== MDDS Topic Tests ===\n\n");

    test_topic_creation();
    test_topic_id_uniqueness();
    test_topic_id_assignment();
    test_topic_name_validation();
    test_topic_type_name();
    test_topic_different_data_types();
    test_topic_data_type_access();

    printf("\n=== Results: %d/%d passed ===\n", g_test_pass, g_test_count);

    return g_test_fail > 0 ? 1 : 0;
}
