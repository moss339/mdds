#include "transport.h"
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <errno.h>

// Include shm module
extern "C" {
#include "shm/shm_api.h"
#include "shm/shm_errors.h"
#include "shm/shm_types.h"
}

namespace mdds {

// ========== SHM Transport Implementation ==========

class ShmTransport : public Transport {
public:
    ShmTransport(DomainId domain_id, TopicId topic_id)
        : domain_id_(domain_id)
        , topic_id_(topic_id)
        , is_server_(false)
        , is_open_(false)
        , shm_handle_(nullptr)
        , local_notify_fd_(-1) {
    }

    ~ShmTransport() override {
        close();
    }

    bool init(const Endpoint& local_endpoint) override {
        (void)local_endpoint;  // Local endpoint not used for SHM

        // Generate SHM name based on domain and topic
        char shm_name[64];
        snprintf(shm_name, sizeof(shm_name), "/mdds_d%u_t%u",
                 static_cast<unsigned>(domain_id_),
                 static_cast<unsigned>(topic_id_));

        // Check if SHM already exists
        if (shm_exists(shm_name)) {
            // Join as client (subscriber)
            shm_error_t err = shm_join(shm_name,
                                        static_cast<shm_permission_t>(SHM_PERM_READ | SHM_PERM_WRITE),
                                        &shm_handle_);
            if (err != SHM_OK) {
                return false;
            }
            is_server_ = false;
        } else {
            // Create as server (publisher)
            size_t data_size = MDDS_MAX_PAYLOAD_SIZE + sizeof(MessageHeader);
            shm_error_t err = shm_create(shm_name,
                                         data_size,
                                         static_cast<shm_permission_t>(SHM_PERM_READ | SHM_PERM_WRITE),
                                         SHM_FLAG_CREATE,
                                         &shm_handle_);
            if (err != SHM_OK) {
                return false;
            }
            is_server_ = true;
        }

        // Get notification fd for polling
        // Note: shm_handle_ is shm_handle_t* (struct shm_handle_impl**)
        // But shm_get_notify_fd expects shm_handle_t (struct shm_handle_impl*)
        // So we need to dereference
        local_notify_fd_ = shm_get_notify_fd(*shm_handle_);

        is_open_ = true;
        return true;
    }

    bool send(const void* data, size_t size, const Endpoint& destination) override {
        (void)destination;  // Not used - SHM is broadcast-like via shared memory

        if (!is_open_ || !shm_handle_) {
            return false;
        }

        // Get data pointer from shared memory
        // Dereference shm_handle_ since functions expect shm_handle_t (single pointer)
        void* shm_data = shm_get_data_ptr(*shm_handle_);
        if (!shm_data) {
            return false;
        }

        // Lock shared memory for writing
        shm_error_t err = shm_lock(*shm_handle_, 1000);
        if (err != SHM_OK && err != SHM_ERR_LOCK_RECOVERED) {
            return false;
        }

        // Copy data to shared memory
        size_t shm_data_size = shm_get_data_size(*shm_handle_);
        if (size > shm_data_size) {
            shm_unlock(*shm_handle_);
            return false;
        }

        std::memcpy(shm_data, data, size);

        // Unlock
        shm_unlock(*shm_handle_);

        // Notify consumers
        err = shm_notify(*shm_handle_);
        if (err != SHM_OK) {
            return false;
        }

        return true;
    }

    bool receive(void* buffer, size_t max_size,
                size_t* received, Endpoint* sender) override {
        (void)sender;  // Not applicable for SHM - sender identified by topic

        if (!is_open_ || !shm_handle_) {
            return false;
        }

        // Poll for notification with timeout 0 (non-blocking)
        struct pollfd pfd = {
            .fd = local_notify_fd_,
            .events = POLLIN,
            .revents = 0
        };

        int ret = poll(&pfd, 1, 0);
        if (ret <= 0) {
            *received = 0;
            return true;  // No data available
        }

        // Consume the notification
        shm_consume_notify(*shm_handle_);

        // Lock shared memory for reading
        shm_error_t err = shm_lock(*shm_handle_, 1000);
        if (err != SHM_OK && err != SHM_ERR_LOCK_RECOVERED) {
            *received = 0;
            return false;
        }

        // Read data from shared memory
        const void* shm_data = shm_get_data_ptr_const(*shm_handle_);
        if (!shm_data) {
            shm_unlock(*shm_handle_);
            *received = 0;
            return false;
        }

        size_t data_size = shm_get_data_size(*shm_handle_);
        if (data_size > max_size) {
            data_size = max_size;
        }

        std::memcpy(buffer, shm_data, data_size);
        *received = data_size;

        // Unlock
        shm_unlock(*shm_handle_);

        return true;
    }

    Endpoint get_local_endpoint() const override {
        // For SHM, we return an empty endpoint as SHM is not addressable by IP
        return Endpoint("0.0.0.0", 0, TransportType::SHM);
    }

    void set_receive_callback(ReceiveCallback callback) override {
        callback_ = std::move(callback);
    }

    bool is_open() const override {
        return is_open_;
    }

    void close() override {
        if (shm_handle_) {
            shm_close(&shm_handle_);
            shm_handle_ = nullptr;
        }

        if (local_notify_fd_ >= 0) {
            ::close(local_notify_fd_);
            local_notify_fd_ = -1;
        }

        is_open_ = false;
    }

    // Internal: get notify fd for integration with external event loops
    int get_notify_fd() const { return local_notify_fd_; }

private:
    DomainId domain_id_;
    TopicId topic_id_;
    bool is_server_;
    bool is_open_;
    shm_handle_t* shm_handle_;  // This is struct shm_handle_impl** (double pointer)
    int local_notify_fd_;
    ReceiveCallback callback_;
};

// ========== Transport Factory ==========

std::unique_ptr<Transport> TransportFactory::create_shm_transport(DomainId domain_id, TopicId topic_id) {
    return std::make_unique<ShmTransport>(domain_id, topic_id);
}

}  // namespace mdds
