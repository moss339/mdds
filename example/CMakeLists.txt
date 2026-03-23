# MDDS Examples CMakeLists.txt

# Find shm library
find_library(SHM_LIBRARY NAMES shm PATHS
    ${CMAKE_BINARY_DIR}/lib
    ${PROJECT_SOURCE_DIR}/shm/build
    NO_DEFAULT_PATH)

# Sensor Publisher
add_executable(sensor_publisher sensor_publisher.cpp)
target_link_libraries(sensor_publisher
    mdds
    ${SHM_LIBRARY}
    Threads::Threads
)
target_include_directories(sensor_publisher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${PROJECT_SOURCE_DIR}/shm/include
)

# Sensor Subscriber
add_executable(sensor_subscriber sensor_subscriber.cpp)
target_link_libraries(sensor_subscriber
    mdds
    ${SHM_LIBRARY}
    Threads::Threads
)
target_include_directories(sensor_subscriber PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${PROJECT_SOURCE_DIR}/shm/include
)

# Set output directories
set_target_properties(sensor_publisher sensor_subscriber PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
