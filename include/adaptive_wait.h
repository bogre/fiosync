#ifndef ADAPTIVE_WAIT_H
#define ADAPTIVE_WAIT_H

#include <cstdint>
#include <emmintrin.h>
#include <thread>

class adaptive_wait
{
    std::int_fast32_t repetition{1};

  public:
    adaptive_wait() noexcept = default;
    adaptive_wait(const adaptive_wait&) noexcept = default;
    adaptive_wait(adaptive_wait&&) noexcept = default;
    adaptive_wait& operator=(const adaptive_wait&) noexcept = default;
    adaptive_wait& operator=(adaptive_wait&&) noexcept = default;
        ~adaptive_wait() noexcept = default;
    void wait()
    {
        if(repetition <= 16)
        {
            for(int_fast32_t i = 0; i < repetition; ++i)
                _mm_pause();// extend for platforms without pause
            repetition <<= 1;
        } else
        {
            std::this_thread::yield();
        }
    }
};
#endif
