#pragma once

#include <string>
#include <iostream>

namespace acpp::network {

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
