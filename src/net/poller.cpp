#include "net/poller.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

#elif defined(__linux__) && defined(USE_EPOLL)
#include <sys/epoll.h>
#else
#include <poll.h>
#endif

namespace {
#if defined (__APPLE__)
bool submit_kevent(int kqueue_fd, const struct kevent *changes, int count, bool ignore_missing){
    int result = 0;

    do {
        result = kevent(kqueue_fd, changes, count, nullptr, 0, nullptr);
    } while (result == -1 && errno == EINTR);

    if(result == -1 && ignore_missing && errno == ENOENT)
        return true;

    return result != -1;
}

bool control_kevent(int kqueue_fd, int fd, PollEvent events, Trigger trigger, bool add){
    struct kevent changes[2];
    int count = 0;
    const int base_flags = add ? (EV_ADD | EV_ENABLE) : EV_DELETE;
    const int edge_flags = (add && trigger == Trigger::Edge) ? EV_CLEAR : 0;

    if(has_event(events, PollEvent::Read))
        EV_SET(&changes[count++], static_cast<std::uintptr_t>(fd), EVFILT_READ, base_flags | edge_flags, 0, 0, nullptr);
    
    if(has_event(events, PollEvent::Write))
        EV_SET(&changes[count++], static_cast<std::uintptr_t>(fd), EVFILT_WRITE, base_flags | edge_flags, 0, 0, nullptr);

    if(count == 0)
        return true;

    return submit_kevent(kqueue_fd, changes, count, !add);
}

#elif defined(__linux__) && defined(USE_EPOLL)
std::uint32_t epoll_mask(PollEvent events, Trigger trigger) {
    std::uint32_t mask = 0;

    if (has_event(events, PollEvent::Read))
        mask |= EPOLLIN;

    if (has_event(events, PollEvent::Write))
        mask |= EPOLLOUT;

#ifdef EPOLLRDHUP
    mask |= EPOLLRDHUP;
#endif

    if (trigger == Trigger::Edge)
        mask |= EPOLLET;

    return mask;
}

int epoll_control(int epoll_fd, int operation, int fd, struct epoll_event *event) {
    int result = 0;

    do {
        result = epoll_ctl(epoll_fd, operation, fd, event);
    } while (result == -1 && errno == EINTR);

    return result;
}

#else
short poll_mask(PollEvent events){
    short mask = 0;

    if(has_event(events, PollEvent::Read))
        mask = static_cast<short>(mask | POLLIN);

    if(has_event(events, PollEvent::Write))
        mask = static_cast<short>(mask | POLLOUT);

    return mask;
}

#endif
}

Poller::Poller(){
#if defined (__APPLE__)
    native_fd_ = kqueue();

    if(native_fd_ >= 0){
        const int flags = fcntl(native_fd_, F_GETFD, 0);

        if(flags != -1)
            static_cast<void>(fcntl(native_fd_, F_SETFD, flags | FD_CLOEXEC));
    }
#elif defined(__linux__) && defined (USE_EPOLL)
    native_fd_ = epoll_create1(EPOLL_CLOEXEC);
#endif
}

Poller::~Poller() {
    if (native_fd_ >= 0)
        static_cast<void>(close(native_fd_));
}

bool Poller::valid() const noexcept {
#if defined (__APPLE__) || (defined (__linux__) && defined (USE_EPOLL))
    return native_fd_ >= 0;
#else
    return true;
#endif
}

Poller::Registration *Poller::find(int fd) noexcept {
    for(Registration &registration : registrations_)
        if(registration.fd == fd)
            return &registration;

    return nullptr;
}

bool Poller::add(int fd, PollEvent events, Trigger trigger){
    if(!valid())
        return false;

    if(fd < 0){
        errno = EBADF;
        return false;
    }

    if(events == PollEvent::None){
        errno = EINVAL;
        return false;
    }

    if(find(fd) != nullptr){
        errno = EEXIST;
        return false;
    }

#if defined (__APPLE__)
    if(!control_kevent(native_fd_, fd, events, trigger, true))
        return false;
#elif defined (__linux__) && defined(USE_EPOLL)
    struct epoll_event event = {};
    event.events = epoll_mask(events, trigger);
    event.data.fd = fd;

    if (epoll_control(native_fd_, EPOLL_CTL_ADD, fd, &event) == -1)
        return false;
#else
    if(trigger == Trigger::Edge){
        errno = ENOTSUP;
        return false;
    }
#endif
    registrations_.push_back({fd, events, trigger});
    return true;
}

bool Poller::modify(int fd, PollEvent events, Trigger trigger){
    if(!valid())
        return false;

    if(events == PollEvent::None){
        errno = EINVAL;
        return false;
    }

    Registration *registration = find(fd);

    if(registration == nullptr){
        errno = ENOENT;
        return false;
    }

#if defined(__APPLE__)
    if(registration->events == events && registration->trigger == trigger)
        return true;

    const Registration old = *registration;

    if(!control_kevent(native_fd_, fd, old.events, old.trigger, false))
        return false;

    if(!control_kevent(native_fd_, fd, events, trigger, true)){
        static_cast<void>(control_kevent(native_fd_, fd, old.events, old.trigger, true));
        return false;
    }

#elif defined(__linux__) && defined(USE_EPOLL)
    struct epoll_event event = {};
    event.events = epoll_mask(events, trigger);
    event.data.fd = fd;

    if (epoll_control(native_fd_, EPOLL_CTL_MOD, fd, &event) == -1)
        return false;
#else
    if (trigger == Trigger::Edge) {
        errno = ENOTSUP;
        return false;
    }
#endif
    registration->events = events;
    registration->trigger = trigger;

    return true;
}

bool Poller::remove(int fd){
    if(!valid())
        return false;

    Registration *registration = find(fd);

    if(registration == nullptr){
        errno = ENOENT;
        return false;
    }

#if defined (__APPLE__)
    if(!control_kevent(native_fd_, fd, registration->events, registration->trigger, false))
        return false;
#elif defined(__linux__) && defined(USE_EPOLL)
    if (epoll_control(native_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1)
        return false;
#endif
    registrations_.erase(registrations_.begin() + (registration - registrations_.data()));
    return true;
}

int Poller::wait(std::vector<ReadyEvent> &ready, int timeout_ms){
    if(!valid())
        return -1;

    ready.clear();

#if defined (__APPLE__)
    struct timespec timeout = {};
    struct timespec *timeout_ptr = nullptr;

    if(timeout_ms >= 0){
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_nsec = static_cast<long>(timeout_ms % 1000) * 1000000L;
        timeout_ptr = &timeout;
    }

    struct kevent events[64];
    int count = 0;

    do {
        count = kevent(native_fd_, nullptr, 0, events, 64, timeout_ptr);
    } while(count == -1 && errno == EINTR);

    if(count == -1)
        return -1;

    for(int i = 0; i < count; i++){
        PollEvent result = PollEvent::None;

        if(events[i].filter == EVFILT_READ)
            result = result | PollEvent::Read;

        if(events[i].filter == EVFILT_WRITE)
            result = result | PollEvent::Write;

        if((events[i].flags & EV_ERROR) != 0)
            result = result | PollEvent::Error;

        if((events[i].flags & EV_EOF) != 0)
            result = result | PollEvent::Hangup;

        ready.push_back({static_cast<int>(events[i].ident), result});
    }

#elif defined(__linux__) && defined(USE_EPOLL)
    struct epoll_event events[64];
    int count = 0;

    do {
        count = epoll_wait(native_fd_, events, 64, timeout_ms);
    } while (count == -1 && errno == EINTR);

    if (count == -1)
        return -1;

    for (int i = 0; i < count; i++) {
        PollEvent result = PollEvent::None;

        if ((events[i].events & (EPOLLIN | EPOLLPRI)) != 0)
            result = result | PollEvent::Read;

        if ((events[i].events & EPOLLOUT) != 0)
            result = result | PollEvent::Write;

        if ((events[i].events & EPOLLERR) != 0)
            result = result | PollEvent::Error;

#ifdef EPOLLRDHUP
        if ((events[i].events & (EPOLLHUP | EPOLLRDHUP)) != 0)
            result = result | PollEvent::Hangup;
#else
        if ((events[i].events & EPOLLHUP) != 0)
            result = result | PollEvent::Hangup;
#endif
        ready.push_back({events[i].data.fd, result});
    }
#else
    std::vector<struct pollfd> poll_fds;
    poll_fds.reserve(registrations_.size());

    for(const Registration &registration : registrations_) {
        struct pollfd entry = {};
        entry.fd = registration.fd;
        entry.events = poll_mask(registration.events);
        poll_fds.push_back(entry);
    }

    int count = 0;

    do {
        count = poll(poll_fds.data(), static_cast<nfds_t>(poll_fds.size()), timeout_ms);
    }while (count == -1 && errno == EINTR);

    if(count == -1)
        return -1;

    for(std::size_t i = 0; i < poll_fds.size(); i++) {
        if (poll_fds[i].revents == 0)
            continue;

        PollEvent result = PollEvent::None;

        if ((poll_fds[i].revents & (POLLIN | POLLPRI)) != 0)
            result = result | PollEvent::Read;

        if ((poll_fds[i].revents & POLLOUT) != 0)
            result = result | PollEvent::Write;

        if ((poll_fds[i].revents & (POLLERR | POLLNVAL)) != 0)
            result = result | PollEvent::Error;

        if ((poll_fds[i].revents & POLLHUP) != 0)
            result = result | PollEvent::Hangup;

        ready.push_back({registrations_[i].fd, result});
    }
#endif
    return static_cast<int>(ready.size());
}
