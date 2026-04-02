cmake_minimum_required(VERSION 3.15)
project(mdds_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Set up mdds library
set(MDDS_LIBRARY /mnt/data/workspace/moss/mdds/build/lib/libmdds.a)
set(MDDS_INCLUDE_DIR /mnt/data/workspace/moss/mdds/include)
add_library(mdds STATIC IMPORTED)
set_target_properties(mdds PROPERTIES IMPORTED_LOCATION ${MDDS_LIBRARY})
set_target_properties(mdds PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MDDS_INCLUDE_DIR}")

# Set up mshm library
set(MSHM_LIBRARY /mnt/data/workspace/moss/mshm/build/src/libmshm.a)
set(MSHM_INCLUDE_DIR /mnt/data/workspace/moss/mshm/include)
add_library(mshm STATIC IMPORTED)
set_target_properties(mshm PROPERTIES IMPORTED_LOCATION ${MSHM_LIBRARY})
set_target_properties(mshm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MSHM_INCLUDE_DIR}")

# MDDS Examples CMakeLists.txt

# Sensor Publisher
add_executable(sensor_publisher sensor_publisher.cpp)
target_link_libraries(sensor_publisher
    mdds
    mshm
    Threads::Threads
)
target_include_directories(sensor_publisher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Sensor Subscriber
add_executable(sensor_subscriber sensor_subscriber.cpp)
target_link_libraries(sensor_subscriber
    mdds
    mshm
    Threads::Threads
)
target_include_directories(sensor_subscriber PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Set output directories
set_target_properties(sensor_publisher sensor_subscriber PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
