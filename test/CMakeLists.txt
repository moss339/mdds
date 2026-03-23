# MDDS Test CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(mdds_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(Threads REQUIRED)

# Include directories
# MDDS headers are at ../../include/mdds/
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Create test executables
add_executable(domain_participant_test domain_participant_test.cpp)
add_executable(topic_test topic_test.cpp)
add_executable(discovery_test discovery_test.cpp)
add_executable(transport_test transport_test.cpp)
add_executable(integration_test integration_test.cpp)

# Link libraries
target_link_libraries(domain_participant_test Threads::Threads)
target_link_libraries(topic_test Threads::Threads)
target_link_libraries(discovery_test Threads::Threads)
target_link_libraries(transport_test Threads::Threads)
target_link_libraries(integration_test Threads::Threads)

# Enable testing
enable_testing()

# Register tests
add_test(NAME domain_participant_test COMMAND domain_participant_test)
add_test(NAME topic_test COMMAND topic_test)
add_test(NAME discovery_test COMMAND discovery_test)
add_test(NAME transport_test COMMAND transport_test)
add_test(NAME integration_test COMMAND integration_test)
