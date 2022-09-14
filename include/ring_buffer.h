#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "adaptive_wait.h"

#include <atomic>
#include <bit>
#include <cmath>
#include <concepts>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

constexpr inline std::size_t default_storage_size = 32768;
#ifndef __cpp_lib_hardware_interference_size
namespace std {
inline constexpr std::size_t hardware_destructive_interference_size = 64;
inline constexpr std::size_t hardware_constructive_interference_size = 64;
} // namespace std
#endif

template<std::semiregular ValueType>
class RingBuffer
{
    using BufferType = std::vector<ValueType>;
    using SizeType = typename BufferType::size_type;
    enum op_result
    {
        ok = 0,
        error_closed,
        op_failed_buffer_empty,
        op_failed_buffer_full
    };
    static constexpr std::size_t circular_index_mask = default_storage_size - 1;

    BufferType buffer;
    alignas(std::hardware_destructive_interference_size) std::shared_mutex mutable mutex{};
    alignas(std::hardware_destructive_interference_size) std::atomic<SizeType> pop_position{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<SizeType> pending_pop_position{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<SizeType> push_position{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<SizeType> pending_push_position{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<SizeType> ring_buffer_capacity{0};
    alignas(std::hardware_destructive_interference_size) std::atomic_bool push_waiting{false};
    alignas(std::hardware_destructive_interference_size) bool closed{false};

  public:
    explicit RingBuffer(SizeType buffer_capacity)
    {
        buffer.resize(buffer_capacity);
        ring_buffer_capacity = std::bit_ceil(buffer.size());
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    auto operator=(const RingBuffer&) = delete;
    auto operator=(RingBuffer&&) = delete;
    ~RingBuffer() = default;

    void close()
    {
        if(push_waiting.exchange(true))
            return;
        std::unique_lock write_lock{mutex};
        closed = true;
        push_waiting.store(false);
    }

    SizeType size() const noexcept
    {
        std::unique_lock write_lock(mutex);
        if(ring_position(pop_position) <= ring_position(push_position)) {
            return ring_position(push_position) - ring_position(pop_position);
        } else {
            return buffer.size() - (ring_position(pop_position) - ring_position(push_position));
        }
    }

    template<typename PushedValueType>
        requires std::convertible_to<PushedValueType, ValueType>
    void push(PushedValueType&& value)
    {
        adaptive_wait await{};
        for(;;) {
            auto push_result = try_push(std::forward<PushedValueType>(value));
            if(push_result == error_closed)
                throw std::runtime_error("invalid buffer state");
            if(push_result == ok)
                return;
            await.wait();
        }
    }

    ValueType pop()
    {
        adaptive_wait await{};
        ValueType value{};
        for(;;) {
            if(!push_waiting.load()) {
                auto pop_status = try_pop(value);
                if(pop_status == error_closed)
                    throw std::runtime_error("invalid buffer state");
                else if(pop_status == ok)
                    return value;
            }
            await.wait();
        }
    }

    template<typename PushedValueType>
        requires std::convertible_to<PushedValueType, ValueType>
    op_result try_push(PushedValueType&& value)
    {
        std::shared_lock push_lock(mutex);
        adaptive_wait await{};
        if(closed)
            return error_closed;
        SizeType local_pending_push_position = pending_push_position;

        while(true) {
            SizeType next_local_push_position = ring_next_pos(local_pending_push_position);
            SizeType local_pop_position = pop_position;
            if(is_full(local_pop_position, next_local_push_position))
                break;
            if(pending_push_position.compare_exchange_weak(local_pending_push_position,
                                                           next_local_push_position)) {
                auto push_it = std::ranges::begin(buffer) + ring_position(local_pending_push_position);
                *push_it = std::forward<PushedValueType>(value);
                {
                    adaptive_wait await{};
                    SizeType acquired_slot = local_pending_push_position;
                    while(!push_position.compare_exchange_weak(acquired_slot,
                                                               next_local_push_position)) {
                        acquired_slot = local_pending_push_position;
                        await.wait();
                    }
                }
                return ok;
            }
            await.wait();
        }
        return op_failed_buffer_full;
    }

    op_result try_pop(ValueType& value)
    {
        std::shared_lock read_lock{mutex};
        adaptive_wait await{};
        SizeType local_pending_pop_position{};
        SizeType next_local_pop_position{};

        local_pending_pop_position = pending_pop_position;
        while(true) {
            SizeType local_push_position = push_position;

            if(local_pending_pop_position == local_push_position) {
                return closed ? error_closed : op_failed_buffer_empty;
            }
            next_local_pop_position = ring_next_pos(local_pending_pop_position);
            if(pending_pop_position.compare_exchange_weak(local_pending_pop_position,
                                                          next_local_pop_position))
                break;
            await.wait();
        }
        value = std::ranges::iter_move(buffer.begin() + ring_position(local_pending_pop_position));
        {
            adaptive_wait await{};
            SizeType acquired_slot = local_pending_pop_position;
            while(!this->pop_position.compare_exchange_weak(acquired_slot, next_local_pop_position)) {
                acquired_slot = local_pending_pop_position;
                await.wait();
            }
        }
        return ok;
    }

  private:
    SizeType ring_next_pos(SizeType position)
    {
        if(ring_position(++position) >= buffer.size())
            position += ring_buffer_capacity - buffer.size(); // If the position reached
        return position;
    }
    constexpr SizeType ring_position(SizeType const position) const
    {
        return position & (ring_buffer_capacity - 1);
    }
    constexpr bool is_full(SizeType const from, SizeType const to) const
    {
        return to >= from + ring_buffer_capacity;
    }
};

#endif
