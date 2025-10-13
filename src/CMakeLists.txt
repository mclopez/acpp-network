add_library(acpp-network)

target_sources(acpp-network
  PUBLIC
    socket.cpp
)

target_include_directories(acpp-network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
