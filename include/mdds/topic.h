#ifndef MDDS_TOPIC_H
#define MDDS_TOPIC_H

#include "types.h"
#include "qos.h"
#include <typeinfo>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace moss {
namespace mdds {

// ========== TopicBase (non-templated, for raw writers/readers) ==========

class TopicBase {
public:
    TopicBase(const std::string& name, TopicId topic_id)
        : name_(name)
        , topic_id_(topic_id)
        , qos_(default_qos::publisher()) {}

    const std::string& get_name() const { return name_; }
    TopicId get_topic_id() const { return topic_id_; }
    const QoSConfig& get_qos() const { return qos_; }
    void set_qos(const QoSConfig& qos) { qos_ = qos; }

private:
    std::string name_;
    TopicId topic_id_;
    QoSConfig qos_;
};

// ========== Topic Template ==========

template<typename T>
class Topic {
public:
    using DataType = T;

    Topic(const std::string& name, TopicId topic_id)
        : name_(name)
        , topic_id_(topic_id)
        , qos_(default_qos::publisher()) {
        type_name_ = typeid(T).name();
    }

    ~Topic() = default;

    const std::string& get_name() const { return name_; }
    const std::string& get_type_name() const { return type_name_; }
    TopicId get_topic_id() const { return topic_id_; }
    const QoSConfig& get_qos() const { return qos_; }
    void set_qos(const QoSConfig& qos) { qos_ = qos; }

private:
    std::string name_;
    std::string type_name_;
    TopicId topic_id_;
    QoSConfig qos_;
};

// ========== TopicManager ==========

class TopicManager {
public:
    TopicManager();
    ~TopicManager();

    TopicId register_topic(const std::string& topic_name, const std::string& type_name);
    bool unregister_topic(const std::string& topic_name);
    TopicId get_topic_id(const std::string& topic_name) const;
    std::string get_topic_name(TopicId topic_id) const;
    bool topic_exists(const std::string& topic_name) const;
    std::vector<std::string> get_all_topics() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TopicId> name_to_id_;
    std::unordered_map<TopicId, std::string> id_to_name_;
    std::unordered_map<std::string, std::string> name_to_type_;
    TopicId next_topic_id_{1};
};

}  // namespace mdds

}  // namespace moss
#endif  // MDDS_TOPIC_H
