cmake_minimum_required(VERSION 3.28)

project(acpp-network-project LANGUAGES CXX)

# Use C++23 and enable module scanning
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(VERBOSE ON)

find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)



add_subdirectory(src)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
else()
    message(WARNING "Tests directory 'tests' not found - skipping tests")
endif()



