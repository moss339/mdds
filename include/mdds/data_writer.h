#ifndef MDDS_DATA_WRITER_H
#define MDDS_DATA_WRITER_H

#include "types.h"
#include "qos.h"
#include "transport.h"
#include "topic.h"
#include <memory>
#include <mutex>
#include <chrono>
#include <cstring>
#include <atomic>

namespace mdds {

// ========== DataWriter Template ==========

template<typename T>
class DataWriter {
public:
    DataWriter(std::shared_ptr<Topic<T>> topic,
               std::shared_ptr<Transport> transport,
               const QoSConfig& qos);
    ~DataWriter();

    // Write data with timestamp
    bool write(const T& data, uint64_t timestamp);

    // Write data (uses current time)
    bool write(const T& data);

    // Get topic ID
    TopicId get_topic_id() const { return topic_->get_topic_id(); }

    // Get topic name
    const std::string& get_topic_name() const { return topic_->get_name(); }

    // Get sequence number
    SequenceNumber get_sequence_number() const { return sequence_number_.load(); }

private:
    std::shared_ptr<Topic<T>> topic_;
    std::shared_ptr<Transport> transport_;
    QoSConfig qos_;
    std::atomic<SequenceNumber> sequence_number_{0};
    std::mutex write_mutex_;
};

// ========== DataWriter Implementation ==========

template<typename T>
DataWriter<T>::DataWriter(std::shared_ptr<Topic<T>> topic,
                          std::shared_ptr<Transport> transport,
                          const QoSConfig& qos)
    : topic_(std::move(topic))
    , transport_(std::move(transport))
    , qos_(qos) {
}

template<typename T>
DataWriter<T>::~DataWriter() = default;

template<typename T>
bool DataWriter<T>::write(const T& data, uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(write_mutex_);

    // Serialize data
    auto serialized = data.serialize();
    size_t payload_size = serialized.size();

    // Validate payload_size - ensure it matches actual serialized data
    if (payload_size == 0 || payload_size > MDDS_MAX_PAYLOAD_SIZE) {
        return false;
    }

    // Additional validation: ensure serialized data pointer is valid
    if (!serialized.data()) {
        return false;
    }

    // Build header
    MessageHeader header;
    header.magic = MDDS_MAGIC;
    header.version = MDDS_VERSION;
    header.message_type = static_cast<uint8_t>(MessageType::DATA);
    header.flags = qos_.to_flags();
    header.topic_id = topic_->get_topic_id();
    header.sequence_number = ++sequence_number_;
    header.timestamp = timestamp;
    header.payload_length = static_cast<uint32_t>(payload_size);

    // Validate total message size won't overflow
    size_t total_size = sizeof(MessageHeader) + payload_size;
    if (total_size < sizeof(MessageHeader) || total_size > MDDS_MAX_PAYLOAD_SIZE + sizeof(MessageHeader)) {
        return false;
    }

    // Combine header and payload
    std::vector<uint8_t> buffer(total_size);
    std::memcpy(buffer.data(), &header, sizeof(MessageHeader));
    std::memcpy(buffer.data() + sizeof(MessageHeader), serialized.data(), payload_size);

    // Validate that buffer contains exactly what we expect
    if (buffer.size() != total_size) {
        return false;
    }

    // Send to all matched subscribers (via transport)
    // Note: destination is broadcast for now
    Endpoint dest;
    dest.address_ = "255.255.255.255";
    dest.port_ = 0;  // Will be resolved by discovery
    dest.type_ = TransportType::UDP;

    return transport_->send(buffer.data(), buffer.size(), dest);
}

template<typename T>
bool DataWriter<T>::write(const T& data) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return write(data, now);
}

}  // namespace mdds

#endif  // MDDS_DATA_WRITER_H
