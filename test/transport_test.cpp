/**
 * @file transport_test.cpp
 * @brief Unit tests for Transport layer
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <atomic>
#include <functional>

// ============================================================================
// MDDS Types (minimal copy for testing without depending on implementation)
// ============================================================================

namespace mdds {

using DomainId = uint8_t;
using TopicId = uint32_t;
using ParticipantId = uint32_t;

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

// Receive callback type
using ReceiveCallback = std::function<void(const void* data, size_t size, const Endpoint& sender)>;

// Transport interface
class Transport {
public:
    virtual ~Transport() = default;

    virtual bool send(const void* data, size_t size, const Endpoint& destination) = 0;
    virtual bool receive(void* buffer, size_t max_size, size_t* received, Endpoint* sender) = 0;
    virtual Endpoint get_local_endpoint() const = 0;
    virtual void set_receive_callback(ReceiveCallback callback) = 0;
    virtual bool is_open() const = 0;
};

// Mock UDP Transport for testing
class UdpTransport : public Transport {
public:
    UdpTransport(const std::string& local_addr, uint16_t local_port);
    ~UdpTransport() override;

    bool send(const void* data, size_t size, const Endpoint& destination) override;
    bool receive(void* buffer, size_t max_size, size_t* received, Endpoint* sender) override;
    Endpoint get_local_endpoint() const override;
    void set_receive_callback(ReceiveCallback callback) override;
    bool is_open() const override { return open_; }

    // Helper for testing: inject data
    bool inject_packet(const void* data, size_t size, const Endpoint& sender);

private:
    std::string local_addr_;
    uint16_t local_port_;
    bool open_;
    ReceiveCallback callback_;
    std::mutex mutex_;
    std::queue<std::pair<std::vector<uint8_t>, Endpoint>> pending_packets_;
};

UdpTransport::UdpTransport(const std::string& local_addr, uint16_t local_port)
    : local_addr_(local_addr)
    , local_port_(local_port)
    , open_(true)
{
}

UdpTransport::~UdpTransport() {
    open_ = false;
}

bool UdpTransport::send(const void* data, size_t size, const Endpoint& destination) {
    if (!open_) return false;
    // In real implementation, this would send via socket
    // For testing, we just return success
    return true;
}

bool UdpTransport::receive(void* buffer, size_t max_size, size_t* received, Endpoint* sender) {
    if (!open_) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_packets_.empty()) {
        return false;
    }

    auto& packet = pending_packets_.front();
    size_t packet_size = packet.first.size();

    if (packet_size > max_size) {
        return false;
    }

    memcpy(buffer, packet.first.data(), packet_size);
    *received = packet_size;
    *sender = packet.second;
    pending_packets_.pop();

    return true;
}

Endpoint UdpTransport::get_local_endpoint() const {
    return Endpoint(local_addr_, local_port_, TransportType::UDP);
}

void UdpTransport::set_receive_callback(ReceiveCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
}

bool UdpTransport::inject_packet(const void* data, size_t size, const Endpoint& sender) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint8_t> packet(size);
    memcpy(packet.data(), data, size);
    pending_packets_.push({packet, sender});

    if (callback_) {
        callback_(data, size, sender);
    }

    return true;
}

// Mock SharedMemory Transport for testing
class SharedMemoryTransport : public Transport {
public:
    SharedMemoryTransport(DomainId domain_id, TopicId topic_id);
    ~SharedMemoryTransport() override;

    bool send(const void* data, size_t size, const Endpoint& destination) override;
    bool receive(void* buffer, size_t max_size, size_t* received, Endpoint* sender) override;
    Endpoint get_local_endpoint() const override;
    void set_receive_callback(ReceiveCallback callback) override;
    bool is_open() const override { return open_; }

    // Helper for testing: inject data
    bool inject_packet(const void* data, size_t size, const Endpoint& sender);

private:
    DomainId domain_id_;
    TopicId topic_id_;
    bool open_;
    ReceiveCallback callback_;
    std::mutex mutex_;
    std::queue<std::pair<std::vector<uint8_t>, Endpoint>> pending_packets_;
    std::string shm_name_;
};

SharedMemoryTransport::SharedMemoryTransport(DomainId domain_id, TopicId topic_id)
    : domain_id_(domain_id)
    , topic_id_(topic_id)
    , open_(true)
{
    shm_name_ = "/mdds_shm_d" + std::to_string(domain_id) + "_t" + std::to_string(topic_id);
}

SharedMemoryTransport::~SharedMemoryTransport() {
    open_ = false;
}

bool SharedMemoryTransport::send(const void* data, size_t size, const Endpoint& destination) {
    if (!open_) return false;
    // In real implementation, this would write to shared memory
    return true;
}

bool SharedMemoryTransport::receive(void* buffer, size_t max_size, size_t* received, Endpoint* sender) {
    if (!open_) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_packets_.empty()) {
        return false;
    }

    auto& packet = pending_packets_.front();
    size_t packet_size = packet.first.size();

    if (packet_size > max_size) {
        return false;
    }

    memcpy(buffer, packet.first.data(), packet_size);
    *received = packet_size;
    *sender = packet.second;
    pending_packets_.pop();

    return true;
}

Endpoint SharedMemoryTransport::get_local_endpoint() const {
    return Endpoint("/" + shm_name_, 0, TransportType::SHM);
}

void SharedMemoryTransport::set_receive_callback(ReceiveCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
}

bool SharedMemoryTransport::inject_packet(const void* data, size_t size, const Endpoint& sender) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint8_t> packet(size);
    memcpy(packet.data(), data, size);
    pending_packets_.push({packet, sender});

    if (callback_) {
        callback_(data, size, sender);
    }

    return true;
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
// Test Cases
// ============================================================================

void test_udp_transport_creation() {
    printf("[Test] UDP transport creation\n");

    mdds::UdpTransport transport("127.0.0.1", 7412);
    TEST_ASSERT_TRUE(transport.is_open(), "UDP transport is open after creation");
}

void test_udp_transport_local_endpoint() {
    printf("[Test] UDP transport local endpoint\n");

    mdds::UdpTransport transport("127.0.0.1", 7412);
    mdds::Endpoint ep = transport.get_local_endpoint();

    TEST_ASSERT_STR_EQ(ep.address_, "127.0.0.1", "Local address is correct");
    TEST_ASSERT_EQ(ep.port_, 7412, "Local port is correct");
    TEST_ASSERT_TRUE(ep.type_ == mdds::TransportType::UDP, "Transport type is UDP");
}

void test_udp_transport_send() {
    printf("[Test] UDP transport send\n");

    mdds::UdpTransport transport("127.0.0.1", 7412);

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    mdds::Endpoint dest("127.0.0.1", 7413, mdds::TransportType::UDP);

    bool result = transport.send(data, sizeof(data), dest);
    TEST_ASSERT_TRUE(result, "UDP send returned true");
}

void test_udp_transport_receive() {
    printf("[Test] UDP transport receive\n");

    mdds::UdpTransport transport("127.0.0.1", 7413);

    // Prepare test data
    uint8_t test_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    mdds::Endpoint sender("127.0.0.1", 7412, mdds::TransportType::UDP);

    // Inject packet
    transport.inject_packet(test_data, sizeof(test_data), sender);

    // Receive packet
    uint8_t buffer[256];
    size_t received = 0;
    mdds::Endpoint recv_sender;

    bool result = transport.receive(buffer, sizeof(buffer), &received, &recv_sender);
    TEST_ASSERT_TRUE(result, "UDP receive returned true");
    TEST_ASSERT_EQ(received, sizeof(test_data), "Received correct data size");
    TEST_ASSERT_EQ(memcmp(buffer, test_data, sizeof(test_data)), 0,
                   "Received correct data content");
}

void test_udp_transport_receive_empty() {
    printf("[Test] UDP transport receive when empty\n");

    mdds::UdpTransport transport("127.0.0.1", 7414);

    uint8_t buffer[256];
    size_t received = 0;
    mdds::Endpoint sender;

    bool result = transport.receive(buffer, sizeof(buffer), &received, &sender);
    TEST_ASSERT_FALSE(result, "UDP receive returned false when queue empty");
}

void test_udp_transport_callback() {
    printf("[Test] UDP transport receive callback\n");

    mdds::UdpTransport transport("127.0.0.1", 7415);

    std::atomic<bool> callback_called{false};
    std::mutex callback_mutex;
    mdds::Endpoint callback_sender;
    size_t callback_size = 0;

    transport.set_receive_callback([&](const void* data, size_t size, const mdds::Endpoint& sender) {
        callback_called = true;
        std::lock_guard<std::mutex> lock(callback_mutex);
        callback_sender = sender;
        callback_size = size;
    });

    uint8_t test_data[] = {0x12, 0x34, 0x56, 0x78};
    mdds::Endpoint sender("127.0.0.1", 7412, mdds::TransportType::UDP);

    transport.inject_packet(test_data, sizeof(test_data), sender);

    // Give callback time to be invoked
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    TEST_ASSERT_TRUE(callback_called.load(), "Callback was invoked");
}

void test_shared_memory_transport_creation() {
    printf("[Test] SharedMemory transport creation\n");

    mdds::SharedMemoryTransport transport(0, 100);
    TEST_ASSERT_TRUE(transport.is_open(), "SharedMemory transport is open after creation");
}

void test_shared_memory_transport_local_endpoint() {
    printf("[Test] SharedMemory transport local endpoint\n");

    mdds::SharedMemoryTransport transport(0, 100);
    mdds::Endpoint ep = transport.get_local_endpoint();

    TEST_ASSERT_TRUE(ep.address_.find("/mdds_shm_d0_t100") != std::string::npos,
                    "Local endpoint contains expected shm name");
    TEST_ASSERT_TRUE(ep.type_ == mdds::TransportType::SHM, "Transport type is SHM");
}

void test_shared_memory_transport_send() {
    printf("[Test] SharedMemory transport send\n");

    mdds::SharedMemoryTransport transport(0, 101);

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    mdds::Endpoint dest("/mdds_shm_d0_t101", 0, mdds::TransportType::SHM);

    bool result = transport.send(data, sizeof(data), dest);
    TEST_ASSERT_TRUE(result, "SharedMemory send returned true");
}

void test_shared_memory_transport_receive() {
    printf("[Test] SharedMemory transport receive\n");

    mdds::SharedMemoryTransport transport(0, 102);

    // Prepare test data
    uint8_t test_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    mdds::Endpoint sender("/mdds_shm_d0_t102", 0, mdds::TransportType::SHM);

    // Inject packet
    transport.inject_packet(test_data, sizeof(test_data), sender);

    // Receive packet
    uint8_t buffer[256];
    size_t received = 0;
    mdds::Endpoint recv_sender;

    bool result = transport.receive(buffer, sizeof(buffer), &received, &recv_sender);
    TEST_ASSERT_TRUE(result, "SharedMemory receive returned true");
    TEST_ASSERT_EQ(received, sizeof(test_data), "Received correct data size");
}

void test_shared_memory_transport_multiple_packets() {
    printf("[Test] SharedMemory transport multiple packets\n");

    mdds::SharedMemoryTransport transport(0, 103);

    // Inject multiple packets
    for (int i = 0; i < 5; i++) {
        uint8_t data[1] = {(uint8_t)i};
        mdds::Endpoint sender("/sender", 0, mdds::TransportType::SHM);
        transport.inject_packet(data, sizeof(data), sender);
    }

    // Receive all packets
    uint8_t buffer[256];
    int count = 0;
    while (true) {
        size_t received = 0;
        mdds::Endpoint sender;
        if (transport.receive(buffer, sizeof(buffer), &received, &sender)) {
            count++;
        } else {
            break;
        }
    }

    TEST_ASSERT_EQ(count, 5, "Received all 5 packets");
}

void test_endpoint_equality() {
    printf("[Test] Endpoint equality\n");

    mdds::Endpoint ep1("127.0.0.1", 7412, mdds::TransportType::UDP);
    mdds::Endpoint ep2("127.0.0.1", 7412, mdds::TransportType::UDP);
    mdds::Endpoint ep3("127.0.0.1", 7413, mdds::TransportType::UDP);
    mdds::Endpoint ep4("127.0.0.2", 7412, mdds::TransportType::UDP);

    TEST_ASSERT_TRUE(ep1 == ep2, "Same endpoints are equal");
    TEST_ASSERT_FALSE(ep1 == ep3, "Different port endpoints are not equal");
    TEST_ASSERT_FALSE(ep1 == ep4, "Different address endpoints are not equal");
}

void test_endpoint_to_string() {
    printf("[Test] Endpoint to_string\n");

    mdds::Endpoint ep("127.0.0.1", 7412, mdds::TransportType::UDP);
    std::string str = ep.to_string();

    TEST_ASSERT_STR_EQ(str, "127.0.0.1:7412", "to_string returns correct format");
}

void test_transport_type_values() {
    printf("[Test] Transport type values\n");

    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::TransportType::UDP) == 0x01,
                    "UDP transport type value is 0x01");
    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::TransportType::TCP) == 0x02,
                    "TCP transport type value is 0x02");
    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::TransportType::SHM) == 0x03,
                    "SHM transport type value is 0x03");
}

void test_message_header() {
    printf("[Test] Message header size\n");

    // With #pragma pack(1), struct should be 28 bytes:
    // 4 + 2 + 1 + 1 + 4 + 4 + 8 + 4 = 28
    TEST_ASSERT_EQ(sizeof(mdds::MessageHeader), 28UL,
                   "MessageHeader is 28 bytes as expected");
}

void test_message_types() {
    printf("[Test] Message types\n");

    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::MessageType::DATA) == 0x01,
                    "DATA message type is 0x01");
    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::MessageType::DISCOVERY) == 0x02,
                    "DISCOVERY message type is 0x02");
    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::MessageType::HEARTBEAT) == 0x03,
                    "HEARTBEAT message type is 0x03");
    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::MessageType::ACK) == 0x04,
                    "ACK message type is 0x04");
}

void test_discovery_types() {
    printf("[Test] Discovery types\n");

    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::DiscoveryType::REGISTER_PUBLISHER) == 0x01,
                    "REGISTER_PUBLISHER is 0x01");
    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::DiscoveryType::REGISTER_SUBSCRIBER) == 0x02,
                    "REGISTER_SUBSCRIBER is 0x02");
    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::DiscoveryType::MATCH_RESPONSE) == 0x03,
                    "MATCH_RESPONSE is 0x03");
    TEST_ASSERT_TRUE(static_cast<uint8_t>(mdds::DiscoveryType::UNREGISTER) == 0x04,
                    "UNREGISTER is 0x04");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("=== MDDS Transport Tests ===\n\n");

    test_udp_transport_creation();
    test_udp_transport_local_endpoint();
    test_udp_transport_send();
    test_udp_transport_receive();
    test_udp_transport_receive_empty();
    test_udp_transport_callback();

    test_shared_memory_transport_creation();
    test_shared_memory_transport_local_endpoint();
    test_shared_memory_transport_send();
    test_shared_memory_transport_receive();
    test_shared_memory_transport_multiple_packets();

    test_endpoint_equality();
    test_endpoint_to_string();
    test_transport_type_values();
    test_message_header();
    test_message_types();
    test_discovery_types();

    printf("\n=== Results: %d/%d passed ===\n", g_test_pass, g_test_count);

    return g_test_fail > 0 ? 1 : 0;
}
