#include "data_reader_raw.h"
#include "topic.h"
#include <cstring>

namespace moss {
namespace mdds {

DataReaderRaw::DataReaderRaw(std::shared_ptr<TopicBase> topic,
                             std::shared_ptr<Transport> transport,
                             const QoSConfig& qos)
    : topic_(std::move(topic))
    , transport_(std::move(transport))
    , qos_(qos) {
    // Start receive thread
    running_ = true;
    receive_thread_ = std::thread([this]() { receive_loop(); });
}

DataReaderRaw::~DataReaderRaw() {
    running_ = false;
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

void DataReaderRaw::receive_loop() {
    uint8_t buffer[MDDS_MAX_PAYLOAD_SIZE + sizeof(MessageHeader)];
    Endpoint sender;

    while (running_.load()) {
        size_t received = 0;
        if (transport_->receive(buffer, sizeof(buffer), &received, &sender)) {
            if (received > 0) {
                handle_incoming_data(buffer, received, sender);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool DataReaderRaw::has_data() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !last_raw_data_.empty();
}

bool DataReaderRaw::read_raw(uint64_t* timestamp_out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (last_raw_data_.empty()) {
        return false;
    }
    if (timestamp_out) {
        *timestamp_out = last_timestamp_us_;
    }
    last_raw_data_.clear();
    return true;
}

void DataReaderRaw::set_callback(RawCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

void DataReaderRaw::clear_callback() {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = nullptr;
}

TopicId DataReaderRaw::get_topic_id() const {
    return topic_->get_topic_id();
}

const std::string& DataReaderRaw::get_topic_name() const {
    return topic_->get_name();
}

void DataReaderRaw::handle_incoming_data(const void* data, size_t size, const Endpoint& sender) {
    (void)sender;

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

    // Extract payload
    const uint8_t* payload = static_cast<const uint8_t*>(data) + sizeof(MessageHeader);
    size_t payload_size = size - sizeof(MessageHeader);

    if (payload_size != header->payload_length) {
        return;
    }

    uint64_t timestamp_us = header->timestamp;

    // Store raw data
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_raw_data_.assign(payload, payload + payload_size);
        last_timestamp_us_ = timestamp_us;
    }

    // Invoke callback
    RawCallback callback_to_invoke;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_to_invoke = callback_;
    }

    if (callback_to_invoke) {
        callback_to_invoke(payload, payload_size, timestamp_us);
    }
}

}  // namespace mdds
}  // namespace moss
