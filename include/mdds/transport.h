#ifndef MDDS_TRANSPORT_H
#define MDDS_TRANSPORT_H

#include "types.h"
#include "qos.h"
#include <memory>
#include <functional>

namespace mdds {

// ========== Transport Interface ==========

class Transport {
public:
    virtual ~Transport() = default;

    // Initialize the transport
    virtual bool init(const Endpoint& local_endpoint) = 0;

    // Send data to destination
    virtual bool send(const void* data, size_t size, const Endpoint& destination) = 0;

    // Receive data (non-blocking)
    virtual bool receive(void* buffer, size_t max_size,
                        size_t* received, Endpoint* sender) = 0;

    // Get local endpoint
    virtual Endpoint get_local_endpoint() const = 0;

    // Set receive callback
    virtual void set_receive_callback(ReceiveCallback callback) = 0;

    // Check if transport is open
    virtual bool is_open() const = 0;

    // Close the transport
    virtual void close() = 0;
};

// ========== Transport Factory ==========

class TransportFactory {
public:
    static std::unique_ptr<Transport> create_udp_transport(DomainId domain_id);
    static std::unique_ptr<Transport> create_shm_transport(DomainId domain_id, TopicId topic_id);
    static std::unique_ptr<Transport> create_transport(DomainId domain_id, TopicId topic_id,
                                                        TransportType type);
};

}  // namespace mdds

#endif  // MDDS_TRANSPORT_H
