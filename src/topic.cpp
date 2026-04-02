#include "topic.h"

namespace moss {
namespace mdds {

// ========== TopicManager Implementation ==========

TopicManager::TopicManager() = default;

TopicManager::~TopicManager() = default;

TopicId TopicManager::register_topic(const std::string& topic_name,
                                     const std::string& type_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (topic_exists(topic_name)) {
        return name_to_id_.at(topic_name);
    }

    TopicId id = next_topic_id_++;
    name_to_id_[topic_name] = id;
    id_to_name_[id] = topic_name;
    name_to_type_[topic_name] = type_name;

    return id;
}

bool TopicManager::unregister_topic(const std::string& topic_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = name_to_id_.find(topic_name);
    if (it == name_to_id_.end()) {
        return false;
    }

    TopicId id = it->second;
    name_to_id_.erase(it);
    id_to_name_.erase(id);
    name_to_type_.erase(topic_name);

    return true;
}

TopicId TopicManager::get_topic_id(const std::string& topic_name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = name_to_id_.find(topic_name);
    if (it != name_to_id_.end()) {
        return it->second;
    }
    return 0;
}

std::string TopicManager::get_topic_name(TopicId topic_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = id_to_name_.find(topic_id);
    if (it != id_to_name_.end()) {
        return it->second;
    }
    return "";
}

bool TopicManager::topic_exists(const std::string& topic_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return name_to_id_.find(topic_name) != name_to_id_.end();
}

std::vector<std::string> TopicManager::get_all_topics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> topics;
    for (const auto& pair : name_to_id_) {
        topics.push_back(pair.first);
    }
    return topics;
}

}  // namespace mdds

}  // namespace moss
