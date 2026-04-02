cmake_minimum_required(VERSION 3.14)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

set(MDDS_SOURCES
    topic.cpp
    domain_participant.cpp
    publisher.cpp
    subscriber.cpp
    data_writer.cpp
    data_reader.cpp
    data_writer_raw.cpp
    data_reader_raw.cpp
    discovery.cpp
    discovery_server.cpp
    multicast_discovery.cpp
    transport_udp.cpp
    transport_shm.cpp
)

if(BUILD_SHARED_LIBS)
    add_library(mdds SHARED ${MDDS_SOURCES})
    set_target_properties(mdds PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
        POSITION_INDEPENDENT_CODE ON
    )
else()
    add_library(mdds STATIC ${MDDS_SOURCES})
    set_target_properties(mdds PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
    )
endif()

target_include_directories(mdds PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/mdds
    ${mshm_ROOT}/include
)

target_link_libraries(mdds PUBLIC
    mshm::mshm
    Threads::Threads
)

target_compile_features(mdds PUBLIC cxx_std_17)

install(TARGETS mdds
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
