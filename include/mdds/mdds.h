#ifndef MDDS_H
#define MDDS_H

// MDDS - Minimal DDS Implementation

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

namespace mdds {

// ========== Version Info ==========

constexpr uint32_t get_version_major() { return 1; }
constexpr uint32_t get_version_minor() { return 0; }
constexpr uint32_t get_version_patch() { return 0; }

}  // namespace mdds

#endif  // MDDS_H
