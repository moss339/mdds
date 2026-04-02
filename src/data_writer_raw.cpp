#include "data_writer_raw.h"
#include "topic.h"
#include <chrono>

namespace moss {
namespace mdds {

DataWriterRaw::DataWriterRaw(std::shared_ptr<TopicBase> topic,
                             std::shared_ptr<Transport> transport,
                             const QoSConfig& qos)
    : topic_(std::move(topic))
    , transport_(std::move(transport))
    , qos_(qos) {
}

bool DataWriterRaw::write_raw(const uint8_t* data, size_t size, uint64_t timestamp_us) {
    std::lock_guard<std::mutex> lock(write_mutex_);

    if (size == 0 || size > MDDS_MAX_PAYLOAD_SIZE) {
        return false;
    }

    if (!data) {
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
    header.timestamp = static_cast<uint32_t>(timestamp_us & 0xFFFFFFFF);
    header.payload_length = static_cast<uint32_t>(size);

    // Combine header and payload
    size_t total_size = sizeof(MessageHeader) + size;
    std::vector<uint8_t> buffer(total_size);
    std::memcpy(buffer.data(), &header, sizeof(MessageHeader));
    std::memcpy(buffer.data() + sizeof(MessageHeader), data, size);

    // Send to all matched subscribers via multicast
    Endpoint dest;
    dest.address_ = "239.255.0.1";
    dest.port_ = 7412;  // Data port
    dest.type_ = TransportType::UDP;

    return transport_->send(buffer.data(), buffer.size(), dest);
}

TopicId DataWriterRaw::get_topic_id() const {
    return topic_->get_topic_id();
}

const std::string& DataWriterRaw::get_topic_name() const {
    return topic_->get_name();
}

SequenceNumber DataWriterRaw::get_sequence_number() const {
    return sequence_number_.load();
}

}  // namespace mdds
}  // namespace moss
