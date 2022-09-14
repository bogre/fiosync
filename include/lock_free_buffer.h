#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "spin_lock.h"

#include <array>
#include <atomic>
#include <concepts>
#include <new>
#include <stdexcept>
#include <utility>

constexpr inline std::size_t default_storage_size = 32768;
template<std::semiregular ValueType, std::size_t BUFFER_SIZE = default_storage_size>
struct RingBuffer
{
    using size_type = std::array<ValueType, BUFFER_SIZE>;
    RingBuffer() = default;
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    auto operator=(const RingBuffer&) = delete;
    auto operator=(RingBuffer&&) = delete;
    ~RingBuffer() = default;
    template<typename Type>
        requires std::convertible_to<Type, ValueType>
    void push(Type&& value)
    {
        for(;;) {
            auto push_result = try_push(std::forward<Type>(value));
            if(push_result == -1 /*overflow*/)
                throw std::runtime_error("invalid buffer state");
            if(push_result == 0 /*success*/)
                return;
        }
    }
    ValueType pop()
    {
        ValueType value{};
        for(;;) {
            if(!writer_waiting.load()) {
                auto pop_status = try_pop(value);
                if(pop_status == -1 /*overflow*/)
                throw std::runtime_error("invalid buffer state");
                else if(pop_status == 0/*success*/)
                    return value;
            }
        }
    }
    template <typename Type>
        requires std::convertible_to<Type, ValueType>
    int try_push(Type &&);

    int try_pop(ValueType &);

  private:
    std::array<ValueType, BUFFER_SIZE> buffer;

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    using std::hardware_destructive_interference_size;
#else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    constexpr static std::size_t hardware_constructive_interference_size = 64;
    constexpr static std::size_t hardware_destructive_interference_size = 64;
#endif

    alignas(hardware_destructive_interference_size) std::atomic<size_type> pop_front_position{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pending_pop_front_position{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> push_back_position{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pending_push_back_position{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> ring_buffer_capacity{0};
    alignas(hardware_destructive_interference_size) std::atomic_bool writer_waiting{false};
    alignas(hardware_destructive_interference_size) bool closed{false};
};

#endif
