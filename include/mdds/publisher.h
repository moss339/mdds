#ifndef MDDS_PUBLISHER_H
#define MDDS_PUBLISHER_H

#include "types.h"
#include "qos.h"
#include "topic.h"
#include <memory>
#include <string>

namespace moss {
namespace mdds {

// Forward declarations
class DomainParticipant;

template<typename T>
class DataWriter;

// ========== Publisher Template ==========

template<typename T>
class Publisher {
public:
    Publisher() = default;

    // Constructor is public to allow make_shared access
    // Use DomainParticipant::create_publisher() for construction
    Publisher(std::shared_ptr<DomainParticipant> participant,
             std::shared_ptr<Topic<T>> topic,
             std::shared_ptr<DataWriter<T>> writer)
        : participant_(std::move(participant))
        , topic_(std::move(topic))
        , writer_(std::move(writer)) {}

    ~Publisher() = default;

    // Write data
    bool write(const T& data);
    bool write(const T& data, uint64_t timestamp);

    // Get topic name
    const std::string& get_topic_name() const;

    // Get topic ID
    TopicId get_topic_id() const;

    // Get data writer
    std::shared_ptr<DataWriter<T>> get_writer() const { return writer_; }

private:
    std::shared_ptr<DomainParticipant> participant_;
    std::shared_ptr<Topic<T>> topic_;
    std::shared_ptr<DataWriter<T>> writer_;
};

// ========== Publisher Implementation ==========

template<typename T>
bool Publisher<T>::write(const T& data) {
    if (!writer_) {
        return false;
    }
    return writer_->write(data);
}

template<typename T>
bool Publisher<T>::write(const T& data, uint64_t timestamp) {
    if (!writer_) {
        return false;
    }
    return writer_->write(data, timestamp);
}

template<typename T>
const std::string& Publisher<T>::get_topic_name() const {
    static std::string empty;
    return topic_ ? topic_->get_name() : empty;
}

template<typename T>
TopicId Publisher<T>::get_topic_id() const {
    return topic_ ? topic_->get_topic_id() : 0;
}

}  // namespace mdds

}  // namespace moss
#endif  // MDDS_PUBLISHER_H
