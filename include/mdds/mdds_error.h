#ifndef MDDS_ERROR_H
#define MDDS_ERROR_H

#include <cstdint>

namespace mdds {

// MDDS Error Codes
enum class MddsError : int {
    OK = 0,

    // General errors (1-99)
    INVALID_PARAM      = 1,
    NOT_FOUND          = 2,
    ALREADY_EXISTS     = 3,
    NO_MEMORY          = 4,
    TIMEOUT            = 5,
    NOT_INITIALIZED    = 6,
    ALREADY_STARTED    = 7,
    NOT_STARTED        = 8,

    // Discovery related errors (100-199)
    DISCOVERY_TIMEOUT  = 100,
    TOPIC_MISMATCH     = 101,
    NO_PUBLISHER       = 102,
    NO_SUBSCRIBER      = 103,
    MATCH_FAILED       = 104,

    // Transport related errors (200-299)
    SEND_FAILED        = 200,
    RECEIVE_FAILED     = 201,
    CONNECTION_LOST    = 202,

    // Data related errors (300-399)
    SERIALIZE_FAILED   = 300,
    DESERIALIZE_FAILED = 301,
    BUFFER_OVERFLOW    = 302,
};

// Error code to string conversion
inline const char* error_to_string(MddsError err) {
    switch (err) {
        case MddsError::OK:              return "OK";
        case MddsError::INVALID_PARAM:   return "Invalid parameter";
        case MddsError::NOT_FOUND:        return "Not found";
        case MddsError::ALREADY_EXISTS:   return "Already exists";
        case MddsError::NO_MEMORY:        return "Out of memory";
        case MddsError::TIMEOUT:          return "Operation timeout";
        case MddsError::NOT_INITIALIZED:  return "Not initialized";
        case MddsError::ALREADY_STARTED:  return "Already started";
        case MddsError::NOT_STARTED:      return "Not started";
        case MddsError::DISCOVERY_TIMEOUT:return "Discovery timeout";
        case MddsError::TOPIC_MISMATCH:   return "Topic mismatch";
        case MddsError::NO_PUBLISHER:     return "No publisher found";
        case MddsError::NO_SUBSCRIBER:    return "No subscriber found";
        case MddsError::MATCH_FAILED:     return "Match failed";
        case MddsError::SEND_FAILED:      return "Send failed";
        case MddsError::RECEIVE_FAILED:   return "Receive failed";
        case MddsError::CONNECTION_LOST:  return "Connection lost";
        case MddsError::SERIALIZE_FAILED: return "Serialization failed";
        case MddsError::DESERIALIZE_FAILED: return "Deserialization failed";
        case MddsError::BUFFER_OVERFLOW: return "Buffer overflow";
        default:                          return "Unknown error";
    }
};

}  // namespace mdds

#endif  // MDDS_ERROR_H
