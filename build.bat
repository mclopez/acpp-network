conan install . --output-folder=build --build=missing -s build_type=RelWithDebInfo
cmake -S . -B ./build/ -DCMAKE_BUILD_TYPE=RelWithDebInfo -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=./build/conan_toolchain.cmake
cmake --build ./build/ --config RelWithDebInfo -v --parallel
