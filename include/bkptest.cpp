
#include "../include/ring_buffer.h"

#include <algorithm>
#include <future>
#include <iostream>
#include <numeric>
#include <random>
#include <ranges>
#include <syncstream>
#include <thread>
#include <type_traits>
#include <vector>
auto main() -> int
{
    constexpr auto total_pushs = 1000'000ull;
    auto scout{std::osyncstream{std::cout}};
    auto thn = std::thread::hardware_concurrency();
    auto pop_counts = std::vector<uint64_t>(thn - 1);
    auto futs = std::vector<std::future<unsigned long long>>();
    futs.reserve(thn - 1);
    auto generate_random_number_in_range = [](int min, int max) {
        return [distribution = std::uniform_int_distribution<int>(min, max),
                random_engine = std::mt19937{std::random_device{}()}]() mutable {
            return distribution(random_engine);
        };
    };
    std::ranges::generate(pop_counts, generate_random_number_in_range(total_pushs / thn, total_pushs / (thn - 1)));
    const auto total_pops = std::accumulate(pop_counts.begin(), pop_counts.end(), 0);
    const auto buff_capacity = (total_pushs - (total_pushs - total_pops)) / 100;
    auto rb = RingBuffer<unsigned long long>(buff_capacity);
    auto sum{[&rb, &scout](const uint64_t pop_count, auto thn) {
        auto sum{0ull};
        for(auto i{0ull}; i < pop_count; ++i) {
            if(!i)
                scout << "start pop-thread #" << thn << '\n';
            else if(i % (pop_count / 10) == 0)
                scout << "pop-thread #" << thn << ", pop #" << i << '\n';
            sum += rb.pop();
        }
        return sum;
    }};
    std::ranges::transform(pop_counts, std::back_inserter(futs), [&sum, &futs](int pop_count) { return std::async(sum, pop_count, futs.size() + 1); });
    scout << "available threads: { " << thn << " }" << '\n';
    scout << "ring buffer cappacity: { " << buff_capacity << " }" << '\n';
    scout << "total pushs to make by main thread: { " << total_pushs << " }\n";
    scout << "total pops to make by " << thn - 1 << " popping threads: " << total_pops << ", pops per popping thread { ";
    std::ranges::copy(pop_counts, std::ostream_iterator<uint64_t>(std::cout, " "));
    scout << "}\n";
    const auto total_sum{total_pops * (total_pops - 1) / 2ull};
    scout << "Expected: { after processing, ring_buffer size==" << total_pushs - total_pops << " }\n";
    scout << "Expected: { SUM(popped elements)==" << total_sum << " }\n";
    for(auto i{0ull}; i < total_pushs; ++i)
        rb.push(i);
    auto total_sum_popped{0ull};
    // scout.emit();
    for(auto& partial_sum : futs) {
        total_sum_popped += partial_sum.get();
    }
    scout << "Actuall: { after processing, ring_buffer size==" << rb.size() << " }\n";
    scout << "Actuall: { SUM(popped elements)==" << total_sum_popped << " }\n";
    return 1;
}
