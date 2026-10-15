#include "common.hpp"

// Compute C = A * B for row-major square matrices, using a scalar dot product
// for each output. C is overwritten, so this is not C += A * B.
// a[i*n+k] walks along a row while b[k*n+j] walks down a column as k increases.
// Cache reuse can still occur; the naive loop is not proof of a fixed DRAM traffic.
void gemm_naive(const std::vector<float>& a, const std::vector<float>& b,
                std::vector<float>& c, int n) {
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) {
        // Keep one output accumulator across the full reduction dimension k.
        float sum = 0;
        for (int k = 0; k < n; ++k) sum += a[static_cast<std::size_t>(i)*n+k] * b[static_cast<std::size_t>(k)*n+j];
        c[static_cast<std::size_t>(i)*n+j] = sum;
    }
}

// One C tile survives across the entire k dimension. Real residency depends on cache.
// Scratch is allocated by the caller; packing/zeroing/copy-out are included in timing.
// at, bt and ct each have tile*tile entries and use tile as their row stride.
// This readable implementation illustrates reuse; it is not a tuned BLAS microkernel.
void gemm_blocked(const std::vector<float>& a, const std::vector<float>& b,
                  std::vector<float>& c, int n, int tile,
                  std::vector<float>& at, std::vector<float>& bt, std::vector<float>& ct) {
    for (int ii = 0; ii < n; ii += tile) for (int jj = 0; jj < n; jj += tile) {
        // ii/jj select one output tile. Clamp edge sizes for non-divisible N,
        // including the case where tile is larger than the entire matrix.
        const int rows = std::min(tile, n-ii), cols = std::min(tile, n-jj);
        // Clear once per OUTPUT tile, not once per k block: partial sums must survive.
        std::fill(ct.begin(), ct.end(), 0.0f);
        for (int kk = 0; kk < n; kk += tile) {
            const int depth = std::min(tile, n-kk);
            // Pack the active input rectangles into contiguous scratch arrays.
            // Unused tail entries need not be initialized because loops never read them.
            for (int i = 0; i < rows; ++i) for (int k = 0; k < depth; ++k)
                at[i*tile+k] = a[static_cast<std::size_t>(ii+i)*n+kk+k];
            for (int k = 0; k < depth; ++k) for (int j = 0; j < cols; ++j)
                bt[k*tile+j] = b[static_cast<std::size_t>(kk+k)*n+jj+j];
            // i-k-j ordering reuses one A value across the output columns.
            // B and C are contiguous in the innermost loop, making SIMD possible.
            // Whether scratch remains in fast cache is a MODEL ASSUMPTION, not
            // something std::vector or these loops can guarantee on every CPU.
            for (int i = 0; i < rows; ++i) for (int k = 0; k < depth; ++k) {
                const float value = at[i*tile+k];
                for (int j = 0; j < cols; ++j) ct[i*tile+j] += value * bt[k*tile+j];
            }
        }
        // Write each output element once, after all k blocks have contributed.
        for (int i = 0; i < rows; ++i) for (int j = 0; j < cols; ++j)
            c[static_cast<std::size_t>(ii+i)*n+jj+j] = ct[i*tile+j];
    }
}

int main(int argc, char** argv) try {
    if (argc != 5) {
        std::cerr << "usage: gemm N naive|blocked TILE REPEATS\n"
                  << "limits: N 1..2048, tile 1..256, repeats 1..100 (tile ignored by naive)\n";
        return 2;
    }
    const int n = positive(argv[1], 2048), tile = positive(argv[3], 256), repeats = positive(argv[4], 100);
    const std::string impl = argv[2];
    if (impl != "naive" && impl != "blocked") throw std::runtime_error("invalid implementation");
    // Allocate outside the timer. Three tile buffers use 12*tile^2 bytes in FP32,
    // in addition to the full matrices; larger tiles are not automatically faster.
    const std::size_t count = static_cast<std::size_t>(n)*n;
    std::vector<float> a(count), b(count), c(count);
    std::vector<float> at(tile*tile), bt(tile*tile), ct(tile*tile);
    std::uint32_t seed = 2026;
    for (auto& v : a) v = sample(seed);
    for (auto& v : b) v = sample(seed);
    // Independent FP64 accumulation from the same inputs, computed once before timing.
    // Cast BEFORE multiplication so the reference product is evaluated in double.
    // External whole-process profilers also see this potentially expensive reference.
    std::vector<double> ref(count, 0);
    for (int i = 0; i < n; ++i) for (int k = 0; k < n; ++k)
        for (int j = 0; j < n; ++j)
            ref[static_cast<std::size_t>(i)*n+j] += static_cast<double>(a[static_cast<std::size_t>(i)*n+k]) * b[static_cast<std::size_t>(k)*n+j];
    std::cout << "gemm impl=" << impl << " n=" << n << " tile=" << tile
              << " seed=2026 warmup=1 threads=1\n";
    std::vector<double> times;
    // One unreported warmup followed by repetitions of the exact same multiplication.
    for (int run = -1; run < repeats; ++run) {
        std::fill(c.begin(), c.end(), 0.0f);
        // Only the selected implementation is timed. For blocked this includes
        // packing, scratch initialization and copying the completed tile back to C.
        const auto start = Clock::now();
        if (impl == "naive") gemm_naive(a,b,c,n);
        else gemm_blocked(a,b,c,n,tile,at,bt,ct);
        const double ms = std::chrono::duration<double, std::milli>(Clock::now()-start).count();
        // Validate every run outside timing; no checksum-only shortcut is used.
        if (!check(c,ref)) return 1;
        if (run >= 0) {
            times.push_back(ms);
            std::cout << "sample=" << run << " compute_ms=" << ms << '\n';
        }
    }
    // Conventional GEMM work: N^3 multiply-adds, counted as 2*N^3 FLOP.
    // This excludes packing/index operations and is not measured hardware throughput.
    summary(times, 2.0*n*n*n, "GFLOP/s");
} catch (const std::exception& e) {
    // Includes malformed arguments, out-of-range integers and allocation failures.
    std::cerr << e.what() << '\n';
    return 2;
}
