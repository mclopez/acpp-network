#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <mutex>
#include <format>


//#define LOG_DEBUG(...)    acpp::network::log_debug(std::format(__VA_ARGS__))
#define LOG_DEBUG(...)    {}
#define LOG_ERROR(...)    acpp::network::log_error(std::format(__VA_ARGS__))


namespace acpp::network {

void log_debug(const std::string& msg);
void log_error(const std::string& msg);

template <typename Writer>
class buffered_writer {
public:
    buffered_writer(Writer& writer, size_t buffer_size = 1024)
        : writer_(&writer), buffer_size_(buffer_size)
    {
        buffer_.reserve(buffer_size_);
    }

    size_t write(const char* buffer, size_t len) {        
        LOG_DEBUG("buffered_writer::write(1) initial len: {}", len); 
        if  (!buffer_.empty() || !writer_->write_enabled()) {
            return add_to_buffer(buffer, len);
        }
        //bufer empty here and write enabled
        auto n = writer_->so_write(buffer, len);

        auto remainging_len = len - n;
        LOG_DEBUG("buffered_writer::write(2) n: {} len: {} n: {}", remainging_len, len, n); 
        if (remainging_len > 0) {
            buffer += n;
            LOG_DEBUG("buffered_writer::write(3) remainging_len: {} len: {} n: {}", remainging_len, len, n); 

            n += add_to_buffer(buffer, remainging_len);
        }
        LOG_DEBUG("buffered_writer::write(1) total writen: {}", n); 
        return n;
    }

    size_t add_to_buffer(const char* buffer, size_t len) {
        LOG_DEBUG("buffered_writer::add_to_buffer len: {}", len); 
        size_t free_n = buffer_.capacity() - buffer_.size();
        auto to_buffer = std::min(len, free_n);
        LOG_DEBUG("buffered_writer::add_to_buffer len: {} free_n: {} to_buffer: {}", len, free_n, to_buffer); 
        if (to_buffer > 0)  {
            buffer_.insert(buffer_.end(), buffer, buffer + to_buffer);
        }
        //TODO: improve this check. assert???
        if (buffer_.capacity() != buffer_size_)
            throw std::runtime_error("write: buffer_.capacity() != buffer_size_");
        return to_buffer;
    }

    size_t write_buffered() {
        if (buffer_.empty()) {
            return 0;
        }
        auto n = writer_->so_write(buffer_.data(), buffer_.size());
        if (n > buffer_.size()) {
            throw std::runtime_error("write_buffered: n > buffer_.size()");
        }
        if (n > 0) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + n);
        }
        return n;
    }   

    size_t buffer_size() const {
        return buffer_.capacity();
    }
    size_t data_size() const {
        return buffer_.size();
    }
private:
    //Write write_func_;
    std::vector<char> buffer_;
    const size_t buffer_size_;
    Writer* writer_;
};



} // namespace acpp::network

