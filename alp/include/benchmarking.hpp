#pragma once

#include <chrono>

namespace chess::benchmark {
class Timer {
  public:
    void start();
    void stop();

    double duration();

  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_end;
};
} // namespace chess::benchmark
