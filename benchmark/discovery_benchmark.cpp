#include "mdds/mdds.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>
#include <iomanip>
#include <cstdlib>

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
    int current = received_count.fetch_add(1);
    if (current < 5) {
        std::cout << "  Received: id=" << data.id << ", value=" << data.value
                  << " at " << (std::chrono::steady_clock::now() - start_time).count() / 1000000.0 << "ms" << std::endl;
    }
}

void run_subscriber(int id) {
    auto participant = mdds::DomainParticipant::create(0);
    participant->enable_multicast_discovery(true);
    participant->start();

    auto subscriber = participant->create_subscriber<BenchmarkData>("BenchmarkTopic", data_callback);

    // Wait a bit to receive some data
    std::this_thread::sleep_for(std::chrono::seconds(2));

    participant->stop();
}

void run_publisher(int id) {
    auto participant = mdds::DomainParticipant::create(0);
    participant->enable_multicast_discovery(true);
    participant->start();

    auto publisher = participant->create_publisher<BenchmarkData>("BenchmarkTopic");

    // Wait for discovery
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Send data
    for (int i = 0; i < 10; ++i) {
        BenchmarkData data;
        data.id = id;
        data.value = (float)i;
        publisher->write(data);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    participant->stop();
}

int main(int argc, char* argv[]) {
    int num_instances = 50;
    bool is_publisher = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-n" && i + 1 < argc) {
            num_instances = std::stoi(argv[++i]);
        } else if (arg == "-s") {
            is_publisher = false;
        } else if (arg == "-p") {
            is_publisher = true;
        }
    }

    std::cout << "=== MDDS Discovery Benchmark ===" << std::endl;
    std::cout << "Mode: " << (is_publisher ? "Publisher" : "Subscriber") << std::endl;
    std::cout << "Instances: " << num_instances << std::endl;
    std::cout << std::endl;

    start_time = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;

    if (is_publisher) {
        std::cout << "Starting " << num_instances << " publishers..." << std::endl;
        for (int i = 0; i < num_instances; ++i) {
            threads.emplace_back(run_publisher, i);
            if ((i + 1) % 10 == 0) {
                std::cout << "  Started " << (i + 1) << " publishers" << std::endl;
            }
        }

        auto start = std::chrono::steady_clock::now();
        for (auto& t : threads) {
            t.join();
        }
        auto elapsed = std::chrono::steady_clock::now() - start;

        std::cout << std::endl;
        std::cout << "All publishers finished in " << std::fixed << std::setprecision(2)
                  << elapsed.count() / 1000000.0 << " ms" << std::endl;
    } else {
        std::cout << "Starting " << num_instances << " subscribers..." << std::endl;
        for (int i = 0; i < num_instances; ++i) {
            threads.emplace_back(run_subscriber, i);
            if ((i + 1) % 10 == 0) {
                std::cout << "  Started " << (i + 1) << " subscribers" << std::endl;
            }
        }

        // Wait for subscribers
        std::this_thread::sleep_for(std::chrono::seconds(3));

        auto elapsed = std::chrono::steady_clock::now() - start_time;
        std::cout << std::endl;
        std::cout << "Total messages received: " << received_count.load() << std::endl;
        std::cout << "Elapsed time: " << std::fixed << std::setprecision(2)
                  << elapsed.count() / 1000000.0 << " ms" << std::endl;
    }

    std::cout << std::endl << "Benchmark complete." << std::endl;
    return 0;
}