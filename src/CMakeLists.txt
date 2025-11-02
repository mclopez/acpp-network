add_library(acpp-network)

target_sources(acpp-network
  PUBLIC
    socket.cpp
    address.cpp
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/windows/socket_base.cpp>
    $<$<PLATFORM_ID:Darwin>:${CMAKE_CURRENT_SOURCE_DIR}/macos/socket_base.cpp>
    $<$<PLATFORM_ID:Linux>:${CMAKE_CURRENT_SOURCE_DIR}/linux/socket_base.cpp>
)

target_include_directories(acpp-network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
