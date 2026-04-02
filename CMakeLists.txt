cmake_minimum_required(VERSION 3.14)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(MDDS_BUILD_TESTS "Build tests" ON)

project(mdds)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# Find mshm using direct imported target
set(MSHM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../mshm" CACHE PATH "mshm root directory")
if(EXISTS "${MSHM_ROOT}/build/src/libmshm.so")
    set(MSHM_LIBRARY "${MSHM_ROOT}/build/src/libmshm.so")
else()
    set(MSHM_LIBRARY "${MSHM_ROOT}/src/libmshm.so")
endif()
set(MSHM_INCLUDE_DIR "${MSHM_ROOT}/include")

add_library(mshm::mshm SHARED IMPORTED)
set_target_properties(mshm::mshm PROPERTIES
    IMPORTED_LOCATION "${MSHM_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${MSHM_INCLUDE_DIR}"
)

add_subdirectory(src)

if(MDDS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

install(DIRECTORY include/ DESTINATION include)
