#include "domain_participant.h"
#include "transport.h"
#include <iostream>

namespace mdds {

// ========== Static Instance ==========

std::shared_ptr<DomainParticipant> DomainParticipantFactory::instance_;
std::mutex DomainParticipantFactory::mutex_;

std::shared_ptr<DomainParticipant> DomainParticipantFactory::get_instance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance_) {
        instance_ = std::make_shared<DomainParticipant>(0);
    }
    return instance_;
}

void DomainParticipantFactory::reset_instance() {
    std::lock_guard<std::mutex> lock(mutex_);
    instance_.reset();
}

// ========== DomainParticipant Implementation ==========

std::shared_ptr<DomainParticipant> DomainParticipant::create(DomainId domain_id) {
    return std::make_shared<DomainParticipant>(domain_id);
}

DomainParticipant::DomainParticipant(DomainId domain_id)
    : domain_id_(domain_id)
    , participant_id_(generate_participant_id())
    , discovery_endpoint_("127.0.0.1", DEFAULT_DISCOVERY_PORT, TransportType::UDP) {

    // Create control transport (UDP for discovery messages)
    control_transport_ = TransportFactory::create_udp_transport(domain_id);

    // Create discovery client
    discovery_ = std::make_unique<Discovery>(domain_id, participant_id_, discovery_endpoint_);
}

DomainParticipant::~DomainParticipant() {
    stop();
}

TopicId DomainParticipant::register_topic(const std::string& topic_name,
                                         const std::string& type_name) {
    std::lock_guard<std::mutex> lock(topics_mutex_);

    auto it = topic_name_to_id_.find(topic_name);
    if (it != topic_name_to_id_.end()) {
        return it->second;
    }

    TopicId id = next_topic_id_++;
    topic_name_to_id_[topic_name] = id;

    (void)type_name;  // Unused in this simplified implementation

    return id;
}

Endpoint DomainParticipant::get_discovery_endpoint() const {
    return discovery_endpoint_;
}

void DomainParticipant::set_discovery_endpoint(const Endpoint& endpoint) {
    discovery_endpoint_ = endpoint;
}

bool DomainParticipant::start() {
    if (running_) {
        return false;
    }

    // Initialize control transport
    Endpoint local_endpoint("0.0.0.0", 0, TransportType::UDP);
    if (!control_transport_->init(local_endpoint)) {
        return false;
    }

    // Start discovery client
    if (!discovery_->start()) {
        return false;
    }

    // Start multicast discovery if enabled
    if (enable_multicast_discovery_) {
        multicast_discovery_ = std::make_unique<MulticastDiscovery>(domain_id_, participant_id_);
        if (!multicast_discovery_->start()) {
            multicast_discovery_.reset();
        }
    }

    running_ = true;
    return true;
}

void DomainParticipant::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Stop multicast discovery
    if (multicast_discovery_) {
        multicast_discovery_->stop();
        multicast_discovery_.reset();
    }

    // Stop discovery
    if (discovery_) {
        discovery_->stop();
    }

    // Close transport
    if (control_transport_) {
        control_transport_->close();
    }
}

uint32_t DomainParticipant::generate_participant_id() {
    static std::atomic<uint32_t> counter{1};
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis(1, 0xFFFFFF);

    uint32_t random_part = dis(gen) << 8;
    uint32_t counter_part = counter.fetch_add(1);

    return random_part | (counter_part & 0xFF);
}

}  // namespace mdds
