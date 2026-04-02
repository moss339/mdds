#ifndef MDDS_QOS_H
#define MDDS_QOS_H

#include "types.h"
#include <cstdint>

namespace moss {
namespace mdds {

// ========== QoS Flags ==========

enum class QoSFlags : uint8_t {
    BEST_EFFORT      = 0x01,  // Best effort delivery
    RELIABLE         = 0x02,   // Reliable delivery
    VOLATILE         = 0x10,   // Volatile (no persistence)
    TRANSIENT_LOCAL  = 0x20,   // Transient local durability
};

// ========== QoS Config ==========

struct QoSConfig {
    QoSFlags reliability{QoSFlags::BEST_EFFORT};
    QoSFlags durability{QoSFlags::VOLATILE};

    QoSConfig() = default;

    constexpr QoSConfig(QoSFlags rel, QoSFlags dur)
        : reliability(rel), durability(dur) {}

    // Simplified QoS compatibility check
    bool is_compatible(const QoSConfig& other) const {
        // TRANSIENT_LOCAL Publisher can send to VOLATILE Subscriber
        if (durability == QoSFlags::TRANSIENT_LOCAL &&
            other.durability == QoSFlags::VOLATILE) {
            return true;
        }
        return durability == other.durability;
    }

    uint8_t to_flags() const {
        return static_cast<uint8_t>(reliability) | static_cast<uint8_t>(durability);
    }

    static QoSConfig from_flags(uint8_t flags) {
        QoSConfig config;
        config.reliability = (flags & 0x0F) == static_cast<uint8_t>(QoSFlags::RELIABLE)
                             ? QoSFlags::RELIABLE : QoSFlags::BEST_EFFORT;
        config.durability = (flags & 0xF0) == static_cast<uint8_t>(QoSFlags::TRANSIENT_LOCAL)
                            ? QoSFlags::TRANSIENT_LOCAL : QoSFlags::VOLATILE;
        return config;
    }
};

// ========== Default QoS ==========

namespace default_qos {
    constexpr QoSConfig publisher() {
        return QoSConfig{QoSFlags::BEST_EFFORT, QoSFlags::VOLATILE};
    }

    constexpr QoSConfig subscriber() {
        return QoSConfig{QoSFlags::BEST_EFFORT, QoSFlags::VOLATILE};
    }
}

}  // namespace mdds

}  // namespace moss
#endif  // MDDS_QOS_H
