# MOSS Decentralized Discovery Service (mdds)

A lightweight C++17 implementation of a DDS-like publish-subscribe middleware with decentralized service discovery over UDP.

## Features

- **Domain Participant**: Logical partition of the network
- **Publisher/Subscriber**: Data distribution with QoS support
- **Topic**: Typed topic registration and discovery
- **QoS Configuration**: Reliability, durability, history depth
- **UDP Transport**: Lightweight unicast and multicast communication
- **Discovery Server**: Optional centralized discovery server
- **C++17**: Modern C++ with smart pointers and template support

## Architecture

```
mdds/
├── include/mdds/
│   ├── mdds.h              # Main include
│   ├── types.h             # Type definitions
│   ├── qos.h               # QoS configuration
│   ├── domain_participant.h # Domain participant
│   ├── topic.h              # Topic registration
│   ├── publisher.h          # Publisher
│   ├── subscriber.h         # Subscriber
│   ├── data_writer.h        # Data writer
│   ├── data_reader.h        # Data reader
│   ├── data_writer_raw.h    # Raw data writer (no serialization)
│   ├── data_reader_raw.h    # Raw data reader (no serialization)
│   ├── transport.h          # Transport interface
│   ├── discovery.h          # Discovery protocol
│   └── discovery_server.h   # Discovery server
├── src/                     # Implementation files
└── test/                    # Unit tests
```

## Dependencies

- mshm (Shared Memory)
- pthread

## Building

```bash
cd mdds
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMDDS_BUILD_TESTS=ON
make -j4
```

## Running Tests

```bash
ctest
```

## Usage Example

```cpp
#include <mdds/mdds.h>

int main() {
    // Create domain participant
    auto participant = moss::mdds::DomainParticipant::create(0, "participant1");

    // Create topic
    auto topic = participant->create_topic("chatter", "std_msgs::String");

    // Create publisher
    auto publisher = participant->create_publisher();
    auto writer = publisher->create_datawriter(topic);

    // Create subscriber
    auto subscriber = participant->create_subscriber();
    auto reader = subscriber->create_datareader(topic,
        [](const void* data, size_t size) {
            // Handle data
        });

    // Publish data (raw bytes - serialization done by caller)
    std::vector<uint8_t> buffer = {'h', 'e', 'l', 'l', 'o'};
    writer->write(buffer.data(), buffer.size());

    return 0;
}
```

## Key Classes

- **DomainParticipant**: Main entry point, manages publishers/subscribers
- **Publisher**: Manages data writers
- **Subscriber**: Manages data readers
- **DataWriterRaw**: Writes raw byte data (no internal serialization)
- **DataReaderRaw**: Reads raw byte data (no internal deserialization)
- **QoSConfig**: Quality of Service settings

## QoS Configuration

```cpp
moss::mdds::QoSConfig qos;
qos.reliability = moss::mdds::ReliabilityKind::RELIABLE;
qos.durability = moss::mdds::DurabilityKind::TRANSIENT_LOCAL;
qos.history_depth = 10;
```

## License

MIT License
