#include "mdds/mdds.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

// Sensor data type (must match publisher)
struct SensorData {
    uint32_t sensor_id;
    float temperature;
    float humidity;
    uint64_t timestamp;

    // Serialization
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buffer(sizeof(SensorData));
        std::memcpy(buffer.data(), this, sizeof(SensorData));
        return buffer;
    }

    // Deserialization
    static SensorData deserialize(const uint8_t* data, size_t size) {
        SensorData result;
        if (size >= sizeof(SensorData)) {
            std::memcpy(&result, data, sizeof(SensorData));
        }
        return result;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "=== MDDS Sensor Subscriber ===" << std::endl;

    // Parse command line arguments
    uint8_t domain_id = 0;
    std::string topic_name = "SensorData";
    uint16_t discovery_port = 7412;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-d" && i + 1 < argc) {
            domain_id = static_cast<uint8_t>(std::stoi(argv[++i]));
        } else if (arg == "-t" && i + 1 < argc) {
            topic_name = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            discovery_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
    }

    std::cout << "Domain ID: " << (int)domain_id << std::endl;
    std::cout << "Topic: " << topic_name << std::endl;
    std::cout << "Discovery Port: " << discovery_port << std::endl;

    // 1. Create DomainParticipant
    auto participant = mdds::DomainParticipant::create(domain_id);
    if (!participant) {
        std::cerr << "Failed to create DomainParticipant" << std::endl;
        return 1;
    }

    // Set discovery server endpoint
    participant->set_discovery_endpoint(
        mdds::Endpoint("127.0.0.1", discovery_port, mdds::TransportType::UDP));

    // 2. Start participant
    if (!participant->start()) {
        std::cerr << "Failed to start participant" << std::endl;
        return 1;
    }

    // 3. Create Subscriber with callback
    auto subscriber = participant->create_subscriber<SensorData>(
        topic_name,
        [](const SensorData& data, uint64_t timestamp) {
            std::cout << ">>> Received: sensor_id=" << data.sensor_id
                      << ", temp=" << data.temperature << "C"
                      << ", humidity=" << data.humidity << "%"
                      << ", ts=" << timestamp
                      << std::endl;
        });

    if (!subscriber) {
        std::cerr << "Failed to create subscriber" << std::endl;
        return 1;
    }

    std::cout << "Subscriber created successfully" << std::endl;
    std::cout << "Waiting for data..." << std::endl;

    // 4. Wait for data (use read() with timeout)
    int count = 0;
    while (count < 100) {
        SensorData data;
        if (subscriber->read(data)) {
            std::cout << ">>> [read] Received: sensor_id=" << data.sensor_id
                      << ", temp=" << data.temperature << "C"
                      << ", humidity=" << data.humidity << "%"
                      << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++count;
    }

    // Cleanup
    participant->stop();

    std::cout << "Subscriber shutdown complete" << std::endl;

    return 0;
}
