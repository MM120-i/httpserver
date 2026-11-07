#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "net/socket.h"

TEST_CASE("nonblocking recv reports EAGAIN", "[socket]"){
    int fds[2];
    
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    REQUIRE(tcp_set_nonblocking(fds[0]) == 0);

    char buf[16];
    errno = 0;

    const ssize_t read_result = tcp_recv(fds[0], buf, sizeof(buf));
    const int read_error = errno;

    CHECK(read_result == -1);
    CHECK((read_error == EAGAIN || read_error == EWOULDBLOCK));
    
    REQUIRE(send(fds[1], "hi", 2, 0) == 2);
    
    CHECK(tcp_recv(fds[0], buf, sizeof(buf)) == 2);
    CHECK(buf[0] == 'h');
    CHECK(buf[1] == 'i');

    tcp_close(fds[0]);
    tcp_close(fds[1]);
}

TEST_CASE("tcp_set_blocking clears O_NONBLOCK", "[socket]"){
    int fds[2];

    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    REQUIRE(tcp_set_nonblocking(fds[0]) == 0);

    const int nonblocking_flags = fcntl(fds[0], F_GETFL, 0);
    REQUIRE(nonblocking_flags != -1);
    CHECK((nonblocking_flags & O_NONBLOCK) != 0);

    REQUIRE(tcp_set_blocking(fds[0]) == 0);

    const int blocking_flags = fcntl(fds[0], F_GETFL, 0);
    REQUIRE(blocking_flags != -1);
    CHECK((blocking_flags & O_NONBLOCK) == 0);

    tcp_close(fds[0]);
    tcp_close(fds[1]);
}
