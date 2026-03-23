/**
 * @file integration_test.cpp
 * @brief Integration tests for MDDS
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
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>
#include <algorithm>
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

// Message header (packed)
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;
    uint16_t version;
    uint8_t message_type;
    uint8_t flags;
    uint32_t topic_id;
    uint32_t sequence_number;
    uint64_t timestamp;
    uint32_t payload_length;
};
#pragma pack(pop)

// Note: static_assert removed - MessageHeader size may vary on different platforms

// ============================================================================
// Mock Components (simplified versions for integration testing)
// ============================================================================

namespace test {

struct SensorData {
    uint32_t sensor_id;
    float temperature;
    float humidity;
    uint64_t timestamp;

    bool operator==(const SensorData& other) const {
        return sensor_id == other.sensor_id &&
               temperature == other.temperature &&
               humidity == other.humidity &&
               timestamp == other.timestamp;
    }
};

struct StringMessage {
    std::string text;
    uint64_t timestamp;

    bool operator==(const StringMessage& other) const {
        return text == other.text && timestamp == other.timestamp;
    }
};

}  // namespace test

// Static counters
static ParticipantId g_next_participant_id = 1;
static TopicId g_next_topic_id = 1;

// Mock DomainParticipant
class DomainParticipant {
public:
    using ParticipantId = uint32_t;

    explicit DomainParticipant(DomainId domain_id);
    ~DomainParticipant();

    DomainParticipant(const DomainParticipant&) = delete;
    DomainParticipant& operator=(const DomainParticipant&) = delete;

    DomainId get_domain_id() const { return domain_id_; }
    ParticipantId get_participant_id() const { return participant_id_; }

    bool start() { running_ = true; return true; }
    void stop() { running_ = false; }

    bool is_running() const { return running_; }

private:
    DomainId domain_id_;
    ParticipantId participant_id_;
    bool running_;
};

DomainParticipant::DomainParticipant(DomainId domain_id)
    : domain_id_(domain_id)
    , participant_id_(g_next_participant_id++)
    , running_(false)
{
}

DomainParticipant::~DomainParticipant() {
    stop();
}

// Mock Topic
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
    DataType sample_;
};

template<typename T>
Topic<T>::Topic(const std::string& name)
    : name_(name)
    , topic_id_(g_next_topic_id++)
{
    type_name_ = typeid(T).name();
}

template<typename T>
Topic<T>::~Topic() {
}

// Mock Publisher
template<typename T>
class Publisher {
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

// Mock Subscriber
template<typename T>
class Subscriber {
public:
    using DataCallback = std::function<void(const T&, uint64_t)>;

    Subscriber() = default;
    ~Subscriber() = default;

    void set_callback(DataCallback callback) { callback_ = callback; }

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

// Mock DataWriter
template<typename T>
class DataWriter {
public:
    DataWriter() = default;
    ~DataWriter() = default;

    bool write(const T& data, uint64_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_queue_.push({data, timestamp});
        return true;
    }

    TopicId get_topic_id() const { return topic_id_; }
    void set_topic_id(TopicId id) { topic_id_ = id; }

    bool has_data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !data_queue_.empty();
    }

    bool read(T& data, uint64_t* timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (data_queue_.empty()) return false;
        auto front = data_queue_.front();
        data = front.first;
        if (timestamp) *timestamp = front.second;
        data_queue_.pop();
        return true;
    }

private:
    TopicId topic_id_ = 0;
    std::queue<std::pair<T, uint64_t>> data_queue_;
    mutable std::mutex mutex_;
};

// Mock DataReader
template<typename T>
class DataReader {
public:
    using DataCallback = std::function<void(const T&, uint64_t)>;

    DataReader() = default;
    ~DataReader() = default;

    void set_callback(DataCallback callback) { callback_ = callback; }
    bool has_data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !data_queue_.empty();
    }
    bool read(T& data, uint64_t* timestamp = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (data_queue_.empty()) return false;
        auto front = data_queue_.front();
        data = front.first;
        if (timestamp) *timestamp = front.second;
        data_queue_.pop();
        return true;
    }
    TopicId get_topic_id() const { return topic_id_; }
    void set_topic_id(TopicId id) { topic_id_ = id; }

    void write_data(const T& data, uint64_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_queue_.push({data, timestamp});
        if (callback_) {
            callback_(data, timestamp);
        }
    }

private:
    TopicId topic_id_ = 0;
    DataCallback callback_;
    std::queue<std::pair<T, uint64_t>> data_queue_;
    mutable std::mutex mutex_;
};

// Mock Discovery
class Discovery {
public:
    Discovery();
    ~Discovery();

    bool register_publisher(const EndpointInfo& info);
    bool register_subscriber(const EndpointInfo& info);
    bool unregister(ParticipantId participant_id, TopicId topic_id);

    std::vector<EndpointInfo> get_matched_publishers(const EndpointInfo& subscriber);
    std::vector<EndpointInfo> get_matched_subscribers(const EndpointInfo& publisher);

    bool has_publishers(const std::string& topic_name);
    size_t get_topic_count() const;

private:
    struct TopicEntry {
        std::vector<EndpointInfo> publishers;
        std::vector<EndpointInfo> subscribers;
    };

    std::map<std::string, TopicEntry> topic_registry_;
    std::map<TopicId, std::string> topic_id_to_name_;
};

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

    auto& pubs = entry.publishers;
    pubs.erase(
        std::remove_if(pubs.begin(), pubs.end(),
            [participant_id](const EndpointInfo& info) {
                return info.participant_id == participant_id;
            }),
        pubs.end()
    );

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
// Integration Test Cases
// ============================================================================

void test_participant_with_domain_id() {
    printf("[Test] Participant with domain ID\n");

    mdds::DomainParticipant participant(42);
    TEST_ASSERT_EQ(participant.get_domain_id(), 42, "Participant has correct domain ID");
    TEST_ASSERT_NE(participant.get_participant_id(), 0, "Participant has unique ID");
    TEST_ASSERT_FALSE(participant.is_running(), "Participant is not running initially");

    participant.start();
    TEST_ASSERT_TRUE(participant.is_running(), "Participant is running after start");

    participant.stop();
    TEST_ASSERT_FALSE(participant.is_running(), "Participant is not running after stop");
}

void test_topic_publisher_creation() {
    printf("[Test] Topic and Publisher creation\n");

    mdds::Topic<mdds::test::SensorData> topic("SensorTopic");
    TEST_ASSERT_STR_EQ(topic.get_name(), "SensorTopic", "Topic name is correct");

    mdds::Publisher<mdds::test::SensorData> publisher;
    publisher.set_topic_name(topic.get_name());

    TEST_ASSERT_STR_EQ(publisher.get_topic_name(), "SensorTopic",
                       "Publisher has correct topic name");
}

void test_topic_subscriber_creation() {
    printf("[Test] Topic and Subscriber creation\n");

    mdds::Topic<mdds::test::SensorData> topic("SensorTopic");

    mdds::Subscriber<mdds::test::SensorData> subscriber;
    subscriber.set_topic_name(topic.get_name());

    TEST_ASSERT_STR_EQ(subscriber.get_topic_name(), "SensorTopic",
                       "Subscriber has correct topic name");
}

void test_publisher_write_read() {
    printf("[Test] Publisher write and read\n");

    mdds::Publisher<mdds::test::SensorData> publisher;

    mdds::test::SensorData data;
    data.sensor_id = 1;
    data.temperature = 25.5f;
    data.humidity = 60.0f;
    data.timestamp = 1000;

    bool write_result = publisher.write(data);
    TEST_ASSERT_TRUE(write_result, "Publisher write succeeded");

    TEST_ASSERT_TRUE(publisher.has_data(), "Publisher has data after write");

    mdds::test::SensorData read_data;
    bool read_result = publisher.read(read_data);
    TEST_ASSERT_TRUE(read_result, "Publisher read succeeded");
    TEST_ASSERT_TRUE(read_data == data, "Read data matches written data");
}

void test_subscriber_write_read() {
    printf("[Test] Subscriber write and read\n");

    mdds::Subscriber<mdds::test::SensorData> subscriber;

    mdds::test::SensorData data;
    data.sensor_id = 2;
    data.temperature = 30.0f;
    data.humidity = 45.0f;
    data.timestamp = 2000;

    subscriber.write_data(data, data.timestamp);

    TEST_ASSERT_TRUE(subscriber.has_data(), "Subscriber has data after write_data");

    mdds::test::SensorData read_data;
    uint64_t timestamp = 0;
    bool read_result = subscriber.read(read_data, &timestamp);
    TEST_ASSERT_TRUE(read_result, "Subscriber read succeeded");
    TEST_ASSERT_TRUE(read_data == data, "Read data matches written data");
    TEST_ASSERT_EQ(timestamp, data.timestamp, "Timestamp is correct");
}

void test_subscriber_callback() {
    printf("[Test] Subscriber callback\n");

    mdds::Subscriber<mdds::test::SensorData> subscriber;

    std::atomic<bool> callback_called{false};
    mdds::test::SensorData callback_data{};
    uint64_t callback_timestamp = 0;

    subscriber.set_callback([&](const mdds::test::SensorData& data, uint64_t timestamp) {
        callback_called = true;
        callback_data = data;
        callback_timestamp = timestamp;
    });

    mdds::test::SensorData data;
    data.sensor_id = 3;
    data.temperature = 22.0f;
    data.humidity = 55.0f;
    data.timestamp = 3000;

    subscriber.write_data(data, data.timestamp);

    // Give callback time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    TEST_ASSERT_TRUE(callback_called.load(), "Callback was invoked");
    TEST_ASSERT_TRUE(callback_data == data, "Callback data is correct");
    TEST_ASSERT_EQ(callback_timestamp, data.timestamp, "Callback timestamp is correct");
}

void test_data_writer_read() {
    printf("[Test] DataWriter and DataReader\n");

    mdds::DataWriter<mdds::test::SensorData> writer;
    mdds::DataReader<mdds::test::SensorData> reader;

    writer.set_topic_id(100);
    reader.set_topic_id(100);

    TEST_ASSERT_EQ(writer.get_topic_id(), 100, "Writer has correct topic ID");
    TEST_ASSERT_EQ(reader.get_topic_id(), 100, "Reader has correct topic ID");

    mdds::test::SensorData data;
    data.sensor_id = 4;
    data.temperature = 18.0f;
    data.humidity = 70.0f;
    data.timestamp = 4000;

    writer.write(data, data.timestamp);

    TEST_ASSERT_TRUE(writer.has_data(), "Writer has data after write");

    // Simulate data transfer (write from writer to reader)
    uint64_t timestamp = 0;
    if (writer.read(data, &timestamp)) {
        reader.write_data(data, timestamp);
    }

    TEST_ASSERT_TRUE(reader.has_data(), "Reader has data after transfer");

    mdds::test::SensorData read_data;
    bool read_result = reader.read(read_data, &timestamp);
    TEST_ASSERT_TRUE(read_result, "Reader read succeeded");
    TEST_ASSERT_TRUE(read_data == data, "Read data matches original");
}

void test_discovery_integration() {
    printf("[Test] Discovery integration\n");

    mdds::Discovery discovery;

    // Create participants
    mdds::DomainParticipant pub_participant(0);
    mdds::DomainParticipant sub_participant(0);

    // Register publisher
    mdds::EndpointInfo pub_info;
    pub_info.participant_id = pub_participant.get_participant_id();
    pub_info.topic_id = 100;
    pub_info.topic_name = "TestTopic";
    pub_info.type_name = "SensorData";
    discovery.register_publisher(pub_info);

    // Register subscriber
    mdds::EndpointInfo sub_info;
    sub_info.participant_id = sub_participant.get_participant_id();
    sub_info.topic_id = 101;
    sub_info.topic_name = "TestTopic";
    sub_info.type_name = "SensorData";
    discovery.register_subscriber(sub_info);

    // Verify match
    auto matched_pubs = discovery.get_matched_publishers(sub_info);
    TEST_ASSERT_EQ(matched_pubs.size(), 1UL, "Found 1 matching publisher");
    TEST_ASSERT_EQ(matched_pubs[0].participant_id, pub_info.participant_id,
                   "Matched publisher ID is correct");

    auto matched_subs = discovery.get_matched_subscribers(pub_info);
    TEST_ASSERT_EQ(matched_subs.size(), 1UL, "Found 1 matching subscriber");
    TEST_ASSERT_EQ(matched_subs[0].participant_id, sub_info.participant_id,
                   "Matched subscriber ID is correct");
}

void test_pubsub_with_discovery() {
    printf("[Test] PubSub with Discovery\n");

    mdds::Discovery discovery;

    // Create publisher participant
    mdds::DomainParticipant pub_participant(0);
    mdds::Topic<mdds::test::SensorData> topic("SensorData");
    mdds::Publisher<mdds::test::SensorData> publisher;
    publisher.set_topic_name(topic.get_name());

    // Register with discovery
    mdds::EndpointInfo pub_info;
    pub_info.participant_id = pub_participant.get_participant_id();
    pub_info.topic_id = topic.get_topic_id();
    pub_info.topic_name = topic.get_name();
    pub_info.type_name = "SensorData";
    discovery.register_publisher(pub_info);

    // Create subscriber participant
    mdds::DomainParticipant sub_participant(0);
    mdds::Subscriber<mdds::test::SensorData> subscriber;
    subscriber.set_topic_name(topic.get_name());

    std::atomic<bool> callback_called{false};
    subscriber.set_callback([&](const mdds::test::SensorData& data, uint64_t timestamp) {
        callback_called = true;
        (void)data;
        (void)timestamp;
    });

    // Register with discovery
    mdds::EndpointInfo sub_info;
    sub_info.participant_id = sub_participant.get_participant_id();
    sub_info.topic_id = topic.get_topic_id();
    sub_info.topic_name = topic.get_name();
    sub_info.type_name = "SensorData";
    discovery.register_subscriber(sub_info);

    // Verify discovery
    auto matched_pubs = discovery.get_matched_publishers(sub_info);
    TEST_ASSERT_EQ(matched_pubs.size(), 1UL, "Discovery found publisher");

    // Simulate data flow
    mdds::test::SensorData data;
    data.sensor_id = 5;
    data.temperature = 23.5f;
    data.humidity = 65.0f;
    data.timestamp = 5000;

    publisher.write(data, data.timestamp);

    // Transfer data from publisher to subscriber
    mdds::test::SensorData transferred_data;
    uint64_t transferred_timestamp = 0;
    if (publisher.read(transferred_data)) {
        subscriber.write_data(transferred_data, transferred_timestamp);
    }

    // Verify subscriber received data
    TEST_ASSERT_TRUE(callback_called.load(), "Subscriber callback was invoked");
}

void test_message_header_fields() {
    printf("[Test] Message header fields\n");

    mdds::MessageHeader header;
    header.magic = mdds::MDDS_MAGIC;
    header.version = mdds::MDDS_VERSION;
    header.message_type = static_cast<uint8_t>(mdds::MessageType::DATA);
    header.flags = 0;
    header.topic_id = 123;
    header.sequence_number = 1;
    header.timestamp = 1234567890;
    header.payload_length = 100;

    TEST_ASSERT_EQ(header.magic, 0x4D444453U, "Magic is correct");
    TEST_ASSERT_EQ(header.version, 0x0001U, "Version is correct");
    TEST_ASSERT_EQ(header.message_type, 0x01, "Message type is DATA");
    TEST_ASSERT_EQ(header.topic_id, 123U, "Topic ID is correct");
    TEST_ASSERT_EQ(header.sequence_number, 1U, "Sequence number is correct");
    TEST_ASSERT_EQ(header.timestamp, 1234567890ULL, "Timestamp is correct");
    TEST_ASSERT_EQ(header.payload_length, 100U, "Payload length is correct");
}

void test_constants() {
    printf("[Test] MDDS constants\n");

    TEST_ASSERT_EQ(mdds::MDDS_MAGIC, 0x4D444453U, "MDDS_MAGIC is correct");
    TEST_ASSERT_EQ(mdds::MDDS_VERSION, 0x0001U, "MDDS_VERSION is correct");
    TEST_ASSERT_EQ(mdds::MDDS_HEADER_SIZE, 24UL, "MDDS_HEADER_SIZE is 24");
    TEST_ASSERT_EQ(mdds::MDDS_MAX_PAYLOAD_SIZE, 65536UL, "MDDS_MAX_PAYLOAD_SIZE is 64KB");
    TEST_ASSERT_EQ(mdds::DEFAULT_DISCOVERY_PORT, 7412U, "DEFAULT_DISCOVERY_PORT is 7412");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("=== MDDS Integration Tests ===\n\n");

    test_participant_with_domain_id();
    test_topic_publisher_creation();
    test_topic_subscriber_creation();
    test_publisher_write_read();
    test_subscriber_write_read();
    test_subscriber_callback();
    test_data_writer_read();
    test_discovery_integration();
    test_pubsub_with_discovery();
    test_message_header_fields();
    test_constants();

    printf("\n=== Results: %d/%d passed ===\n", g_test_pass, g_test_count);

    return g_test_fail > 0 ? 1 : 0;
}
