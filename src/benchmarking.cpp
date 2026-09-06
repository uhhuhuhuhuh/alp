#include "benchmarking.hpp"
#include <chrono>

void chess::Timer::start() { m_start = std::chrono::high_resolution_clock::now(); }
void chess::Timer::stop() { m_end = std::chrono::high_resolution_clock::now(); }
double chess::Timer::duration() {
    auto start_us =
        std::chrono::time_point_cast<std::chrono::microseconds>(m_start).time_since_epoch().count();
    auto end_us =
        std::chrono::time_point_cast<std::chrono::microseconds>(m_end).time_since_epoch().count();
    auto duration = end_us - start_us;

    return duration;
};