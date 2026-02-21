# Syscall Latency Profiler

A high-performance Linux syscall latency measurement tool for systems engineering and performance analysis. This profiler measures the exact latency of critical system calls with nanosecond precision.

## Problem Statement

Understanding syscall latency is critical for:
- **Real-time systems**: Predictable, low-latency syscalls are essential
- **High-frequency trading**: Sub-microsecond latencies matter
- **Server optimization**: Identifying syscall bottlenecks in request handling
- **Kernel tuning**: Measuring impact of scheduling policies and CPU affinity

This profiler provides quantitative measurements across three categories of syscalls:
1. **Multiplexing** (epoll_wait) - Event notification latency
2. **Synchronization** (futex) - Cross-core thread synchronization
3. **I/O** (read/write) - Data transfer latency

## Architecture Overview

```
         Main Orchestrator (src/main.c)
         CLI parsing, result aggregation, JSON output
                    |
        +-----------+-----------+
        |           |           |
    epoll_bench  futex_bench  io_bench
        |           |           |
        +-----------+-----------+
                    |
              Histogram (stats.c)
                    |
            CPU Affinity (affinity.c)
```

## Benchmarks

### 1. epoll_wait Latency (src/bench_epoll.c)

Measures the latency from when data is written to a pipe until `epoll_wait()` returns.

- Runs with 10, 100, and 1000 file descriptors
- Single-byte trigger on each iteration
- Typical results: 1-10 microseconds depending on fd count

### 2. futex Wake Latency (src/bench_futex.c)

Measures the round-trip latency of futex synchronization primitives.

- Same-core test: Waiter and waker on same CPU
- Cross-core test: Waiter on last CPU, waker on CPU 0
- Same-core: 100-500 nanoseconds
- Cross-core: 1-5 microseconds (includes IPI)

### 3. I/O Latency (src/bench_io.c)

Measures read/write syscall latency on various FD types.

- Pipe write (64 bytes and 4KB)
- Unix domain socketpair send (64 bytes and 4KB)
- Regular file write (64 bytes and 4KB)
- Typical results: 100-1000 nanoseconds

## Hot Path Optimization

- **Zero heap allocations**: All working memory pre-allocated in histogram struct
- **Static arrays**: Fixed-size bucket arrays for histogram
- **CPU pinning**: Eliminates context switch overhead
- **CLOCK_MONOTONIC_RAW**: Bypasses NTP adjustments, accurate kernel time

## Statistics & Percentiles

The histogram module computes: Min/Max, P50/P90/P99/P999 percentiles for tail latency analysis with online histogram using fixed buckets (1ns resolution up to 1000ns).

## Building

```bash
# Ubuntu/Debian
sudo apt-get install build-essential

cd syscall-bench
make clean && make all
```

## Running Benchmarks

```bash
# Run all benchmarks
./bin/syscall-bench -a epoll -a futex -a io -o results.json

# Single benchmark with custom iterations
./bin/syscall-bench -a epoll -i 50000 -o results.json

# Verbose output
./bin/syscall-bench -a futex -v -o results.json
```

## Output Format (JSON)

```json
{
  "timestamp": 1234567890,
  "benchmarks": [
    {
      "name": "epoll",
      "iterations": 30000,
      "duration_sec": 15.234,
      "min_ns": 850,
      "max_ns": 45000,
      "p50_ns": 2100.0,
      "p99_ns": 8200.0,
      "p999_ns": 25000.0
    }
  ]
}
```

## System Requirements

- **Linux kernel**: 2.6.29+ (for CLOCK_MONOTONIC_RAW)
- **Architecture**: x86_64, ARM64
- **Permissions**: Regular user (no root needed)

## Performance Tips

- Use `-march=native` for CPU-specific optimizations
- Pin to isolated CPUs (kernel boot `isolcpus=2,3`)
- Disable frequency scaling for consistent results
- Use large sample counts (50k+) for statistical significance

## License

MIT License - See LICENSE file
