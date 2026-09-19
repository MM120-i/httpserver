#include <stddef.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const char k_request[] = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";

int main(void){
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *list = NULL;

    const int resolve = getaddrinfo("127.0.0.1", "8000", &hints, &list);

    if(resolve != 0){
        // NOTE: getaddrinfo does NOT set errno, so perror() would print garbage here.
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(resolve));
        return EXIT_FAILURE;
    }

    int socket_fd = -1;

    for(struct addrinfo *address = list; address != NULL; address = address->ai_next){
        socket_fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

        if(socket_fd == -1){
            continue;
        }

        if(connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0){
            break;
        }

        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(list);

    if(socket_fd == -1){
        fprintf(stderr, "connect: failed\n");
        return EXIT_FAILURE;
    }

    const size_t want = sizeof(k_request) - 1;
    size_t sent = 0;

    while(sent < want){
        const ssize_t bytes_sent = send(socket_fd, k_request + sent, want - sent, 0);

        if(bytes_sent <= 0){
            perror("send");
            close(socket_fd);
            return EXIT_FAILURE;
        }

        sent += (size_t)bytes_sent;
    }   

    char buf[4096];

    while(1){
        const ssize_t bytes_received = recv(socket_fd, buf, sizeof(buf), 0);

        if(bytes_received == 0){
            break;  // server hung up cleanly: full reply received, this is success
        }

        if(bytes_received < 0){
            perror("recv");
            close(socket_fd);
            return EXIT_FAILURE;
        }

        fwrite(buf, 1, (size_t)bytes_received, stdout);
    }

    close(socket_fd);

    return EXIT_SUCCESS;
}
