#ifndef MDDS_TYPES_H
#define MDDS_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace moss {
namespace mdds {

// ========== Type Definitions ==========

using DomainId = uint8_t;
using TopicId = uint32_t;
using ParticipantId = uint32_t;
using SequenceNumber = uint32_t;

// ========== Constants ==========

constexpr uint32_t MDDS_MAGIC = 0x4D444453;  // 'MDDS'
constexpr uint16_t MDDS_VERSION = 0x0001;
constexpr size_t MDDS_HEADER_SIZE = 24;
constexpr size_t MDDS_MAX_PAYLOAD_SIZE = 64 * 1024;  // 64KB
constexpr uint16_t DEFAULT_DISCOVERY_PORT = 7412;

// ========== Message Types ==========

enum class MessageType : uint8_t {
    DATA              = 0x01,
    DISCOVERY         = 0x02,
    HEARTBEAT         = 0x03,
    ACK               = 0x04
};

enum class DiscoveryType : uint8_t {
    REGISTER_PUBLISHER   = 0x01,
    REGISTER_SUBSCRIBER  = 0x02,
    MATCH_RESPONSE       = 0x03,
    UNREGISTER           = 0x04
};

enum class TransportType : uint8_t {
    UDP  = 0x01,
    TCP  = 0x02,
    SHM  = 0x03
};

// ========== Message Header ==========

#pragma pack(push, 1)

struct MessageHeader {
    uint32_t magic;           // MDDS_MAGIC
    uint16_t version;         // MDDS_VERSION
    uint8_t message_type;     // MessageType
    uint8_t flags;            // Flags
    uint32_t topic_id;        // Topic ID
    uint32_t sequence_number; // Sequence number
    uint32_t timestamp;       // Timestamp (4 bytes for compactness)
    uint32_t payload_length;  // Payload length
};

#pragma pack(pop)

static_assert(sizeof(MessageHeader) == 24, "MessageHeader must be 24 bytes");

// ========== Endpoint ==========

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

// ========== EndpointInfo ==========

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

// ========== Discovery Messages ==========

struct RegisterPublisherMsg {
    ParticipantId participant_id;
    TopicId topic_id;
    std::string topic_name;
    std::string type_name;
    uint8_t qos_flags;
};

struct RegisterSubscriberMsg {
    ParticipantId participant_id;
    TopicId topic_id;
    std::string topic_name;
    std::string type_name;
    uint8_t qos_flags;
};

struct UnregisterMsg {
    ParticipantId participant_id;
    TopicId topic_id;
    std::string topic_name;
};

struct MatchResponseMsg {
    TopicId topic_id;
    std::string topic_name;
    std::vector<EndpointInfo> publishers;
    uint8_t result;  // 0=success, 1=waiting, 2=failed
};

// ========== Receive Callback ==========

using ReceiveCallback = std::function<void(const void* data, size_t size, const Endpoint& sender)>;

}  // namespace mdds
}  // namespace moss

#endif  // MDDS_TYPES_H
