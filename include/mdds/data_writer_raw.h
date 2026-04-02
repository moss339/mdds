#ifndef MDDS_DATA_WRITER_RAW_H
#define MDDS_DATA_WRITER_RAW_H

#include "types.h"
#include "qos.h"
#include "transport.h"
#include "topic.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <cstring>

namespace moss {
namespace mdds {

/**
 * @brief 传输原始字节的 DataWriter
 *
 * 与 DataWriter<T> 的区别：
 * - 不依赖 T::serialize() 方法
 * - 直接接收 uint8_t* 字节数组 + size
 * - 由上层（mcom）负责 protobuf 序列化
 *
 * 这是 mdds 从"数据类型相关"到"数据类型无关"的关键改动。
 */
class DataWriterRaw {
public:
    DataWriterRaw(std::shared_ptr<TopicBase> topic,
                 std::shared_ptr<Transport> transport,
                 const QoSConfig& qos);

    ~DataWriterRaw() = default;

    bool write_raw(const uint8_t* data, size_t size, uint64_t timestamp_us);

    TopicId get_topic_id() const;
    const std::string& get_topic_name() const;
    SequenceNumber get_sequence_number() const;

private:
    std::shared_ptr<TopicBase> topic_;
    std::shared_ptr<Transport> transport_;
    QoSConfig qos_;
    std::atomic<SequenceNumber> sequence_number_{0};
    std::mutex write_mutex_;
};

}  // namespace mdds
}  // namespace moss
#endif // MDDS_DATA_WRITER_RAW_H