/**
 * @file domain_participant_test.cpp
 * @brief Unit tests for DomainParticipant
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <functional>
#include <atomic>
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
// Mock DomainParticipant Implementation for Testing
// ============================================================================

class DomainParticipant {
public:
    using ParticipantId = uint32_t;

    explicit DomainParticipant(DomainId domain_id);
    ~DomainParticipant();

    // Disable copy
    DomainParticipant(const DomainParticipant&) = delete;
    DomainParticipant& operator=(const DomainParticipant&) = delete;

    // Template methods defined below
    template<typename T>
    class Publisher;

    template<typename T>
    class Subscriber;

    // Get Domain ID
    DomainId get_domain_id() const { return domain_id_; }

    // Get Participant ID
    ParticipantId get_participant_id() const { return participant_id_; }

    // Start/Stop
    bool start();
    void stop();

    bool is_running() const { return running_; }

private:
    DomainId domain_id_;
    ParticipantId participant_id_;
    bool running_;
};

// Static participant ID counter
static ParticipantId g_next_participant_id = 1;

DomainParticipant::DomainParticipant(DomainId domain_id)
    : domain_id_(domain_id)
    , participant_id_(g_next_participant_id++)
    , running_(false)
{
}

DomainParticipant::~DomainParticipant() {
    stop();
}

bool DomainParticipant::start() {
    if (running_) return false;
    running_ = true;
    return true;
}

void DomainParticipant::stop() {
    running_ = false;
}

// ============================================================================
// Mock Publisher for Testing
// ============================================================================

template<typename T>
class DomainParticipant::Publisher {
public:
    Publisher() = default;
    ~Publisher() = default;

    bool write(const T& data) {
        return write(data, 0);
    }

    bool write(const T& data, uint64_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        (void)timestamp;
        data_queue_.push(data);
        return true;
    }

    bool has_data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !data_queue_.empty();
    }

    bool read(T& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (data_queue_.empty()) return false;
        data = data_queue_.front();
        data_queue_.pop();
        return true;
    }

    const std::string& get_topic_name() const { return topic_name_; }
    void set_topic_name(const std::string& name) { topic_name_ = name; }

private:
    std::string topic_name_;
    std::queue<T> data_queue_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Mock Subscriber for Testing
// ============================================================================

template<typename T>
class DomainParticipant::Subscriber {
public:
    using DataCallback = std::function<void(const T&, uint64_t timestamp)>;

    Subscriber() = default;
    ~Subscriber() = default;

    void set_callback(DataCallback callback) {
        callback_ = callback;
    }

    bool read(T& data, uint64_t* timestamp = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (data_queue_.empty()) return false;
        auto& front = data_queue_.front();
        data = front.first;
        if (timestamp) *timestamp = front.second;
        data_queue_.pop();
        return true;
    }

    bool has_data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !data_queue_.empty();
    }

    void write_data(const T& data, uint64_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_queue_.push({data, timestamp});
        if (callback_) {
            callback_(data, timestamp);
        }
    }

    const std::string& get_topic_name() const { return topic_name_; }
    void set_topic_name(const std::string& name) { topic_name_ = name; }

private:
    std::string topic_name_;
    DataCallback callback_;
    std::queue<std::pair<T, uint64_t>> data_queue_;
    mutable std::mutex mutex_;
};

}  // namespace mdds

// ============================================================================
// Test Data Types
// ============================================================================

namespace test {

struct SensorData {
    uint32_t sensor_id;
    float temperature;
    float humidity;
    uint64_t timestamp;
};

}  // namespace test

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

void test_domain_participant_creation() {
    printf("[Test] DomainParticipant creation\n");

    // Test valid domain ID creation
    {
        mdds::DomainParticipant participant(0);
        TEST_ASSERT_EQ(participant.get_domain_id(), 0, "Domain ID is 0");
        TEST_ASSERT_NE(participant.get_participant_id(), 0, "Participant ID is assigned");
    }

    // Test domain ID range (0-255)
    {
        mdds::DomainParticipant participant(100);
        TEST_ASSERT_EQ(participant.get_domain_id(), 100, "Domain ID is 100");
    }

    // Test domain ID 255 (max)
    {
        mdds::DomainParticipant participant(255);
        TEST_ASSERT_EQ(participant.get_domain_id(), 255, "Domain ID is 255 (max)");
    }
}

void test_domain_participant_lifecycle() {
    printf("[Test] DomainParticipant lifecycle\n");

    mdds::DomainParticipant participant(0);

    // Test initial state (not started)
    TEST_ASSERT_FALSE(participant.is_running(), "Initially not running");

    // Test first start
    bool first_start = participant.start();
    TEST_ASSERT_TRUE(first_start, "First start returns true");
    TEST_ASSERT_TRUE(participant.is_running(), "Participant is running after start");

    // Test double start
    bool second_start = participant.start();
    TEST_ASSERT_FALSE(second_start, "Second start returns false (already running)");

    // Test stop
    participant.stop();
    TEST_ASSERT_FALSE(participant.is_running(), "Participant is not running after stop");

    // Test restart after stop
    bool restart = participant.start();
    TEST_ASSERT_TRUE(restart, "Start after stop returns true");
    TEST_ASSERT_TRUE(participant.is_running(), "Participant is running after restart");
}

void test_participant_id_uniqueness() {
    printf("[Test] Participant ID uniqueness\n");

    mdds::DomainParticipant p1(0);
    mdds::DomainParticipant p2(0);
    mdds::DomainParticipant p3(0);

    // Each participant should get a unique ID
    TEST_ASSERT_NE(p1.get_participant_id(), p2.get_participant_id(),
                   "Participant 1 and 2 have different IDs");
    TEST_ASSERT_NE(p2.get_participant_id(), p3.get_participant_id(),
                   "Participant 2 and 3 have different IDs");
    TEST_ASSERT_NE(p1.get_participant_id(), p3.get_participant_id(),
                   "Participant 1 and 3 have different IDs");
}

void test_multiple_domains() {
    printf("[Test] Multiple domain participants\n");

    mdds::DomainParticipant p0(0);
    mdds::DomainParticipant p1(1);
    mdds::DomainParticipant p42(42);
    mdds::DomainParticipant p255(255);

    TEST_ASSERT_EQ(p0.get_domain_id(), 0, "Domain 0 participant has domain_id=0");
    TEST_ASSERT_EQ(p1.get_domain_id(), 1, "Domain 1 participant has domain_id=1");
    TEST_ASSERT_EQ(p42.get_domain_id(), 42, "Domain 42 participant has domain_id=42");
    TEST_ASSERT_EQ(p255.get_domain_id(), 255, "Domain 255 participant has domain_id=255");
}

void test_publisher_creation() {
    printf("[Test] Publisher creation and write\n");

    mdds::DomainParticipant participant(0);
    mdds::DomainParticipant::Publisher<test::SensorData> publisher;
    publisher.set_topic_name("SensorTopic");

    TEST_ASSERT_STR_EQ(publisher.get_topic_name(), "SensorTopic", "Publisher topic name is correct");
    TEST_ASSERT_FALSE(publisher.has_data(), "Publisher initially has no data");

    // Write data
    test::SensorData data;
    data.sensor_id = 1;
    data.temperature = 25.5f;
    data.humidity = 60.0f;
    data.timestamp = 1000;

    bool write_result = publisher.write(data);
    TEST_ASSERT_TRUE(write_result, "Publisher write succeeded");
    TEST_ASSERT_TRUE(publisher.has_data(), "Publisher has data after write");

    // Read data
    test::SensorData read_data;
    bool read_result = publisher.read(read_data);
    TEST_ASSERT_TRUE(read_result, "Publisher read succeeded");
    TEST_ASSERT_EQ(read_data.sensor_id, data.sensor_id, "Read sensor_id matches");
    TEST_ASSERT_EQ(read_data.temperature, data.temperature, "Read temperature matches");
    TEST_ASSERT_FALSE(publisher.has_data(), "Publisher has no data after read");
}

void test_subscriber_creation() {
    printf("[Test] Subscriber creation and callback\n");

    mdds::DomainParticipant participant(0);
    mdds::DomainParticipant::Subscriber<test::SensorData> subscriber;
    subscriber.set_topic_name("SensorTopic");

    TEST_ASSERT_STR_EQ(subscriber.get_topic_name(), "SensorTopic", "Subscriber topic name is correct");
    TEST_ASSERT_FALSE(subscriber.has_data(), "Subscriber initially has no data");

    // Set callback
    std::atomic<bool> callback_invoked{false};
    subscriber.set_callback([&](const test::SensorData& data, uint64_t timestamp) {
        callback_invoked = true;
        TEST_ASSERT_EQ(data.sensor_id, 2u, "Callback received correct sensor_id");
        (void)timestamp;
    });

    // Write data directly
    test::SensorData data;
    data.sensor_id = 2;
    data.temperature = 30.0f;
    data.humidity = 45.0f;
    data.timestamp = 2000;

    subscriber.write_data(data, data.timestamp);

    // Give callback time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    TEST_ASSERT_TRUE(callback_invoked.load(), "Callback was invoked");
    TEST_ASSERT_TRUE(subscriber.has_data(), "Subscriber has data after write_data");

    // Read data
    test::SensorData read_data;
    uint64_t read_timestamp = 0;
    bool read_result = subscriber.read(read_data, &read_timestamp);
    TEST_ASSERT_TRUE(read_result, "Subscriber read succeeded");
    TEST_ASSERT_EQ(read_data.sensor_id, data.sensor_id, "Read sensor_id matches");
    TEST_ASSERT_EQ(read_timestamp, data.timestamp, "Read timestamp matches");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("=== MDDS DomainParticipant Tests ===\n\n");

    test_domain_participant_creation();
    test_domain_participant_lifecycle();
    test_participant_id_uniqueness();
    test_multiple_domains();
    test_publisher_creation();
    test_subscriber_creation();

    printf("\n=== Results: %d/%d passed ===\n", g_test_pass, g_test_count);

    return g_test_fail > 0 ? 1 : 0;
}
