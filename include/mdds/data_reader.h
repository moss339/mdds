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
#include <thread>
#include <atomic>

namespace moss {
namespace mdds {

// ========== DataReader Template ==========

/**
 * @deprecated 使用 DataReaderRaw 代替
 * DataReader<T> 依赖 T::deserialize() 方法，已废弃。
 * 请使用 DataReaderRaw + 上层 protobuf 反序列化。
 */
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
    void receive_loop();
    void handle_incoming_data(const void* data, size_t size, const Endpoint& sender);

    std::shared_ptr<Topic<T>> topic_;
    std::shared_ptr<Transport> transport_;
    QoSConfig qos_;
    DataCallback callback_;

    mutable std::mutex mutex_;
    std::queue<std::pair<T, uint64_t>> pending_data_;

    std::atomic<bool> running_{false};
    std::thread receive_thread_;
};

// ========== DataReader Implementation ==========

template<typename T>
DataReader<T>::DataReader(std::shared_ptr<Topic<T>> topic,
                           std::shared_ptr<Transport> transport,
                           const QoSConfig& qos)
    : topic_(std::move(topic))
    , transport_(std::move(transport))
    , qos_(qos) {

    // Start receive thread
    running_ = true;
    receive_thread_ = std::thread([this]() { receive_loop(); });
}

template<typename T>
DataReader<T>::~DataReader() {
    running_ = false;
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

template<typename T>
void DataReader<T>::receive_loop() {
    uint8_t buffer[1024];
    Endpoint sender;

    while (running_.load()) {
        size_t received = 0;
        if (transport_->receive(buffer, sizeof(buffer), &received, &sender)) {
            if (received > 0) {
                handle_incoming_data(buffer, received, sender);
            }
        }
        // Small sleep to avoid busy loop
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

template<typename T>
bool DataReader<T>::has_data() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !pending_data_.empty();
}

template<typename T>
bool DataReader<T>::read(T& data, uint64_t* timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Try to receive data from transport if queue is empty
    if (pending_data_.empty()) {
        uint8_t buffer[1024];
        size_t received = 0;
        Endpoint sender;

        // Non-blocking receive attempt
        if (transport_->receive(buffer, sizeof(buffer), &received, &sender)) {
            if (received > 0) {
                handle_incoming_data(buffer, received, sender);
            }
        }
    }

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

    // Verify topic ID - temporarily disabled for cross-process testing
    // TODO: Implement consistent topic_id assignment via discovery
    // if (header->topic_id != topic_->get_topic_id()) {
    //     return;
    // }

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

}  // namespace moss
#endif  // MDDS_DATA_READER_H
