# MDDS Library Source CMakeLists.txt

set(MDDS_SOURCES
    topic.cpp
    domain_participant.cpp
    publisher.cpp
    subscriber.cpp
    data_writer.cpp
    data_reader.cpp
    discovery.cpp
    discovery_server.cpp
    multicast_discovery.cpp
    transport_udp.cpp
    transport_shm.cpp
)

# Create library
add_library(mdds ${MDDS_SOURCES})

# Include directories
target_include_directories(mdds PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/mdds
    ${SHM_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(mdds
    PUBLIC
    mshm
    pthread
)

# Set C++ standard
target_compile_features(mdds PUBLIC cxx_std_17)

# Enable symbol visibility
target_compile_options(mdds PRIVATE -fPIC)

# Set output directory
set_target_properties(mdds PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
