#pragma once

#include <string>
#include <iostream>
#include <chrono>

namespace acpp::network {

class timer {
public:
    timer(std::string msg= "") {
        start(msg);
    }
    ~timer(){
        if (active_) stop();
    }
    void start(std::string msg) {
        active_ = true;
        msg_ = msg;
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    void stop(){
        active_ = false;
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
        std::cout << msg_ << " duration: " << duration.count() << " us"<< std::endl;
    }
private:
    bool active_ = false;
    std::string msg_;    
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    //std::chrono::steady_clock::time_point start_;
};

}
