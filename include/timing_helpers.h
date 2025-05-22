#ifndef TIMING_H
#define TIMING_H

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

struct TimingStats
{
    using StorageDuration = std::chrono::nanoseconds;

    StorageDuration total = StorageDuration::zero();
    StorageDuration min = StorageDuration::max();
    StorageDuration max = StorageDuration::zero();
    StorageDuration last = StorageDuration::zero();
    uint64_t count = 0;

    template <typename OtherDuration>
    void operator()(OtherDuration duration) {
        StorageDuration d = std::chrono::duration_cast<StorageDuration>(duration);
        last = d;
        total += d;
        if (d < min) min = d;
        if (d > max) max = d;
        ++count;
    }

    double average() const {
        return (count > 0) ? static_cast<double>(total.count()) / static_cast<double>(count) : 0.0;
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

template <typename OutputDuration = std::chrono::milliseconds>
void printStats(const std::string& name, const TimingStats& stats) {
    constexpr const char* units = getDurationUnits<OutputDuration>();
    auto toOut = [](const auto& d) { return std::chrono::duration_cast<OutputDuration>(d).count(); };

    std::cout << name << ":\n";
    std::cout << "  Iterations: " << stats.count << "\n";
    std::cout << "  Last:       " << toOut(stats.last) << " " << units << "\n";
    std::cout << "  Min:        " << toOut(stats.min) << " " << units << "\n";
    std::cout << "  Max:        " << toOut(stats.max) << " " << units << "\n";
    std::cout << "  Average:    " << (stats.count ? 
        (static_cast<double>(toOut(stats.total)) / stats.count) : 0.0)
        << " " << units << "\n";
}

template <typename StatsT>
class ScopedStopwatch {
public:
    using clock = std::chrono::steady_clock;
    using time_point = typename clock::time_point;

    ScopedStopwatch(StatsT& stats)
        : stats_(stats), t_start_(clock::now()) {}

    void Stop() {
        if (stopped_) return;
        stopped_ = true;
        auto t_end = clock::now();
        stats_(t_end - t_start_);
    }

    ~ScopedStopwatch() { Stop(); }
private:
    StatsT& stats_;
    time_point t_start_;
    bool stopped_ = false;
};

class ScopedRateLimiter {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    template <typename DurationT>
    explicit ScopedRateLimiter(DurationT interval)
        : target_time_(clock::now() + interval) {}

    ~ScopedRateLimiter() {
        auto now = clock::now();
        if (now < target_time_) {
            std::this_thread::sleep_until(target_time_);
        } 
        // else
        // {
        //     Loop overrun, can add logging here if needed
        // }
    }

private:
    time_point target_time_;
};

#endif // TIMING_H