#include "socket.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int tcp_create(void) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        return -1;
    }

#ifdef SO_NOSIGPIPE
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif

    return fd;
}

int tcp_set_reuseaddr(int socket_fd) {
    const int one = 1;
    return setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int tcp_bind(int socket_fd, unsigned short port) {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_fd, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
        return -1;
    }

    return 0;
}

int tcp_listen(int socket_fd, int backlog) {
    return listen(socket_fd, backlog);
}

int tcp_accept(int listener_fd) {
    while (1) {
        const int client = accept(listener_fd, NULL, NULL);

        if (client >= 0) {
            return client;
        }

        if (errno == EINTR) {
            continue;
        }

        return -1;
    }
}

int tcp_send_all(int socket_fd, const char* buf, size_t len) {
    size_t sent = 0;

    while (sent < len) {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0; 
#endif
        const ssize_t n = send(socket_fd, buf + sent, len - sent, flags);

        if (n <= 0) {
            return -1;
        }

        sent += (size_t)n;
    }

    return 0;
}

int tcp_close(int socket_fd) {
    return close(socket_fd);
}

int tcp_peer_name(int fd, char *buf, int len){
    struct sockaddr_in addr = {0};
    socklen_t addr_len = sizeof(addr);

    if(getpeername(fd, (struct sockaddr *)&addr, &addr_len) == -1){
        return -1;
    }

    char ip[INET_ADDRSTRLEN] = {0};

    if(inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)) == NULL){
        return -1;
    }

    snprintf(buf, len, "%s:%d", ip, ntohs(addr.sin_port));
    return 0;
}

int tcp_buffer_sizes(int fd, int *send_size, int *recv_size){
    socklen_t opt_len = sizeof(*send_size);

    if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, send_size, &opt_len) == -1){
        return -1;
    }

    opt_len = sizeof(*recv_size);

    if(getsockopt(fd, SOL_SOCKET, SO_RCVBUF, recv_size, &opt_len) == -1){
        return -1;
    }

    return 0;
}
