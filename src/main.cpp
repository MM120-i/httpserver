#include "http/parser.h"
#include "http/request.hpp"
#include "http/response.hpp"
#include "net/socket.h"
#include "net/socket.hpp"
#include "routing/router.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <sys/socket.h>

namespace {
    constexpr unsigned short k_port = 8000;
    constexpr int k_backlog = 16;

    HttpResponse home_handler(const HttpRequest &){
        return make_response(200, "Hello, world!");
    }

    HttpResponse status_handler(const HttpRequest &){
        return make_response(200, R"({"status":"ok"})", "application/json");
    }

    HttpResponse list_users_handler(const HttpRequest &){
        return make_response(200, R"([{"id":1,"name":"ada"}])", "application/json");
    }

    HttpResponse create_user_handler(const HttpRequest &req){
        return make_response(201, req.body, "application/json");
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

    Router router;

    router.get("/", home_handler);
    router.get("/status", status_handler);
    router.get("/users", list_users_handler);
    router.post("/users", create_user_handler);

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

        http_parse_error_t perr = HTTP_PARSE_OK;
        const std::optional<HttpRequest> maybe = parse_http(std::string_view(buf, static_cast<size_t>(bytes_received)), &perr);
        HttpResponse resp;

        if(!maybe){
            const int status = http_status_for_error(perr);
            resp = make_response(status, reason_phrase(status) + "\n");
            std::fprintf(stderr, "bad request from [%s]: err = %d\n", peer, static_cast<int>(perr));
        }
        else {
            resp = router.route(*maybe);
        }

        const std::string raw = serialize_response(resp);

        if(tcp_send_all(client.get(), raw.data(), raw.size()) == -1)
            std::perror("send");
    }

    return EXIT_SUCCESS;
}
