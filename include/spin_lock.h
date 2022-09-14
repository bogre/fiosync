#ifndef SPIN_LOCK_H
#define SPIN_LOCK_H

#include <array>
#include <atomic>
#include <emmintrin.h>
#include <thread>

struct spin_lock
{
    void lock() noexcept
    {
        constexpr std::array iterations = {5, 10, 3000};
        for(int i = 0; i < iterations[0]; ++i) {
            if(try_lock())
                return;
        }

        for(int i = 0; i < iterations[1]; ++i) {
            if(try_lock())
                return;

            _mm_pause();
        }

        while(true) {
            for(int i = 0; i < iterations[2]; ++i) {
                if(try_lock())
                    return;

                _mm_pause();
                _mm_pause();
                _mm_pause();
                _mm_pause();
                _mm_pause();
                _mm_pause();
                _mm_pause();
                _mm_pause();
                _mm_pause();
                _mm_pause();
            }
            std::puts("yield");
            std::this_thread::yield();
        }
    }

    bool try_lock() noexcept
    {
        return !flag.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        flag.clear(std::memory_order_release);
    }

  private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};
#endif
