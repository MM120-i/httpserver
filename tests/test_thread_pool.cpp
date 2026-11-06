#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <cstddef>

#include "concurrency/thread_pool.hpp"

TEST_CASE("thread pool runs every submitted task", "[thread_pool]"){
    ThreadPool pool(4);
    std::atomic<int> count{0};

    for(size_t i = 0; i < 200; i++){
        pool.submit([&count]{
            count.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.shutdown();

    CHECK(count.load() == 200);
}

TEST_CASE("submit is rejected after shutdown", "[thread_pool]"){
    ThreadPool pool(2);
    pool.shutdown();

    CHECK_FALSE(pool.submit([]{}));
}

TEST_CASE("shutdown is idempotent", "[thread_pool]"){
    ThreadPool pool(2);
    pool.shutdown();
    pool.shutdown();

    CHECK(true);
}

TEST_CASE("tasks actually execute", "[thread_pool]"){
    ThreadPool pool(2);
    std::atomic<bool> ran{false};

    pool.submit([&ran]{
        ran.store(true, std::memory_order_relaxed);
    });

    pool.shutdown();
    CHECK(ran.load());
}
