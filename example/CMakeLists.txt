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
