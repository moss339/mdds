#include "mdds/mdds.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

// Simple test data type
struct TestData {
    uint32_t id;
    float value;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buffer(sizeof(TestData));
        std::memcpy(buffer.data(), this, sizeof(TestData));
        return buffer;
    }

    static TestData deserialize(const uint8_t* data, size_t size) {
        TestData result;
        if (size >= sizeof(TestData)) {
            std::memcpy(&result, data, sizeof(TestData));
        }
        return result;
    }
};

void test_types() {
    std::cout << "Test: types..." << std::endl;

    // Test MessageHeader size
    static_assert(sizeof(mdds::MessageHeader) == 24, "MessageHeader must be 24 bytes");
    assert(sizeof(mdds::MessageHeader) == 24);

    // Test constants
    assert(mdds::MDDS_MAGIC == 0x4D444453);
    assert(mdds::MDDS_VERSION == 0x0001);
    assert(mdds::MDDS_HEADER_SIZE == 24);

    std::cout << "  PASSED" << std::endl;
}

void test_qos() {
    std::cout << "Test: qos..." << std::endl;

    mdds::QoSConfig qos1;
    assert(qos1.reliability == mdds::QoSFlags::BEST_EFFORT);
    assert(qos1.durability == mdds::QoSFlags::VOLATILE);

    mdds::QoSConfig qos2(mdds::QoSFlags::RELIABLE, mdds::QoSFlags::TRANSIENT_LOCAL);
    assert(qos2.reliability == mdds::QoSFlags::RELIABLE);
    assert(qos2.durability == mdds::QoSFlags::TRANSIENT_LOCAL);

    // Test compatibility
    mdds::QoSConfig pub_qos(mdds::QoSFlags::BEST_EFFORT, mdds::QoSFlags::TRANSIENT_LOCAL);
    mdds::QoSConfig sub_qos(mdds::QoSFlags::BEST_EFFORT, mdds::QoSFlags::VOLATILE);
    assert(pub_qos.is_compatible(sub_qos));  // TRANSIENT_LOCAL can send to VOLATILE

    std::cout << "  PASSED" << std::endl;
}

void test_endpoint() {
    std::cout << "Test: endpoint..." << std::endl;

    mdds::Endpoint ep1("192.168.1.1", 7412, mdds::TransportType::UDP);
    assert(ep1.address_ == "192.168.1.1");
    assert(ep1.port_ == 7412);
    assert(ep1.type_ == mdds::TransportType::UDP);

    mdds::Endpoint ep2("192.168.1.1", 7412, mdds::TransportType::UDP);
    assert(ep1 == ep2);

    std::cout << "  PASSED" << std::endl;
}

void test_domain_participant() {
    std::cout << "Test: domain_participant..." << std::endl;

    auto participant = mdds::DomainParticipant::create(0);
    assert(participant != nullptr);
    assert(participant->get_domain_id() == 0);
    assert(participant->get_participant_id() != 0);
    assert(!participant->is_running());

    std::cout << "  PASSED" << std::endl;
}

void test_topic() {
    std::cout << "Test: topic..." << std::endl;

    mdds::Topic<TestData> topic("TestTopic", 1);
    assert(topic.get_name() == "TestTopic");
    assert(topic.get_topic_id() == 1);

    std::cout << "  PASSED" << std::endl;
}

void test_error() {
    std::cout << "Test: error..." << std::endl;

    assert(std::string(mdds::error_to_string(mdds::MddsError::OK)) == "OK");
    assert(std::string(mdds::error_to_string(mdds::MddsError::INVALID_PARAM)) == "Invalid parameter");
    assert(std::string(mdds::error_to_string(mdds::MddsError::NOT_FOUND)) == "Not found");

    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "=== MDDS Basic Tests ===" << std::endl;

    test_types();
    test_qos();
    test_endpoint();
    test_domain_participant();
    test_topic();
    test_error();

    std::cout << "\n=== All Tests PASSED ===" << std::endl;

    return 0;
}
