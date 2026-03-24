#pragma once

#include <chrono>
#include <iostream>
#include <string>

namespace {

class timer {
public:
    void start(std::string msg) {
        msg_ = msg;
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    void stop(){
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
        std::cout << msg_ << " duration: " << duration.count() << std::endl;
    }
private:
    std::string msg_;    
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    //std::chrono::steady_clock::time_point start_;
};

}


std::string random_string(size_t length,
                          const std::string& charset =
                              "0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz");
