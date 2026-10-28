#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    constexpr int k_threads = 8;
    constexpr int k_increments = 1'000'000;

    uint64_t g_racy = 0;
    uint64_t g_mutex_guarded = 0;

    std::mutex g_mutex;
    std::atomic<uint64_t> g_atomic{0};

    template <typename Body>
    void run_threads(Body body){
        std::vector<std::thread> threads;
        threads.reserve(k_threads);

        for(size_t i = 0; i < k_threads; i++)
            threads.emplace_back(body);

        for(auto &thread : threads)
            thread.join();
    }

    template <typename Body>
    long long time_ms(Body body){
        const auto start = std::chrono::steady_clock::now();
        body();
        const auto end = std::chrono::steady_clock::now();

        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
}

int main(){
    const uint64_t expected = static_cast<uint64_t>(k_threads) * static_cast<uint64_t>(k_increments);
    
    const long long racy_ms = time_ms([]{
        run_threads([]{
            for(size_t i = 0; i < k_increments; i++)
                g_racy++;
        });
    });

    const long long mutex_ms = time_ms([]{
        run_threads([]{
            for(size_t i = 0; i < k_increments; i++){
                const std::lock_guard<std::mutex> lock(g_mutex);
                g_mutex_guarded++;
            }
        });
    });

    const long long atomic_ms = time_ms([]{
        run_threads([]{
            for(size_t i = 0; i < k_increments; i++)
                g_atomic.fetch_add(1, std::memory_order_relaxed);
        });
    });

    std::printf("expected %llu\n", static_cast<unsigned long long>(expected));
    std::printf("racy     %llu  (%lld ms)\n", static_cast<unsigned long long>(g_racy), racy_ms);
    std::printf("mutex    %llu  (%lld ms)\n", static_cast<unsigned long long>(g_mutex_guarded), mutex_ms);
    std::printf("atomic   %llu  (%lld ms)\n", static_cast<unsigned long long>(g_atomic.load()), atomic_ms);

    return 0;
}
