#ifndef MDDS_SUBSCRIBER_H
#define MDDS_SUBSCRIBER_H

#include "types.h"
#include "qos.h"
#include "topic.h"
#include <memory>
#include <string>
#include <functional>

namespace mdds {

// Forward declarations
class DomainParticipant;

template<typename T>
class DataReader;

// ========== Subscriber Template ==========

template<typename T>
class Subscriber {
public:
    using DataCallback = std::function<void(const T& data, uint64_t timestamp)>;

    Subscriber() = default;

    // Constructor is public to allow make_shared access
    // Use DomainParticipant::create_subscriber() for construction
    Subscriber(std::shared_ptr<DomainParticipant> participant,
               std::shared_ptr<Topic<T>> topic,
               std::shared_ptr<DataReader<T>> reader)
        : participant_(std::move(participant))
        , topic_(std::move(topic))
        , reader_(std::move(reader)) {}

    ~Subscriber() = default;

    // Set data callback
    void set_callback(DataCallback callback);

    // Read data (non-blocking)
    bool read(T& data, uint64_t* timestamp = nullptr);

    // Check if data is available
    bool has_data() const;

    // Get topic name
    const std::string& get_topic_name() const;

    // Get topic ID
    TopicId get_topic_id() const;

    // Get data reader
    std::shared_ptr<DataReader<T>> get_reader() const { return reader_; }

private:
    std::shared_ptr<DomainParticipant> participant_;
    std::shared_ptr<Topic<T>> topic_;
    std::shared_ptr<DataReader<T>> reader_;
    DataCallback callback_;
};

// ========== Subscriber Implementation ==========

template<typename T>
void Subscriber<T>::set_callback(DataCallback callback) {
    callback_ = std::move(callback);
    if (reader_) {
        reader_->set_callback(
            [this](const T& data, uint64_t timestamp) {
                if (callback_) {
                    callback_(data, timestamp);
                }
            });
    }
}

template<typename T>
bool Subscriber<T>::read(T& data, uint64_t* timestamp) {
    return reader_ ? reader_->read(data, timestamp) : false;
}

template<typename T>
bool Subscriber<T>::has_data() const {
    return reader_ ? reader_->has_data() : false;
}

template<typename T>
const std::string& Subscriber<T>::get_topic_name() const {
    static std::string empty;
    return topic_ ? topic_->get_name() : empty;
}

template<typename T>
TopicId Subscriber<T>::get_topic_id() const {
    return topic_ ? topic_->get_topic_id() : 0;
}

}  // namespace mdds

#endif  // MDDS_SUBSCRIBER_H
