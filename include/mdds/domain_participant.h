#ifndef MDDS_DOMAIN_PARTICIPANT_H
#define MDDS_DOMAIN_PARTICIPANT_H

#include "types.h"
#include "qos.h"
#include "transport.h"
#include "discovery.h"
#include "topic.h"
#include "publisher.h"
#include "subscriber.h"
#include "data_writer.h"
#include "data_reader.h"
#include "data_writer_raw.h"
#include "data_reader_raw.h"
#include "multicast_discovery.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <random>

namespace moss {
namespace mdds {

// ========== Domain Participant ==========

class DomainParticipant : public std::enable_shared_from_this<DomainParticipant> {
public:
    // Create a domain participant instance
    static std::shared_ptr<DomainParticipant> create(DomainId domain_id);

    ~DomainParticipant();

    // Disable copy
    DomainParticipant(const DomainParticipant&) = delete;
    DomainParticipant& operator=(const DomainParticipant&) = delete;

    // Create a Publisher for a topic
    template<typename T>
    std::shared_ptr<Publisher<T>> create_publisher(const std::string& topic_name,
                                                     const QoSConfig& qos = default_qos::publisher());

    // Create a Subscriber for a topic
    template<typename T>
    std::shared_ptr<Subscriber<T>> create_subscriber(const std::string& topic_name,
                                                      typename Subscriber<T>::DataCallback callback,
                                                      const QoSConfig& qos = default_qos::subscriber());

    // Create a Raw Publisher (for protobuf serialization)
    std::shared_ptr<DataWriterRaw> create_writer_raw(const std::string& topic_name,
                                                      const QoSConfig& qos = default_qos::publisher());

    // Create a Raw Subscriber (for protobuf serialization)
    std::shared_ptr<DataReaderRaw> create_reader_raw(const std::string& topic_name,
                                                      const QoSConfig& qos = default_qos::subscriber());

    // Get Domain ID
    DomainId get_domain_id() const { return domain_id_; }

    // Get Participant ID
    ParticipantId get_participant_id() const { return participant_id_; }

    // Get discovery server endpoint
    Endpoint get_discovery_endpoint() const;

    // Set discovery server endpoint (for clients)
    void set_discovery_endpoint(const Endpoint& endpoint);

    // Start the participant
    bool start();

    // Stop the participant
    void stop();

    // Check if participant is running
    bool is_running() const { return running_; }

    // Internal access for template classes
    std::shared_ptr<DomainParticipant> get_shared() { return shared_from_this(); }

    // Multicast discovery control
    void enable_multicast_discovery(bool enable) { enable_multicast_discovery_ = enable; }
    bool is_multicast_discovery_enabled() const { return enable_multicast_discovery_; }

    // Add topic to multicast discovery
    void add_multicast_topic(const std::string& topic_name, bool is_publisher);

public:
    // Note: Constructor is public to allow make_shared.
    // Use DomainParticipant::create() for construction.
    explicit DomainParticipant(DomainId domain_id);

    // Register topic internally
    TopicId register_topic(const std::string& topic_name, const std::string& type_name);

    // Generate participant ID
    static uint32_t generate_participant_id();

    DomainId domain_id_;
    ParticipantId participant_id_;
    std::unique_ptr<Discovery> discovery_;
    std::unique_ptr<Transport> control_transport_;
    std::unique_ptr<MulticastDiscovery> multicast_discovery_;
    Endpoint discovery_endpoint_;
    std::atomic<bool> running_{false};
    bool is_server_{false};
    bool enable_multicast_discovery_{false};

    // Topic management
    std::mutex topics_mutex_;
    std::unordered_map<std::string, TopicId> topic_name_to_id_;
    TopicId next_topic_id_{1};
};

// ========== DomainParticipantFactory ==========

class DomainParticipantFactory {
public:
    static std::shared_ptr<DomainParticipant> get_instance();
    static void reset_instance();

private:
    static std::shared_ptr<DomainParticipant> instance_;
    static std::mutex mutex_;
};

// ========== Template Implementations ==========

template<typename T>
std::shared_ptr<Publisher<T>> DomainParticipant::create_publisher(
    const std::string& topic_name, const QoSConfig& qos) {

    static_assert(sizeof(T) > 0, "Invalid topic type");

    // Register topic
    TopicId topic_id = register_topic(topic_name, typeid(T).name());

    // Create topic object
    auto topic = std::make_shared<Topic<T>>(topic_name, topic_id);
    topic->set_qos(qos);

    // Create transport - bind to multicast data port
    auto transport = TransportFactory::create_transport(domain_id_, topic_id, TransportType::UDP);
    Endpoint local_ep("0.0.0.0", 7412, TransportType::UDP);
    transport->init(local_ep);

    // Get local endpoint before moving transport
    Endpoint transport_endpoint = transport->get_local_endpoint();

    // Create data writer
    auto writer = std::make_shared<DataWriter<T>>(topic, std::shared_ptr<Transport>(std::move(transport)), qos);

    // Create publisher
    auto publisher = std::make_shared<Publisher<T>>(shared_from_this(), topic, writer);

    // Register with discovery (skip if using multicast discovery)
    if (!enable_multicast_discovery_) {
        discovery_->register_publisher(topic_name, typeid(T).name(), topic_id,
                                      transport_endpoint, qos);
    }

    // Add to multicast discovery if enabled
    if (enable_multicast_discovery_ && multicast_discovery_) {
        multicast_discovery_->add_local_publisher(topic_name);
    }

    return publisher;
}

template<typename T>
std::shared_ptr<Subscriber<T>> DomainParticipant::create_subscriber(
    const std::string& topic_name,
    typename Subscriber<T>::DataCallback callback,
    const QoSConfig& qos) {

    static_assert(sizeof(T) > 0, "Invalid topic type");

    // Register topic
    TopicId topic_id = register_topic(topic_name, typeid(T).name());

    // Create topic object
    auto topic = std::make_shared<Topic<T>>(topic_name, topic_id);
    topic->set_qos(qos);

    // Create transport - bind to multicast data port for receiving
    auto transport = TransportFactory::create_transport(domain_id_, topic_id, TransportType::UDP);
    Endpoint local_ep("0.0.0.0", 7412, TransportType::UDP);
    transport->init(local_ep);

    // Get local endpoint before moving transport
    Endpoint transport_endpoint = transport->get_local_endpoint();

    // Create data reader
    auto reader = std::make_shared<DataReader<T>>(topic, std::shared_ptr<Transport>(std::move(transport)), qos);
    reader->set_callback(callback);

    // Create subscriber
    auto subscriber = std::make_shared<Subscriber<T>>(shared_from_this(), topic, reader);

    // Register with discovery (skip if using multicast discovery)
    if (!enable_multicast_discovery_) {
        discovery_->register_subscriber(topic_name, typeid(T).name(), topic_id,
                                      transport_endpoint, qos);
    }

    // Add to multicast discovery if enabled
    if (enable_multicast_discovery_ && multicast_discovery_) {
        multicast_discovery_->add_local_subscriber(topic_name);
    }

    return subscriber;
}

// ========== Raw Writer/Reader Factory Methods ==========

inline std::shared_ptr<DataWriterRaw> DomainParticipant::create_writer_raw(
    const std::string& topic_name, const QoSConfig& qos) {

    // Register topic with empty type name (raw doesn't need type info)
    TopicId topic_id = register_topic(topic_name, "raw");

    // Create non-templated topic
    auto topic = std::make_shared<TopicBase>(topic_name, topic_id);
    topic->set_qos(qos);

    // Create transport
    auto transport = TransportFactory::create_transport(domain_id_, topic_id, TransportType::UDP);
    Endpoint local_ep("0.0.0.0", 7412, TransportType::UDP);
    transport->init(local_ep);

    // Create raw data writer
    return std::make_shared<DataWriterRaw>(topic, std::move(transport), qos);
}

inline std::shared_ptr<DataReaderRaw> DomainParticipant::create_reader_raw(
    const std::string& topic_name, const QoSConfig& qos) {

    // Register topic with empty type name (raw doesn't need type info)
    TopicId topic_id = register_topic(topic_name, "raw");

    // Create non-templated topic
    auto topic = std::make_shared<TopicBase>(topic_name, topic_id);
    topic->set_qos(qos);

    // Create transport
    auto transport = TransportFactory::create_transport(domain_id_, topic_id, TransportType::UDP);
    Endpoint local_ep("0.0.0.0", 7412, TransportType::UDP);
    transport->init(local_ep);

    // Create raw data reader
    return std::make_shared<DataReaderRaw>(topic, std::move(transport), qos);
}

}  // namespace mdds

}  // namespace moss
#endif  // MDDS_DOMAIN_PARTICIPANT_H
