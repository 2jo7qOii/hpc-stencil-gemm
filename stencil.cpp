#include "common.hpp"

// One time step of a five-point stencil on a row-major H x W grid.
// current and next must not overlap: every output reads the OLD time step only.
// Both arrays have fixed zero boundaries; this function overwrites only the interior.
// Switching traversal order changes locality, not the numerical dependencies.
void stencil_step(const float* current, float* next, int h, int w, bool column) {
    auto update = [&](int r, int c) {
        // Adjacent columns differ by 1 element; adjacent rows differ by w elements.
        // Interior-only iteration makes all four neighbor indices valid.
        const std::size_t i = static_cast<std::size_t>(r) * w + c;
        // Three neighbor additions, two multiplies and one final addition: 6 FLOP.
        // Five source-level reads do NOT imply five DRAM reads: neighbors can be cached.
        next[i] = 0.5f * current[i] + 0.125f *
            (((current[i-w] + current[i+w]) + current[i-1]) + current[i+1]);
    };
    if (column) {
        // The inner loop jumps by one full row, potentially wasting cache-line traffic.
        for (int c = 1; c < w-1; ++c)
            for (int r = 1; r < h-1; ++r) update(r, c);
    } else {
        // The inner loop follows contiguous columns, encouraging locality and SIMD.
        // Actual speed also depends on compiler vectorization and the working-set size.
        for (int r = 1; r < h-1; ++r)
            for (int c = 1; c < w-1; ++c) update(r, c);
    }
}

int main(int argc, char** argv) try {
    if (argc != 6) {
        std::cerr << "usage: stencil HEIGHT WIDTH STEPS row|column REPEATS\n"
                  << "limits: dimensions 3..4096, steps 1..1000, repeats 1..100\n";
        return 2;
    }
    const int h = positive(argv[1], 4096), w = positive(argv[2], 4096);
    const int steps = positive(argv[3], 1000), repeats = positive(argv[5], 100);
    const std::string impl = argv[4];
    if (h < 3 || w < 3 || (impl != "row" && impl != "column"))
        throw std::runtime_error("invalid dimensions or implementation");
    // Convert before multiplication; the validated limits bound allocation size.
    // input is an immutable starting state; a and b are the two working buffers.
    const std::size_t count = static_cast<std::size_t>(h) * w;
    std::vector<float> input(count, 0), a(count), b(count);
    std::uint32_t seed = 2026;
    for (int r = 1; r < h-1; ++r)
        for (int c = 1; c < w-1; ++c) input[static_cast<std::size_t>(r)*w+c] = sample(seed);
    // Independent double-precision reference, outside the measured region.
    // Start from the exact same FP32 inputs, then promote arithmetic to double.
    // ref/tmp also ping-pong so the reference preserves the same time-step semantics.
    std::vector<double> ref(input.begin(), input.end()), tmp(count, 0);
    for (int t = 0; t < steps; ++t) {
        for (int r = 1; r < h-1; ++r) for (int c = 1; c < w-1; ++c) {
            const auto i = static_cast<std::size_t>(r)*w+c;
            tmp[i] = ref[i]/2.0 + (ref[i-w]+ref[i+w]+ref[i-1]+ref[i+1])/8.0;
        }
        // Swapping vector storage is constant-time; no full-grid copy is required.
        ref.swap(tmp);
    }
    std::cout << "stencil impl=" << impl << " h=" << h << " w=" << w
              << " steps=" << steps << " seed=2026 warmup=1 threads=1\n";
    std::vector<double> times;
    // run == -1 is a complete warmup: validate it, but exclude its time from statistics.
    for (int run = -1; run < repeats; ++run) {
        // Reset both buffers outside the timer so every repetition solves the same task.
        // Resetting and later validation affect caches: this is not a cold-cache test.
        a = input;
        std::fill(b.begin(), b.end(), 0.0f);
        const auto start = Clock::now();
        // Include all time steps and swaps, not just a single favorable update.
        for (int t = 0; t < steps; ++t) {
            stencil_step(a.data(), b.data(), h, w, impl == "column");
            // a always owns the newest result, for both odd and even step counts.
            // b now holds the old input and becomes the destination for the next step.
            a.swap(b);
        }
        const double ms = std::chrono::duration<double, std::milli>(Clock::now()-start).count();
        // Check every result and all boundaries, including odd/even final-buffer selection.
        if (!check(a, ref)) return 1;
        // Enforce exact zero boundaries separately; the general comparison uses tolerance.
        for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c)
            if ((r == 0 || c == 0 || r == h-1 || c == w-1) && a[static_cast<std::size_t>(r)*w+c] != 0)
                throw std::runtime_error("boundary changed");
        if (run >= 0) {
            times.push_back(ms);
            std::cout << "sample=" << run << " compute_ms=" << ms << '\n';
        }
    }
    // Count interior updates only; each update is one unit, not five neighbor loads.
    const double updates = static_cast<double>(h-2)*(w-2)*steps;
    const double flops = 6.0 * updates;
    // "Effective bandwidth" is a useful-byte model, not a hardware-counter
    // measurement: ideal spatial reuse reads each input once and writes each output
    // once per step, for 8 bytes per FP32 interior update. Cache-line waste,
    // write-allocate traffic and temporal reuse can make actual DRAM traffic differ.
    const double effective_bytes = 8.0 * updates;
    const auto stats = timing_stats(times);
    std::cout << "samples=" << stats.samples
              << " mean_ms=" << stats.mean_ms
              << " median_ms=" << stats.median_ms
              << " stddev_ms=" << stats.stddev_ms
              << " min_ms=" << stats.min_ms
              << " max_ms=" << stats.max_ms
              << " stddev_kind=population throughput_time=median";
    if (stats.median_ms > 0) {
        std::cout << " GUpdates/s=" << updates / (stats.median_ms * 1e6)
                  << " GFLOP/s=" << flops / (stats.median_ms * 1e6)
                  << " effective_bandwidth_GB/s=" << effective_bytes / (stats.median_ms * 1e6);
    } else {
        const char* unavailable = "unavailable(timer_resolution; increase workload)";
        std::cout << " GUpdates/s=" << unavailable
                  << " GFLOP/s=" << unavailable
                  << " effective_bandwidth_GB/s=" << unavailable;
    }
    std::cout << " bandwidth_model=ideal_spatial_reuse bytes_per_update=8\n";
} catch (const std::exception& e) {
    // Argument conversion and allocation failures are reported consistently.
    std::cerr << e.what() << '\n';
    return 2;
}
