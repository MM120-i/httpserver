#pragma once

#include "socket.h"

#include <utility>

class Socket {
private:
    int fd_;

public:
    explicit Socket(int socket_fd = -1) noexcept: fd_(socket_fd){}

    ~Socket(){
        reset();
    }

    Socket(const Socket &) = delete;
    Socket &operator = (const Socket&) = delete;
    Socket(Socket &&other) noexcept : fd_(other.release()){}
    
    Socket &operator = (Socket &&other) noexcept {
        if(this != &other){
            reset(other.release());
        }

        return *this;
    }

    [[nodiscard]] int get() const noexcept {
        return fd_;
    }

    [[nodiscard]] bool valid() const noexcept {
        return fd_ >= 0;
    }

    void reset(int socket_fd = -1) noexcept {
        if(fd_ >= 0){
            tcp_close(fd_);
        }

        fd_ = socket_fd;
    } 

    int release() noexcept {
        return std::exchange(fd_, -1);
    }
};