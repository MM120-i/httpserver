#include "http/parser.h"
#include "net/socket.h"
#include "net/socket.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <sys/socket.h>

namespace {
    constexpr unsigned short k_port = 8000;
    constexpr int k_backlog = 16;

    const std::string_view k_response = "HTTP/1.1 200 OK\r\n"
                                        "Content-Length: 13\r\n"
                                        "Content-Type: text/plain\r\n"
                                        "Connection: close\r\n"
                                        "\r\n"
                                        "Hello, world!";
} 

constexpr std::string_view k400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
constexpr std::string_view k413 = "HTTP/1.1 413 Content Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

// Full response builder arrives in Phase 5; these constants are the stopgap.
constexpr std::string_view status_response(http_parse_error_t err) {
    switch (err) {
        case HTTP_PARSE_OK:
            return k_response;
        case HTTP_PARSE_BODY_TOO_LARGE:
        case HTTP_PARSE_REQUEST_TOO_LARGE:
        case HTTP_PARSE_HEADER_TOO_LARGE:
        case HTTP_PARSE_URI_TOO_LONG:
            return k413;
        default:
            return k400;  
    }
}

int main() {
    Socket listener(tcp_create());

    if (!listener.valid()) {
        std::perror("socket");
        return EXIT_FAILURE;
    }

    if (tcp_set_reuseaddr(listener.get()) == -1 || tcp_bind(listener.get(), k_port) == -1) {
        std::perror("bind");
        return EXIT_FAILURE;
    }

    if (tcp_listen(listener.get(), k_backlog) == -1) {
        std::perror("listen");
        return EXIT_FAILURE;
    }

    int snd_buf = 0;
    int rcv_buf = 0;

    if(tcp_buffer_sizes(listener.get(), &snd_buf, &rcv_buf) == 0)
        std::printf("socket buffers: send = %d recv = %d bytes\n", snd_buf, rcv_buf);

    std::printf("Listening on 0.0.0.0:%d (Ctrl + C to stop)\n", k_port);

    while (true) {
        Socket client(tcp_accept(listener.get()));

        if (!client.valid()) {
            std::perror("accept");
            continue;
        }

        char peer[64] = {0};

        if(tcp_peer_name(client.get(), peer, sizeof(peer)) == 0)
            std::printf("[%s] connected\n", peer);

        char buf[HTTP_MAX_REQUEST_SIZE + 1];
        const ssize_t bytes_received = recv(client.get(), buf, sizeof(buf), 0);

        if (bytes_received <= 0) 
            continue;

        std::fwrite(buf, 1, (size_t)bytes_received, stdout);
        std::fflush(stdout);

        http_request_t request;
        const http_parse_error_t err = http_parse(buf, (size_t)bytes_received, &request);

        if (err != HTTP_PARSE_OK)
            std::fprintf(stderr, "bad request from [%s]: err=%d\n", peer, (int)err);

        const std::string_view response = status_response(err);

        if (tcp_send_all(client.get(), response.data(), response.size()) == -1)
            std::perror("send");
    }

    return EXIT_SUCCESS;
}