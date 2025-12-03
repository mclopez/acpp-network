
#include <iostream>
#include <mutex>

#include <detail/common.h>


namespace acpp::network {

static std::mutex m;

void log_debug(const std::string& msg) {
    std::lock_guard<std::mutex> l(m);
    std::cout  << msg << std::endl;
}

void log_error(const std::string& msg) {
    std::lock_guard<std::mutex> l(m);
    std::cout  << msg << std::endl;
}


} // namespace acpp::network
