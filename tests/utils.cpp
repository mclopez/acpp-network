#include "utils.h"
#include "random"

std::string random_string(size_t length,
                          const std::string& charset)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, charset.size() - 1);

    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        result.push_back(charset[dist(rng)]);
    }
    return result;
}
