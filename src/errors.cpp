

#if defined(_WIN32)
  #include <winsock2.h>
#else
  #include <cerrno>
#endif

#include <acpp-network/errors.h>


namespace acpp::network {

const std::error_category& error_category() noexcept {
    static class error_category instance;
    return instance;
}

bool is_error(const std::error_code& e) {
    if (e.category() != error_category())
        return false;
    return (static_cast<error>(e.value()) > error::data_pending);
}

std::error_code make_error_code(error e) {
    return {static_cast<int>(e), error_category()};
}

std::error_code last_error() noexcept {
#if defined(_WIN32)
    return std::error_code(::WSAGetLastError(), std::system_category());
#else
    return std::error_code(errno, std::system_category());
#endif    
}




} // namespace acpp::network     