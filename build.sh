#!/bin/bash 

#./conan_helper.sh
conan install . --output-folder=build/ --build=missing -s build_type=Release


cmake -S . -B ./build/ -DCMAKE_BUILD_TYPE=Release -G Ninja -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake #-DCMAKE_CXX_COMPILER=clang++ 

cmake --build ./build/  --config Release -v --parallel
