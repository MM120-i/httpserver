#include "socket.h"

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int tcp_create(void){
    const int fd = socket(AF_INET, SOCK_STREAM, 0);

    if(fd == -1){
        return -1;
    }

#ifdef SO_NOSIGPIPE
    // macOS/BSD have no MSG_NOSIGNAL: suppress SIGPIPE per-socket instead.
    // (A write to a closed socket would otherwise kill the process.)
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif

    return fd;
}

int tcp_set_reuseaddr(int socket_fd){
    const int one = 1;
    return setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int tcp_bind(int socket_fd, unsigned short port){
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(socket_fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1){
        return -1;
    }

    return 0;
}

int tcp_listen(int socket_fd, int backlog){
    return listen(socket_fd, backlog);
}

int tcp_accept(int listener_fd){
    while(1){
        const int client = accept(listener_fd, NULL, NULL);

        if(client >= 0){
            return client;
        }

        if(errno == EINTR){
            continue;
        }

        return -1;
    }
}

int tcp_send_all(int socket_fd, const char *buf, size_t len){
    size_t sent = 0;

    while(sent < len){
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;  // Linux: suppress SIGPIPE for this send.
#else
        const int flags = 0;  // macOS: already covered by SO_NOSIGPIPE in tcp_create().
#endif
        const ssize_t n = send(socket_fd, buf + sent, len - sent, flags);

        if(n <= 0){
            return -1;
        }

        sent += (size_t)n;
    }

    return 0;
}

int tcp_close(int socket_fd){
    return close(socket_fd);
}
