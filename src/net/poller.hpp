#pragma once

#include <cstdint>
#include <vector>

// Bitmask of readiness conditions returned by Poller::wait.
enum class PollEvent : std::uint8_t {
    None = 0,
    Read = 1U << 0,
    Write = 1U << 1,
    Error = 1U << 2,
    Hangup = 1U << 3,
};

constexpr PollEvent operator | (PollEvent lhs, PollEvent rhs) noexcept {
    return static_cast<PollEvent>(
        static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs)
    );
}

constexpr bool has_event(PollEvent events, PollEvent flag) noexcept {
    return (static_cast<std::uint32_t>(events) & static_cast<std::uint32_t>(flag)) != 0;
}

enum class Trigger : std::uint8_t {
    Level, 
    Edge,
};

struct ReadyEvent {
    int fd = -1;
    PollEvent events = PollEvent::None;
};

class Poller {
private:
    struct Registration {
        int fd;
        PollEvent events;
        Trigger trigger;
    };

    Registration *find(int fd) noexcept;
    std::vector<Registration> registrations_;
    int native_fd_ = -1;

public:
    Poller();
    ~Poller();

    Poller(const Poller &) = delete;
    Poller &operator = (const Poller &) = delete;

    [[nodiscard]] bool valid() const noexcept;
    
    [[nodiscard]] static constexpr bool supports_edge_trigger() noexcept {
#if defined(__APPLE__) || (defined(__linux__) && defined(USE_EPOLL))
        return true;
#else 
        return false;
#endif
    }

    bool add(int fd, PollEvent events, Trigger trigger = Trigger::Level);
    bool modify(int fd, PollEvent events, Trigger trigger = Trigger::Level);
    bool remove(int fd);
    int wait(std::vector<ReadyEvent> &ready, int timeout_ms);
};
