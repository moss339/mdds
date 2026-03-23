#include "mdds/mdds.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

struct BenchmarkData {
    uint32_t id;
    float value;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buffer(sizeof(BenchmarkData));
        std::memcpy(buffer.data(), this, sizeof(BenchmarkData));
        return buffer;
    }

    static BenchmarkData deserialize(const uint8_t* data, size_t size) {
        BenchmarkData result;
        if (size >= sizeof(BenchmarkData)) {
            std::memcpy(&result, data, sizeof(BenchmarkData));
        }
        return result;
    }
};

std::atomic<int> received_count{0};
std::chrono::steady_clock::time_point start_time;

void data_callback(const BenchmarkData& data, uint64_t timestamp) {
    received_count.fetch_add(1);
}

void run_subscriber_instance(int id, int duration_sec) {
    auto participant = mdds::DomainParticipant::create(0);
    participant->enable_multicast_discovery(true);
    if (!participant->start()) {
        std::cerr << "Failed to start participant" << std::endl;
        return;
    }

    auto subscriber = participant->create_subscriber<BenchmarkData>("BenchmarkTopic", data_callback);
    if (!subscriber) {
        std::cerr << "Failed to create subscriber" << std::endl;
        return;
    }

    std::cout << "Subscriber " << id << " started, listening for " << duration_sec << " seconds..." << std::endl;

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

    participant->stop();
    std::cout << "Subscriber " << id << " finished. Received " << received_count.load() << " messages" << std::endl;
}

void run_publisher_instance(int id, int num_messages) {
    auto participant = mdds::DomainParticipant::create(0);
    participant->enable_multicast_discovery(true);
    if (!participant->start()) {
        std::cerr << "Failed to start participant" << std::endl;
        return;
    }

    auto publisher = participant->create_publisher<BenchmarkData>("BenchmarkTopic");
    if (!publisher) {
        std::cerr << "Failed to create publisher" << std::endl;
        return;
    }

    // Wait for discovery
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Publisher " << id << " started, sending " << num_messages << " messages..." << std::endl;

    // Send data
    for (int i = 0; i < num_messages; ++i) {
        BenchmarkData data;
        data.id = id;
        data.value = (float)i;
        publisher->write(data);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    participant->stop();
    std::cout << "Publisher " << id << " finished" << std::endl;
}

int main(int argc, char* argv[]) {
    int num_instances = 2;
    int num_messages = 10;
    int duration_sec = 5;
    bool is_subscriber = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-n" && i + 1 < argc) {
            num_instances = std::stoi(argv[++i]);
        } else if (arg == "-m" && i + 1 < argc) {
            num_messages = std::stoi(argv[++i]);
        } else if (arg == "-d" && i + 1 < argc) {
            duration_sec = std::stoi(argv[++i]);
        } else if (arg == "-s") {
            is_subscriber = true;
        }
    }

    start_time = std::chrono::steady_clock::now();

    if (is_subscriber) {
        std::cout << "=== Running " << num_instances << " subscribers for " << duration_sec << " seconds ===" << std::endl;
        std::vector<std::thread> threads;
        for (int i = 0; i < num_instances; ++i) {
            threads.emplace_back(run_subscriber_instance, i, duration_sec);
        }
        for (auto& t : threads) {
            t.join();
        }
        std::cout << "All subscribers finished. Total received: " << received_count.load() << std::endl;
    } else {
        std::cout << "=== Running " << num_instances << " publishers ===" << std::endl;
        std::vector<std::thread> threads;
        for (int i = 0; i < num_instances; ++i) {
            threads.emplace_back(run_publisher_instance, i, num_messages);
        }
        for (auto& t : threads) {
            t.join();
        }
        std::cout << "All publishers finished" << std::endl;
    }

    auto total_time = std::chrono::steady_clock::now() - start_time;
    std::cout << "Total time: " << std::fixed << std::setprecision(2)
              << total_time.count() / 1000000.0 << " ms" << std::endl;

    return 0;
}