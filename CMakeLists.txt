# MDDS CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(mdds VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Add mshm from sibling directory (pre-built)
set(MSHM_LIBRARY /mnt/data/workspace/moss/mshm/build/src/libmshm.a)
set(MSHM_INCLUDE_DIR /mnt/data/workspace/moss/mshm/include)
add_library(mshm STATIC IMPORTED)
set_target_properties(mshm PROPERTIES IMPORTED_LOCATION ${MSHM_LIBRARY})
set_target_properties(mshm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MSHM_INCLUDE_DIR}")

# Add subdirectories
add_subdirectory(src)

# Add examples
option(MDDS_BUILD_EXAMPLES "Build MDDS examples" OFF)
if(MDDS_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

# Add benchmark (optional)
option(MDDS_BUILD_BENCHMARK "Build MDDS benchmark" ON)
if(MDDS_BUILD_BENCHMARK)
    add_subdirectory(benchmark)
endif()

# Add tests (optional)
option(MDDS_BUILD_TESTS "Build MDDS tests" OFF)
if(MDDS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Print configuration
message(STATUS "MDDS Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SHM library: ${MSHM_LIBRARY}")
message(STATUS "  SHM include: ${MSHM_INCLUDE_DIR}")
