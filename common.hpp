#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Small teaching driver helpers, not a benchmark framework.
// Accept decimal digits only: signs, whitespace and trailing characters are errors.
// The caller supplies a limit before dimensions are used for allocation or indexing.
// stoll can also throw on overflow; main catches that exception and reports an error.
inline int positive(const char* text, int limit) {
    const std::string s(text);
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("expected a positive integer");
    const auto n = std::stoll(s);
    if (n < 1 || n > limit) throw std::runtime_error("argument outside supported range");
    return static_cast<int>(n);
}
// Deterministic input generation without implementation-specific random distributions.
// Unsigned arithmetic wraps modulo 2^32. The upper 24 bits fit exactly in FP32;
// dividing by 2^24 produces reproducible values in [0, 1).
// Passing state by reference advances the same sequence across successive calls.
inline float sample(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / 16777216.0f;
}
// Compare every FP32 output against the independently computed FP64 reference.
// Absolute tolerance handles values near zero; relative tolerance scales with magnitude.
// These tolerances are for this teaching workload, not a universal numerical guarantee.
// The checksum is a diagnostic that also consumes the computed output; it is NOT
// a substitute for element-wise checking. Call this only outside the timed region.
inline bool check(const std::vector<float>& got, const std::vector<double>& ref) {
    double max_error = 0, checksum = 0;
    bool ok = got.size() == ref.size();
    for (std::size_t i = 0; i < got.size() && i < ref.size(); ++i) {
        const double error = std::abs(static_cast<double>(got[i]) - ref[i]);
        ok = ok && std::isfinite(got[i]) && error <= 1e-5 + 1e-4 * std::abs(ref[i]);
        max_error = std::max(max_error, error);
        checksum += got[i];
    }
    std::cout << "check=" << (ok ? "PASS" : "FAIL")
              << " max_abs_error=" << max_error << " checksum=" << checksum << '\n';
    return ok;
}
// times must contain at least one measured (non-warmup) duration in milliseconds.
// Take the vector by value so sorting does not change the caller's sample order.
struct TimingStats {
    std::size_t samples;
    double mean_ms;
    double median_ms;
    double stddev_ms;
    double min_ms;
    double max_ms;
};
inline TimingStats timing_stats(std::vector<double> times) {
    if (times.empty()) throw std::runtime_error("timing summary needs at least one sample");
    std::sort(times.begin(), times.end());
    const auto n = times.size();
    const double median = n % 2 ? times[n / 2] : (times[n / 2 - 1] + times[n / 2]) / 2;
    double total = 0;
    for (const double time : times) total += time;
    const double mean = total / static_cast<double>(n);
    double squared_deviation = 0;
    for (const double time : times) {
        const double deviation = time - mean;
        squared_deviation += deviation * deviation;
    }
    // Population standard deviation describes exactly the measured repetitions.
    return {n, mean, median, std::sqrt(squared_deviation / static_cast<double>(n)),
            times.front(), times.back()};
}
inline void summary(std::vector<double> times, double work, const char* unit) {
    const auto stats = timing_stats(std::move(times));
    std::cout << "samples=" << stats.samples
              << " mean_ms=" << stats.mean_ms
              << " median_ms=" << stats.median_ms
              << " stddev_ms=" << stats.stddev_ms
              << " min_ms=" << stats.min_ms
              << " max_ms=" << stats.max_ms
              << " stddev_kind=population " << unit << "=";
    // work / (milliseconds * 10^6) converts to billions of work units per second.
    // Tiny correctness cases can finish within one timer tick; do not divide by zero
    // or claim an infinite throughput for such cases.
    if (stats.median_ms > 0) std::cout << work / (stats.median_ms * 1e6);
    else std::cout << "unavailable(timer_resolution; increase workload)";
    std::cout << '\n';
}
// A monotonic clock avoids wall-clock corrections during elapsed-time measurement.
using Clock = std::chrono::steady_clock;
