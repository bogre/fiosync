#ifndef SPIN_WAIT_H
#define SPIN_WAIT_H

class spin_wait {

void wait()
{
    if(current <= max_repetitions) // Start active spinning phase
    {
        for(int_fast32_t i = 0; i < current; ++i)
            pause_processor();
        current <<= 1; // double the amount of active CPU waiting cycles.
    } else             // Start passive spinning phase
    {
        std::this_thread::yield();
    }
}
};
#endif
