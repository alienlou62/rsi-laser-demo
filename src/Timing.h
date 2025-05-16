#ifndef TIMING_H
#define TIMING_H

#include <chrono>
#include <iostream>
#include <string>

template <typename DurationT>
struct Stopwatch {
    using clock = std::chrono::steady_clock;
    using time_point = typename clock::time_point;

    DurationT total = DurationT::zero();
    DurationT min = DurationT::max();
    DurationT max = DurationT::zero();
    DurationT last = DurationT::zero();
    uint64_t count = 0;

    time_point t_start;
    bool running = false;

    void Start() {
        t_start = clock::now();
        running = true;
    }

    void Stop() {
        if (!running) {
            std::cerr << "Stopwatch::Stop() called without matching Start()!\n";
            return;
        }
        time_point t_end = clock::now();
        last = std::chrono::duration_cast<DurationT>(t_end - t_start);
        total += last;
        if (last < min) min = last;
        if (last > max) max = last;
        ++count;
        running = false;
    }

    double average() const {
        if (count == 0) return 0.0;
        return static_cast<double>(total.count()) / static_cast<double>(count);
    }
};

// Helper function for units
template <typename DurationT>
constexpr const char* getDurationUnits() {
    if constexpr (std::is_same_v<DurationT, std::chrono::hours>)        return "h";
    else if constexpr (std::is_same_v<DurationT, std::chrono::minutes>) return "min";
    else if constexpr (std::is_same_v<DurationT, std::chrono::seconds>) return "s";
    else if constexpr (std::is_same_v<DurationT, std::chrono::milliseconds>) return "ms";
    else if constexpr (std::is_same_v<DurationT, std::chrono::microseconds>) return "us";
    else if constexpr (std::is_same_v<DurationT, std::chrono::nanoseconds>)  return "ns";
    else return "unknown";
}

template <typename DurationT>
void printStats(const std::string& name, const Stopwatch<DurationT>& stats) {
    constexpr const char* units = getDurationUnits<DurationT>();
    std::cout << name << ":\n";
    std::cout << "  Iterations: " << stats.count << "\n";
    std::cout << "  Last:       " << stats.last.count() << " " << units << "\n";
    std::cout << "  Min:        " << stats.min.count() << " " << units << "\n";
    std::cout << "  Max:        " << stats.max.count() << " " << units << "\n";
    std::cout << "  Average:    " << stats.average() << " " << units << "\n";
}

#endif // TIMING_H