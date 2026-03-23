#include "mdds/mdds.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <cstring>

// Sensor data type
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
    std::cout << "=== MDDS Sensor Publisher ===" << std::endl;

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

    // Enable multicast discovery (decentralized)
    participant->enable_multicast_discovery(true);

    // 2. Start participant
    if (!participant->start()) {
        std::cerr << "Failed to start participant" << std::endl;
        return 1;
    }

    // 3. Create Publisher
    auto publisher = participant->create_publisher<SensorData>(topic_name);
    if (!publisher) {
        std::cerr << "Failed to create publisher" << std::endl;
        return 1;
    }

    std::cout << "Publisher created successfully" << std::endl;

    // 4. Publish data loop
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> temp_dis(15.0, 35.0);
    std::uniform_real_distribution<> humidity_dis(30.0, 90.0);

    SensorData data;
    data.sensor_id = 1;

    std::cout << "Starting to publish sensor data..." << std::endl;

    int count = 0;
    while (count < 100) {  // Publish 100 messages then exit
        // Generate random sensor values
        data.temperature = static_cast<float>(temp_dis(gen));
        data.humidity = static_cast<float>(humidity_dis(gen));
        data.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

        // Write data
        if (publisher->write(data)) {
            std::cout << "[" << count << "] Published: sensor_id=" << data.sensor_id
                      << ", temp=" << data.temperature << "C"
                      << ", humidity=" << data.humidity << "%"
                      << std::endl;
        } else {
            std::cerr << "[" << count << "] Failed to publish data" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++count;
    }

    // Cleanup
    participant->stop();

    std::cout << "Publisher shutdown complete" << std::endl;

    return 0;
}
