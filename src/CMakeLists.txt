add_library(acpp-network)

target_sources(acpp-network
  PUBLIC
    socket.cpp
    address.cpp
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/windows/socket_base.cpp>
    $<$<PLATFORM_ID:Darwing>:/macos/socket_base.cpp>
    $<$<PLATFORM_ID:Linux>:/Linux/socket_base.cpp>
)

target_include_directories(acpp-network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
