cmake_minimum_required(VERSION 3.28)

project(my-modules-project LANGUAGES CXX)

# Use C++23 and enable module scanning
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(VERBOSE ON)




add_library(acpp-network)

target_sources(acpp-network
  PUBLIC
    src/socket.cpp
)

target_include_directories(acpp-network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


