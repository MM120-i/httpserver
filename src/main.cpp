#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <sys/socket.h>  

#include "net/socket.h"
#include "net/socket.hpp"

namespace {
    constexpr unsigned short k_port = 8000;
    constexpr int k_backlog = 16;
    constexpr int k_buff_size = 4090;

    const std::string_view k_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 13\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"  
        "Hello, world!";
}

int main(){
    Socket listener(tcp_create());

    if(!listener.valid()){
        std::perror("socket");
        return EXIT_FAILURE;
    }

    if(tcp_set_reuseaddr(listener.get()) == -1 || tcp_bind(listener.get(), k_port) == -1){
        std::perror("bind");
        return EXIT_FAILURE;
    }

    if(tcp_listen(listener.get(), k_backlog) == -1){
        std::perror("listen");
        return EXIT_FAILURE;
    }

    std::printf("Listening on 0.0.0.0:%d (Ctrl + C to stop)\n", k_port);

    while(true){
        Socket client(tcp_accept(listener.get()));

        if(!client.valid()){
            std::perror("accept");
            continue;
        }

        char buf[k_buff_size];
        const ssize_t bytes_received = recv(client.get(), buf, sizeof(buf), 0);

        if(bytes_received <= 0){
            continue;
        }

        std::fwrite(buf, 1, (size_t)bytes_received, stdout);
        std::fflush(stdout);

        if(tcp_send_all(client.get(), k_response.data(), k_response.size()) == -1){
            std::perror("send");
        }
    }

    return EXIT_SUCCESS;
}