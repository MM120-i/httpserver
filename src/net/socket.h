#pragma once

//#include <cstddef>
#include <stddef.h>


#ifdef __cplusplus

extern "C" {
#endif

    int tcp_create(void);
    int tcp_set_reuseaddr(int socket_fd);
    int tcp_bind(int socket_fd, unsigned short port);
    int tcp_listen(int socket_fd, int backlog);
    int tcp_accept(int listener_fd);
    int tcp_send_all(int socket_fd, const char *buf, size_t len);
    int tcp_close(int socket_fd);

#ifdef __cplusplus
}
#endif