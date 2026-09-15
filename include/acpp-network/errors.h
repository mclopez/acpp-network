#pragma once

#include <iostream>
#include <system_error>
#include <string>
#include <ios>

namespace acpp::network {

enum class error {
    success = 0,
    data_pending = 1,
    system_error = 2,
    unknown = 3
};


class error_category : public std::error_category {
public:
    const char* name() const noexcept override {
        return "acpp::network::error_category";
    }

    // Translates the numeric value into an explanatory string
    std::string message(int ev) const override {
        switch (static_cast<error>(ev)) {
            case error::success: return "success";
            case error::data_pending: return "data_pending";
            case error::system_error: return "system_error";
            case error::unknown: return "unknown";
            default:                       
                return "Unknown acpp network error";
        }
    }
};

const std::error_category& error_category() noexcept;

bool is_error(const std::error_code& e);

std::error_code make_error_code(error e);


std::error_code last_error() noexcept;

} // namespace acpp::network

// 5. Register your enum with the std library trait (crucial for implicit conversions)
template <>
struct ::std::is_error_code_enum<acpp::network::error> : true_type {};
