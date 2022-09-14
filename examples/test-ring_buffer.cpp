

#include "../include/ring_buffer.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <numeric>
#include <random>
#include <ranges>
#include <string>
#include <syncstream>
#include <thread>
#include <type_traits>
#include <vector>
auto main() -> int
{
    auto now = []() {
        return std::chrono::steady_clock::now();
    };
    constexpr auto total_pushs = 10'000'000ull;
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
    uint64_t total_pops = std::accumulate(pop_counts.begin(), pop_counts.end(), 0);
    uint64_t unpopped = 10;
    *--pop_counts.end() += total_pushs - total_pops - unpopped;
    total_pops = std::accumulate(pop_counts.begin(), pop_counts.end(), 0);
    const auto buff_capacity = 32768;
    auto rb = RingBuffer<unsigned long long>(buff_capacity);
    auto sum{[&rb, &scout](const uint64_t pop_count, auto thn) {
        scout << "start pop-thread #" << thn << '\n';
        auto sum{0ull};
        auto i{0ull};
        for(; i < pop_count; ++i) {
            sum += rb.pop();
        }
        scout << "finished pop-thread #" << thn << ", total pops: " << i << '\n';
        return sum;
    }};
    scout << "available threads: { " << thn << " }" << '\n';
    scout << "ring buffer cappacity: { " << buff_capacity << " }" << '\n';
    scout << "total pushs to make by main thread: { " << total_pushs << " }\n";
    scout << "total pops to make by " << thn - 1 << " popping threads: " << total_pops << ", pops per popping thread { ";
    std::ranges::copy(pop_counts, std::ostream_iterator<uint64_t>(scout, " "));
    scout << "}\n";
    const uint64_t total_sum = (total_pops * (total_pops - 1)) / 2;
    scout << "Expected: { after processing, ring_buffer size==" << unpopped << " }\n";
    scout << "Expected: { SUM(popped elements)==" << total_sum << " }\n";
    scout << ">>>start processing...\n";
    auto start = now();
    auto current_pop_th = 0;
    std::ranges::transform(pop_counts, std::back_inserter(futs), [&sum, &current_pop_th](int pop_count) {
        return std::async(sum, pop_count, ++current_pop_th);
    });
    for(auto i{0ull}; i < total_pushs; ++i)
        rb.push(i);
    auto total_sum_popped{0ull};
    for(auto& partial_sum : futs) {
        total_sum_popped += partial_sum.get();
    }
    auto end = now();
    scout << ">>>processing finished\n";
    const auto test1_passed = (rb.size() == unpopped);
    const auto test2_passed = (total_sum_popped == total_sum);
    using namespace std::string_literals;
    scout << "Actuall: { after processing, ring_buffer size==" << rb.size() << " }" << (test1_passed ? " => as Expected"s : " => Failed"s) << '\n';
    scout << "Actuall: { SUM(popped elements)==" << total_sum_popped << " }" << (test2_passed ? " => as Expected"s : " => Failed"s) << '\n';
    if(test1_passed && test2_passed) {
        scout << "### RingBuffer Test PASSED ###\n";
        scout << "execution time: " << std::chrono::duration<double>(end - start).count() << " seconds\n";
        return 0;
    }
    scout << ">>> RingBuffer Test FAILED <<<\n";
    return 1;
}
