#include <catch2/catch_test_macros.hpp>

#include "net/poller.hpp"
#include "net/socket.h"
#include "net/socket.hpp"

#include <sys/socket.h>
#include <cerrno>
#include <vector>

namespace {
    struct SocketPair {
        Socket first;
        Socket second;

        SocketPair(){
            int fds[2] = {-1, -1};
            REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
            first.reset(fds[0]);
            second.reset(fds[1]);
        }
    };
}

TEST_CASE("poller rejects negative descriptors", "[poller]"){
    Poller poller;
    REQUIRE(poller.valid());

    errno = 0;
    CHECK_FALSE(poller.add(-1, PollEvent::Read));
    CHECK(errno == EBADF);
}

TEST_CASE("poller reports readable and writable sockets", "[poller]"){
    SocketPair pair;
    Poller poller;
    REQUIRE(poller.valid());
    REQUIRE(poller.add(pair.first.get(), PollEvent::Read));

    std::vector<ReadyEvent> ready;
    CHECK(poller.wait(ready, 0) == 0);

    REQUIRE(send(pair.second.get(), "x", 1, 0) == 1);
    REQUIRE(poller.wait(ready, 1000) == 1);
    REQUIRE(ready.size() == 1);

    CHECK(ready[0].fd == pair.first.get());
    CHECK(has_event(ready[0].events, PollEvent::Read));

    char byte = 0;
    CHECK(tcp_recv(pair.first.get(), &byte, 1) == 1);
    CHECK(poller.wait(ready, 0) == 0);

    REQUIRE(poller.modify(pair.first.get(), PollEvent::Write));
    REQUIRE(poller.wait(ready, 1000) == 1);
    CHECK(has_event(ready[0].events, PollEvent::Write));
}

TEST_CASE("poller rejects duplicate registration", "[poller]"){
    SocketPair pair;
    Poller poller;
    REQUIRE(poller.add(pair.first.get(), PollEvent::Read));

    errno = 0;
    CHECK_FALSE(poller.add(pair.first.get(), PollEvent::Read));
    CHECK(errno == EEXIST);
}

TEST_CASE("poller remove stops watching the socket", "[poller]"){
    SocketPair pair;
    Poller poller;
    REQUIRE(poller.add(pair.first.get(), PollEvent::Read));
    REQUIRE(poller.remove(pair.first.get()));
    CHECK_FALSE(poller.remove(pair.first.get()));

    REQUIRE(send(pair.second.get(), "x", 1, 0) == 1);

    std::vector<ReadyEvent> ready;
    CHECK(poller.wait(ready, 0) == 0);
}

TEST_CASE("poller reports peer close", "[poller]"){
    SocketPair pair;
    Poller poller;
    REQUIRE(poller.add(pair.first.get(), PollEvent::Read));

    pair.second.reset();

    std::vector<ReadyEvent> ready;
    REQUIRE(poller.wait(ready, 1000) == 1);
    REQUIRE(ready.size() == 1);
    CHECK(ready[0].fd == pair.first.get());
    
    CHECK((has_event(ready[0].events, PollEvent::Read) || 
        has_event(ready[0].events, PollEvent::Hangup)));

    char byte = 0;
    CHECK(tcp_recv(pair.first.get(), &byte, 1) == 0);
}

TEST_CASE("edge trigger needs new data", "[poller]"){
    if(!Poller::supports_edge_trigger())
        SKIP("this backend is level triggered only");

    SocketPair pair;
    REQUIRE(tcp_set_nonblocking(pair.first.get()) == 0);
    REQUIRE(tcp_set_nonblocking(pair.second.get()) == 0);

    Poller poller;
    REQUIRE(poller.add(pair.first.get(), PollEvent::Read, Trigger::Edge));

    REQUIRE(send(pair.second.get(), "ab", 2, 0) == 2);

    std::vector<ReadyEvent> ready;
    REQUIRE(poller.wait(ready, 1000) == 1);
    CHECK(has_event(ready[0].events, PollEvent::Read));

    char byte = 0;
    REQUIRE(tcp_recv(pair.first.get(), &byte, 1) == 1);
    CHECK(poller.wait(ready, 50) == 0);

    REQUIRE(send(pair.second.get(), "c", 1, 0) == 1);
    REQUIRE(poller.wait(ready, 1000) == 1);
    CHECK(has_event(ready[0].events, PollEvent::Read));

    char rest[2] = {0};
    REQUIRE(tcp_recv(pair.first.get(), rest, sizeof(rest)) == 2);
    CHECK(rest[0] == 'b');
    CHECK(rest[1] == 'c');
}
