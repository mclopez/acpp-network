enable_testing()

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


add_executable(acpp-network-tests
    main.cpp
    socket_test.cpp
)

target_include_directories(acpp-network-tests 
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)


target_link_libraries(acpp-network-tests PRIVATE acpp-network)

target_link_libraries(acpp-network-tests 
PRIVATE
    gtest::gtest
    spdlog::spdlog
)

# Add tests
add_test(NAME acpp-network-tests COMMAND libacpp-network-tests)



if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")

    execute_process(
        COMMAND xcodebuild -version -sdk macosx Path
        OUTPUT_VARIABLE MACOSX_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(CMAKE_OSX_SYSROOT ${MACOSX_SDK_PATH})

endif()

