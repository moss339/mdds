/**
 * @file types.h
 * @brief MDDS Type Definitions and Constants
 *
 * This file defines the core type definitions used throughout the MDDS
 * (Minimal DDS Implementation) module.
 */

#ifndef MDDS_TYPES_H
#define MDDS_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace moss {
namespace mdds {

/**
 * @brief MDDS Type Definitions
 * @{
 */

/** @brief Domain identifier (0-255) */
using DomainId = uint8_t;

/** @brief Topic identifier */
using TopicId = uint32_t;

/** @brief Participant identifier */
using ParticipantId = uint32_t;

/** @brief Sequence number for message ordering */
using SequenceNumber = uint32_t;

/** @} */

/**
 * @brief MDDS Protocol Constants
 * @{
 */

/** @brief MDDS magic number for packet identification ('MDDS') */
constexpr uint32_t MDDS_MAGIC = 0x4D444453;

/** @brief MDDS protocol version */
constexpr uint16_t MDDS_VERSION = 0x0001;

/** @brief Size of MDDS message header in bytes */
constexpr size_t MDDS_HEADER_SIZE = 24;

/** @brief Maximum payload size (64KB) */
constexpr size_t MDDS_MAX_PAYLOAD_SIZE = 64 * 1024;

/** @brief Default discovery server port */
constexpr uint16_t DEFAULT_DISCOVERY_PORT = 7412;

/**
 * @brief MDDS Message Types
 *
 * Defines the types of messages used in the MDDS protocol.
 */
enum class MessageType : uint8_t {
    DATA              = 0x01,  /**< @brief User data message */
    DISCOVERY         = 0x02,  /**< @brief Discovery message */
    HEARTBEAT         = 0x03,  /**< @brief Heartbeat message */
    ACK               = 0x04   /**< @brief Acknowledgment message */
};

/**
 * @brief Discovery Message Types
 *
 * Defines the types of discovery messages for topic matching.
 */
enum class DiscoveryType : uint8_t {
    REGISTER_PUBLISHER   = 0x01,  /**< @brief Register as publisher */
    REGISTER_SUBSCRIBER  = 0x02,  /**< @brief Register as subscriber */
    MATCH_RESPONSE       = 0x03,  /**< @brief Match response */
    UNREGISTER           = 0x04   /**< @brief Unregister endpoint */
};

/**
 * @brief Transport Protocol Types
 */
enum class TransportType : uint8_t {
    UDP  = 0x01,  /**< @brief UDP transport */
    TCP  = 0x02,  /**< @brief TCP transport */
    SHM  = 0x03   /**< @brief Shared memory transport */
};

/**
 * @brief MDDS Message Header
 *
 * 24-byte header structure for all MDDS messages.
 * Packed for network transmission.
 */
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;           /**< @brief MDDS_MAGIC for packet identification */
    uint16_t version;         /**< @brief MDDS protocol version */
    uint8_t message_type;     /**< @brief MessageType enum value */
    uint8_t flags;            /**< @brief QoS and other flags */
    uint32_t topic_id;        /**< @brief Topic identifier */
    uint32_t sequence_number; /**< @brief Sequence number for ordering */
    uint32_t timestamp;       /**< @brief Timestamp (seconds since epoch) */
    uint32_t payload_length;  /**< @brief Length of payload in bytes */
};
#pragma pack(pop)

static_assert(sizeof(MessageHeader) == 24, "MessageHeader must be 24 bytes");

/**
 * @brief Network Endpoint Definition
 *
 * Represents a network address with port and transport type.
 */
struct Endpoint {
    std::string address_;      /**< @brief IP address or hostname */
    uint16_t port_;            /**< @brief Port number */
    TransportType type_;       /**< @brief Transport type */

    /** @brief Default constructor */
    Endpoint() : port_(0), type_(TransportType::UDP) {}

    /** @brief Construct endpoint with address, port, and type */
    Endpoint(const std::string& addr, uint16_t port, TransportType type)
        : address_(addr), port_(port), type_(type) {}

    /** @brief Equality comparison operator */
    bool operator==(const Endpoint& other) const {
        return address_ == other.address_ && port_ == other.port_;
    }

    /** @brief Convert to string representation */
    std::string to_string() const {
        return address_ + ":" + std::to_string(port_);
    }
};

/**
 * @brief Endpoint Information
 *
 * Contains complete information about a discovered endpoint including
 * participant ID, topic information, and network endpoint.
 */
struct EndpointInfo {
    ParticipantId participant_id;  /**< @brief Participant identifier */
    TopicId topic_id;               /**< @brief Topic identifier */
    std::string topic_name;         /**< @brief Topic name */
    std::string type_name;           /**< @brief Type name for serialization */
    Endpoint endpoint;              /**< @brief Network endpoint */
    uint8_t qos_flags;              /**< @brief QoS configuration flags */

    /** @brief Convert to string representation */
    std::string to_string() const {
        return "EndpointInfo[pid=" + std::to_string(participant_id) +
               ", topic=" + topic_name +
               ", endpoint=" + endpoint.to_string() + "]";
    }
};

/**
 * @brief Register Publisher Discovery Message
 *
 * Message sent by a publisher to register itself with the discovery service.
 */
struct RegisterPublisherMsg {
    ParticipantId participant_id;  /**< @brief Participant identifier */
    TopicId topic_id;               /**< @brief Topic identifier */
    std::string topic_name;         /**< @brief Topic name */
    std::string type_name;          /**< @brief Type name for serialization */
    uint8_t qos_flags;              /**< @brief QoS configuration flags */
};

/**
 * @brief Register Subscriber Discovery Message
 *
 * Message sent by a subscriber to register itself with the discovery service.
 */
struct RegisterSubscriberMsg {
    ParticipantId participant_id;  /**< @brief Participant identifier */
    TopicId topic_id;               /**< @brief Topic identifier */
    std::string topic_name;         /**< @brief Topic name */
    std::string type_name;           /**< @brief Type name for serialization */
    uint8_t qos_flags;              /**< @brief QoS configuration flags */
};

/**
 * @brief Unregister Discovery Message
 *
 * Message sent when an endpoint wants to unregister from discovery.
 */
struct UnregisterMsg {
    ParticipantId participant_id;  /**< @brief Participant identifier */
    TopicId topic_id;               /**< @brief Topic identifier */
    std::string topic_name;         /**< @brief Topic name */
};

/**
 * @brief Match Response Discovery Message
 *
 * Response from discovery service containing matched endpoints.
 */
struct MatchResponseMsg {
    TopicId topic_id;                            /**< @brief Topic identifier */
    std::string topic_name;                      /**< @brief Topic name */
    std::vector<EndpointInfo> publishers;         /**< @brief List of matching publishers */
    uint8_t result;                               /**< @brief Result code (0=success, 1=waiting, 2=failed) */
};

/**
 * @brief Receive Callback Function Type
 *
 * Callback type for receiving data from transport layer.
 *
 * @param data Pointer to received data
 * @param size Size of received data in bytes
 * @param sender Endpoint information of the sender
 */
using ReceiveCallback = std::function<void(const void* data, size_t size, const Endpoint& sender)>;

}  // namespace mdds
}  // namespace moss

#endif  // MDDS_TYPES_H
