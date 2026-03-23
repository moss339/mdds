#ifndef MDDS_DATA_READER_H
#define MDDS_DATA_READER_H

#include "types.h"
#include "qos.h"
#include "transport.h"
#include "topic.h"
#include <memory>
#include <mutex>
#include <queue>
#include <functional>

namespace mdds {

// ========== DataReader Template ==========

template<typename T>
class DataReader {
public:
    using DataCallback = std::function<void(const T& data, uint64_t timestamp)>;

    DataReader(std::shared_ptr<Topic<T>> topic,
               std::shared_ptr<Transport> transport,
               const QoSConfig& qos);
    ~DataReader();

    // Check if data is available
    bool has_data() const;

    // Read next data (non-blocking)
    bool read(T& data, uint64_t* timestamp = nullptr);

    // Set data callback
    void set_callback(DataCallback callback);

    // Get topic ID
    TopicId get_topic_id() const { return topic_->get_topic_id(); }

    // Get topic name
    const std::string& get_topic_name() const { return topic_->get_name(); }

private:
    void handle_incoming_data(const void* data, size_t size, const Endpoint& sender);

    std::shared_ptr<Topic<T>> topic_;
    std::shared_ptr<Transport> transport_;
    QoSConfig qos_;
    DataCallback callback_;

    mutable std::mutex mutex_;
    std::queue<std::pair<T, uint64_t>> pending_data_;
};

// ========== DataReader Implementation ==========

template<typename T>
DataReader<T>::DataReader(std::shared_ptr<Topic<T>> topic,
                           std::shared_ptr<Transport> transport,
                           const QoSConfig& qos)
    : topic_(std::move(topic))
    , transport_(std::move(transport))
    , qos_(qos) {

    // Set receive callback
    transport_->set_receive_callback(
        [this](const void* data, size_t size, const Endpoint& sender) {
            this->handle_incoming_data(data, size, sender);
        });
}

template<typename T>
DataReader<T>::~DataReader() = default;

template<typename T>
bool DataReader<T>::has_data() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !pending_data_.empty();
}

template<typename T>
bool DataReader<T>::read(T& data, uint64_t* timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (pending_data_.empty()) {
        return false;
    }

    auto& item = pending_data_.front();
    data = item.first;
    if (timestamp != nullptr) {
        *timestamp = item.second;
    }
    pending_data_.pop();

    return true;
}

template<typename T>
void DataReader<T>::set_callback(DataCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

template<typename T>
void DataReader<T>::handle_incoming_data(const void* data, size_t size, const Endpoint& sender) {
    (void)sender;  // Unused

    if (size < sizeof(MessageHeader)) {
        return;
    }

    const MessageHeader* header = static_cast<const MessageHeader*>(data);

    // Verify magic
    if (header->magic != MDDS_MAGIC) {
        return;
    }

    // Verify message type
    if (header->message_type != static_cast<uint8_t>(MessageType::DATA)) {
        return;
    }

    // Verify topic ID
    if (header->topic_id != topic_->get_topic_id()) {
        return;
    }

    // Extract payload
    const uint8_t* payload = static_cast<const uint8_t*>(data) + sizeof(MessageHeader);
    size_t payload_size = size - sizeof(MessageHeader);

    if (payload_size != header->payload_length) {
        return;
    }

    // Deserialize data
    T obj = T::deserialize(payload, payload_size);

    // Prepare callback data before acquiring lock
    DataCallback callback_to_invoke;
    uint64_t timestamp = header->timestamp;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Add to pending queue
        pending_data_.emplace(obj, timestamp);

        // Copy callback under lock to invoke after unlock
        callback_to_invoke = callback_;
    }

    // Invoke callback OUTSIDE the lock to avoid deadlock and reentry issues
    if (callback_to_invoke) {
        callback_to_invoke(obj, timestamp);
    }
}

}  // namespace mdds

#endif  // MDDS_DATA_READER_H
