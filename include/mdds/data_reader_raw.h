#ifndef MDDS_DATA_READER_RAW_H
#define MDDS_DATA_READER_RAW_H

#include "types.h"
#include "qos.h"
#include "transport.h"
#include "topic.h"
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

namespace moss {
namespace mdds {

/**
 * @brief 接收原始字节的 DataReader
 *
 * 与 DataReader<T> 的区别：
 * - 不依赖 T::deserialize() 方法
 * - 回调直接传递 uint8_t* + size
 * - 由上层（mcom）负责 protobuf 反序列化
 *
 * 这是 mdds 从"数据类型相关"到"数据类型无关"的关键改动。
 */
class DataReaderRaw {
public:
    using RawCallback = std::function<void(const uint8_t* data, size_t size, uint64_t timestamp_us)>;

    DataReaderRaw(std::shared_ptr<TopicBase> topic,
                  std::shared_ptr<Transport> transport,
                  const QoSConfig& qos);

    ~DataReaderRaw();

    bool has_data() const;
    bool read_raw(uint64_t* timestamp_out);

    void set_callback(RawCallback callback);
    void clear_callback();

    TopicId get_topic_id() const;
    const std::string& get_topic_name() const;

private:
    void receive_loop();
    void handle_incoming_data(const void* data, size_t size, const Endpoint& sender);

    std::shared_ptr<TopicBase> topic_;
    std::shared_ptr<Transport> transport_;
    QoSConfig qos_;
    RawCallback callback_;

    mutable std::mutex mutex_;
    std::vector<uint8_t> last_raw_data_;
    uint64_t last_timestamp_us_ = 0;

    std::atomic<bool> running_{false};
    std::thread receive_thread_;
};

}  // namespace mdds
}  // namespace moss
#endif // MDDS_DATA_READER_RAW_H