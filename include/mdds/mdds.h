/**
 * @file mdds.h
 * @brief MDDS - Minimal DDS Implementation
 *
 * MDDS (Minimal Data Distribution Service) is a lightweight publish-subscribe
 * middleware for automotive and embedded systems. It provides:
 * - Domain Participant management
 * - Publisher/Subscriber creation
 * - Topic registration with discovery
 * - Raw data writer/reader for protobuf integration
 *
 * @note This module is designed to work with mcom which handles protobuf
 * serialization. MDDS only handles raw byte transmission.
 */

/**
 * @mainpage MDDS API Documentation
 *
 * @section overview Overview
 *
 * MDDS is a lightweight DDS (Data Distribution Service) implementation
 * designed for resource-constrained embedded systems. It provides:
 * - Topic-based publish/subscribe communication
 * - Automatic service discovery
 * - UDP transport for real-time data
 * - Support for raw byte transmission (compatible with protobuf)
 *
 * @section architecture Architecture
 *
 * @dot
 * digraph MDDSArchitecture {
 *   rankdir=TB;
 *   DomainParticipant -> Publisher;
 *   DomainParticipant -> Subscriber;
 *   DomainParticipant -> Discovery;
 *   DomainParticipant -> Transport;
 *   Publisher -> DataWriter;
 *   Subscriber -> DataReader;
 *   DataWriter -> Transport;
 *   DataReader -> Transport;
 *   Discovery -> DiscoveryServer;
 * }
 * @enddot
 *
 * @section usage Usage Example
 *
 * @code
 * // Create domain participant
 * auto participant = moss::mdds::DomainParticipant::create(0);
 *
 * // Create publisher
 * auto publisher = participant->create_publisher<MyData>("my_topic");
 *
 * // Create subscriber with callback
 * auto subscriber = participant->create_subscriber<MyData>("my_topic",
 *     [](const MyData& data, uint64_t timestamp) {
 *         // Handle received data
 *     });
 * @endcode
 */

#ifndef MDDS_H
#define MDDS_H

#include "mdds_error.h"
#include "types.h"
#include "qos.h"
#include "transport.h"
#include "discovery.h"
#include "discovery_server.h"
#include "domain_participant.h"
#include "topic.h"
#include "publisher.h"
#include "subscriber.h"
#include "data_writer.h"
#include "data_reader.h"

namespace moss {
namespace mdds {

/**
 * @brief MDDS Version Information
 * @{
 */

/** @brief Get major version number */
constexpr uint32_t get_version_major() { return 1; }

/** @brief Get minor version number */
constexpr uint32_t get_version_minor() { return 0; }

/** @brief Get patch version number */
constexpr uint32_t get_version_patch() { return 0; }

/** @} */

}  // namespace mdds

}  // namespace moss
#endif  // MDDS_H
