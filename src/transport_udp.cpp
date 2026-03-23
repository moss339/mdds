#include "transport.h"
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

namespace mdds {

// ========== UDP Transport Implementation ==========

class UdpTransport : public Transport {
public:
    UdpTransport(DomainId domain_id)
        : domain_id_(domain_id)
        , sock_(-1)
        , is_open_(false) {
    }

    ~UdpTransport() override {
        close();
    }

    bool init(const Endpoint& local_endpoint) override {
        // Create UDP socket
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) {
            return false;
        }

        // Set non-blocking
        int flags = fcntl(sock_, F_GETFL, 0);
        fcntl(sock_, F_SETFL, flags | O_NONBLOCK);

        // Bind to local address
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(local_endpoint.port_);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(sock_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close();
            return false;
        }

        local_endpoint_ = local_endpoint;
        local_endpoint_.port_ = ntohs(addr.sin_port);
        is_open_ = true;

        return true;
    }

    bool send(const void* data, size_t size, const Endpoint& destination) override {
        if (!is_open_ || sock_ < 0) {
            return false;
        }

        struct sockaddr_in dest_addr;
        std::memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(destination.port_);

        if (inet_pton(AF_INET, destination.address_.c_str(), &dest_addr.sin_addr) <= 0) {
            return false;
        }

        ssize_t sent = sendto(sock_, data, size, 0,
                             (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        return sent == static_cast<ssize_t>(size);
    }

    bool receive(void* buffer, size_t max_size,
                size_t* received, Endpoint* sender) override {
        if (!is_open_ || sock_ < 0) {
            return false;
        }

        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);

        ssize_t ret = recvfrom(sock_, buffer, max_size, 0,
                               (struct sockaddr*)&sender_addr, &addr_len);

        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                *received = 0;
                return true;
            }
            return false;
        }

        *received = static_cast<size_t>(ret);

        if (sender != nullptr) {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, ip_str, sizeof(ip_str));
            sender->address_ = ip_str;
            sender->port_ = ntohs(sender_addr.sin_port);
            sender->type_ = TransportType::UDP;
        }

        return true;
    }

    Endpoint get_local_endpoint() const override {
        return local_endpoint_;
    }

    void set_receive_callback(ReceiveCallback callback) override {
        callback_ = std::move(callback);
    }

    bool is_open() const override {
        return is_open_;
    }

    void close() override {
        if (sock_ >= 0) {
            ::close(sock_);
            sock_ = -1;
        }
        is_open_ = false;
    }

    // Internal: get socket fd for select()
    int get_socket_fd() const { return sock_; }

private:
    DomainId domain_id_;
    int sock_;
    bool is_open_;
    Endpoint local_endpoint_;
    ReceiveCallback callback_;
};

// ========== Transport Factory ==========

std::unique_ptr<Transport> TransportFactory::create_udp_transport(DomainId domain_id) {
    return std::make_unique<UdpTransport>(domain_id);
}

std::unique_ptr<Transport> TransportFactory::create_transport(DomainId domain_id, TopicId topic_id,
                                                              TransportType type) {
    switch (type) {
        case TransportType::UDP:
            return create_udp_transport(domain_id);
        case TransportType::SHM:
            return create_shm_transport(domain_id, topic_id);
        case TransportType::TCP:
        default:
            return nullptr;
    }
}

}  // namespace mdds
