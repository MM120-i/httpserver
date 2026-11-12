#include "http/parser.h"
#include "http/request.hpp"
#include "http/response.hpp"
#include "net/socket.h"
#include "net/socket.hpp"
#include "net/poller.hpp"
#include "routing/router.hpp"
#include "concurrency/thread_pool.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <utility>
#include <memory>
#include <vector>

namespace {
    constexpr unsigned short k_port = 8000;
    constexpr int k_backlog = 128;

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

void handle_client(Socket client, const Router &router){
    char peer[64] = {0};

    if(tcp_peer_name(client.get(), peer, sizeof(peer)) == 0)
        std::printf("[%s] connected\n", peer);

    char buf[HTTP_MAX_REQUEST_SIZE + 1];
    const ssize_t bytes_received = tcp_recv(client.get(), buf, sizeof(buf));

    if(bytes_received <= 0)
        return;

    std::fwrite(buf, 1, static_cast<size_t>(bytes_received), stdout);
    std::fflush(stdout);

    http_parse_error_t perr = HTTP_PARSE_OK;
    const std::optional<HttpRequest> maybe = parse_http(std::string_view(buf, static_cast<size_t>(bytes_received)), &perr);
    HttpResponse resp;

    if(!maybe){
        const int status = http_status_for_error(perr);
        resp = make_response(status, reason_phrase(status) + "\n");
        std::fprintf(stderr, "bad request from [%s]: err = %d\n", peer, static_cast<int>(perr));
    }
    else{
        resp = router.route(*maybe);
    }
    
    const std::string raw = serialize_response(resp);

    if(tcp_send_all(client.get(), raw.data(), raw.size()) == -1)
        std::perror("send");
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

    if(tcp_set_nonblocking(listener.get()) == -1){
        std::perror("fcntl");
        return EXIT_FAILURE;
    }

    Poller poller;

    if(!poller.valid()){
        std::perror("poller");
        return EXIT_FAILURE;
    }

    if(!poller.add(listener.get(), PollEvent::Read)){
        std::perror("poller add");
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

    size_t worker_count = std::thread::hardware_concurrency();
    worker_count = std::max<size_t>(worker_count, 4);
    ThreadPool pool(worker_count);
    std::printf("Worker threads: %zu\n", worker_count);

    std::vector<ReadyEvent> ready;

    while (true) {
        if(poller.wait(ready, -1) == -1){
            std::perror("poller wait");
            break;
        }

        for(const ReadyEvent &event : ready){
            if(event.fd == listener.get()){
                while(true){
                    const int client_fd = tcp_accept(listener.get());

                    if(client_fd == -1){
                        if(errno == EAGAIN || errno == EWOULDBLOCK)
                            break;

                        std::perror("accept");
                        break;
                    }

                    if(tcp_set_nonblocking(client_fd) == -1){
                        std::perror("fcntl");
                        tcp_close(client_fd);
                        continue;
                    }

                    if(!poller.add(client_fd, PollEvent::Read)){
                        std::perror("poller add");
                        tcp_close(client_fd);
                        continue;
                    }
                }

                continue;
            }

            if(!poller.remove(event.fd)){
                std::perror("poller remove");
                tcp_close(event.fd);
                continue;
            }

            if(!has_event(event.events, PollEvent::Read)){
                tcp_close(event.fd);
                continue;
            }

            if(tcp_set_blocking(event.fd) == -1){
                std::perror("fcntl");
                tcp_close(event.fd);
                continue;
            }

            auto client = std::make_shared<Socket>(event.fd);

            const bool queued = pool.submit([client, &router]{
                handle_client(std::move(*client), router);
            });

            if(!queued)
                std::fprintf(stderr, "Pool stopped, dropping connections.\n");
        }
    }

    return EXIT_FAILURE;
}
