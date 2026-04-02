/**
 * @file qos.h
 * @brief MDDS Quality of Service (QoS) Configuration
 */

#ifndef MDDS_QOS_H
#define MDDS_QOS_H

#include "types.h"
#include <cstdint>

namespace moss {
namespace mdds {

/**
 * @brief QoS Flags for reliability and durability
 * @{
 */

/**
 * @brief Reliability QoS flags
 */
enum class QoSFlags : uint8_t {
    BEST_EFFORT      = 0x01,  /**< @brief Best effort delivery (no guaranteed ordering) */
    RELIABLE         = 0x02,  /**< @brief Reliable delivery (guaranteed ordering) */
};

/**
 * @brief Durability QoS flags
 */
enum class QoSDurabilityFlags : uint8_t {
    VOLATILE         = 0x10,   /**< @brief Volatile (no persistence, samples not stored) */
    TRANSIENT_LOCAL  = 0x20,   /**< @brief Transient local (samples stored for late joiners) */
};

/** @} */

/**
 * @brief QoS Configuration Structure
 *
 * Defines the Quality of Service settings for publishers and subscribers.
 * QoS settings control reliability, durability, and other communication aspects.
 */
struct QoSConfig {
    QoSFlags reliability{QoSFlags::BEST_EFFORT};     /**< @brief Reliability setting */
    QoSFlags durability{QoSFlags::VOLATILE};          /**< @brief Durability setting */

    /** @brief Default constructor */
    QoSConfig() = default;

    /** @brief Construct QoSConfig with reliability and durability flags */
    constexpr QoSConfig(QoSFlags rel, QoSFlags dur)
        : reliability(rel), durability(dur) {}

    /**
     * @brief Check QoS compatibility with another configuration
     * @param other The other QoS configuration to check
     * @return true if compatible, false otherwise
     *
     * TRANSIENT_LOCAL Publisher can send to VOLATILE Subscriber.
     */
    bool is_compatible(const QoSConfig& other) const {
        if (durability == QoSFlags::TRANSIENT_LOCAL &&
            other.durability == QoSFlags::VOLATILE) {
            return true;
        }
        return durability == other.durability;
    }

    /**
     * @brief Convert QoS settings to flags byte
     * @return Packed flags byte
     */
    uint8_t to_flags() const {
        return static_cast<uint8_t>(reliability) | static_cast<uint8_t>(durability);
    }

    /**
     * @brief Create QoSConfig from flags byte
     * @param flags Packed flags byte
     * @return QoSConfig structure
     */
    static QoSConfig from_flags(uint8_t flags) {
        QoSConfig config;
        config.reliability = (flags & 0x0F) == static_cast<uint8_t>(QoSFlags::RELIABLE)
                             ? QoSFlags::RELIABLE : QoSFlags::BEST_EFFORT;
        config.durability = (flags & 0xF0) == static_cast<uint8_t>(QoSFlags::TRANSIENT_LOCAL)
                            ? QoSFlags::TRANSIENT_LOCAL : QoSFlags::VOLATILE;
        return config;
    }
};

/**
 * @brief Default QoS Configurations
 * @{
 */
namespace default_qos {
    /**
     * @brief Default QoS for publishers
     * @return QoSConfig with BEST_EFFORT reliability and VOLATILE durability
     */
    constexpr QoSConfig publisher() {
        return QoSConfig{QoSFlags::BEST_EFFORT, QoSFlags::VOLATILE};
    }

    /**
     * @brief Default QoS for subscribers
     * @return QoSConfig with BEST_EFFORT reliability and VOLATILE durability
     */
    constexpr QoSConfig subscriber() {
        return QoSConfig{QoSFlags::BEST_EFFORT, QoSFlags::VOLATILE};
    }
}
/** @} */

}  // namespace mdds

}  // namespace moss
#endif  // MDDS_QOS_H
