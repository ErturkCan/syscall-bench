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
┌─────────────────────────────────────────────────────────────┐
│                     Main Orchestrator                       │
│                   (src/main.c)                              │
│          CLI parsing, result aggregation, JSON output       │
└──────────────┬──────────────────────────────────────────────┘
               │
        ┌──────┼──────┬──────────┐
        │      │      │          │
   ┌────▼──┐ ┌─▼──┐ ┌─▼──┐  ┌──▼────┐
   │epoll  │ │futx│ │I/O │  │Stats  │
   │Bench  │ │Bnch│ │Bnch│  │Module │
   └───────┘ └────┘ └────┘  └───────┘
        │      │      │          │
        └──────┼──────┼──────────┘
               │
        ┌──────▼──────┐
        │  Histogram  │
        │  (stats.c)  │
        └─────────────┘
               │
        ┌──────▼─────────────┐
        │  CPU Affinity      │
        │  (affinity.c)      │
        └────────────────────┘
```

## Benchmarks

### 1. epoll_wait Latency (src/bench_epoll.c)

Measures the latency from when data is written to a pipe until `epoll_wait()` returns.

**Configuration**:
- Runs with 10, 100, and 1000 file descriptors
- Single-byte trigger on each iteration
- Drains event buffer between measurements

**Measures**:
- Syscall entry/exit overhead
- Event ring buffer traversal
- Impact of descriptor count on latency

**Typical results**: 1-10 microseconds depending on fd count

### 2. futex Wake Latency (src/bench_futex.c)

Measures the round-trip latency of futex synchronization primitives.

**Configuration**:
- Same-core test: Waiter and waker on same CPU
- Cross-core test: Waiter on last CPU, waker on CPU 0
- CPU pinning via sched_setaffinity

**Measures**:
- Futex syscall entry/exit
- Kernel scheduler latency
- Cross-core IPI (Inter-Processor Interrupt) latency

**Typical results**:
- Same-core: 100-500 nanoseconds
- Cross-core: 1-5 microseconds (includes IPI)

### 3. I/O Latency (src/bench_io.c)

Measures read/write syscall latency on various FD types.

**Configuration**:
- Pipe write (64 bytes and 4KB)
- Unix domain socketpair send (64 bytes and 4KB)
- Regular file write (64 bytes and 4KB)

**Measures**:
- Syscall overhead for different kernel paths
- Buffer copy overhead
- Kernel buffering behavior
- Impact of payload size

**Typical results**: 100-1000 nanoseconds

## Hot Path Optimization

The measurement hot path uses:
- **Zero heap allocations**: All working memory pre-allocated in histogram struct
- **Static arrays**: Fixed-size bucket arrays for histogram
- **Inline timing**: `clock_gettime()` calls minimized, measured directly
- **CPU pinning**: Eliminates context switch overhead
- **CLOCK_MONOTONIC_RAW**: Bypasses NTP adjustments, accurate kernel time

## Statistics & Percentiles

The histogram module computes:
- **Min/Max**: Global minimum and maximum latencies
- **P50/P90/P99/P999**: Percentiles for tail latency analysis
- **Sample storage**: All measurements stored for detailed analysis

Percentile computation uses online histogram with fixed buckets (1ns resolution up to 1000ns, then coarse buckets).

## Building

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# For visualization (optional)
pip install matplotlib
```

### Compilation

```bash
cd syscall-bench
make clean
make all
```

**Flags**:
- `-O2`: Optimization level 2 (better code generation)
- `-march=native`: CPU-specific optimizations
- `-Wall -Wextra`: All warnings enabled
- `-D_GNU_SOURCE`: Linux-specific features (CLOCK_MONOTONIC_RAW, futex, etc.)

## Running Benchmarks

### Run all benchmarks

```bash
./bin/syscall-bench -a epoll -a futex -a io -o results.json
```

### Run single benchmark with custom iterations

```bash
./bin/syscall-bench -a epoll -i 50000 -o results.json
```

### Run with verbose output

```bash
./bin/syscall-bench -a futex -v -o results.json
```

### Run one benchmark at a time

```bash
./bin/syscall-bench -a epoll -o epoll_results.json
./bin/syscall-bench -a futex -o futex_results.json
./bin/syscall-bench -a io -o io_results.json
```

## Output Format

### JSON Results (results.json)

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
      "p90_ns": 3500.0,
      "p99_ns": 8200.0,
      "p999_ns": 25000.0
    }
  ]
}
```

### Visualization

```bash
python3 scripts/plot.py results.json -o plots/

# Or just print summary
python3 scripts/plot.py results.json --summary
```

This generates:
- `latency_histograms.png`: Bar charts of percentiles per benchmark
- `percentile_comparison.png`: Comparison across benchmarks

## Performance Experiment Ideas

### 1. CPU Frequency Scaling Impact

```bash
# Run at different CPU frequencies
echo powersave | sudo tee /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
./bin/syscall-bench -a futex -o freq_powersave.json

echo performance | sudo tee /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
./bin/syscall-bench -a futex -o freq_performance.json
```

### 2. NUMA Impact

```bash
# Bind to different NUMA nodes
numactl --cpunodebind=0 ./bin/syscall-bench -a epoll -o numa0.json
numactl --cpunodebind=1 ./bin/syscall-bench -a epoll -o numa1.json
```

### 3. Load Impact

```bash
# Run in quiet system
./bin/syscall-bench -a io -o baseline.json

# Run under CPU load
stress-ng --cpu 7 &
./bin/syscall-bench -a io -o under_load.json
pkill -f stress-ng
```

### 4. fd_count sensitivity for epoll

Currently runs 10, 100, 1000 FDs. Modify `bench_epoll.c` to test other counts:

```c
int fd_counts[] = {1, 5, 10, 50, 100, 500, 1000, 5000};
```

### 5. Payload Size Sensitivity for I/O

The I/O benchmark tests 64B and 4KB. Add more sizes:

```c
int sizes[] = {1, 8, 64, 256, 1024, 4096, 8192};
```

## System Requirements

- **Linux kernel**: 2.6.29+ (for CLOCK_MONOTONIC_RAW)
- **Architecture**: x86_64, ARM64 (any with sched_setaffinity)
- **Permissions**: Regular user (no root needed)

Note: For cross-core futex measurements, system must have 2+ CPUs.

## Implementation Details

### Timing Mechanism

Uses `clock_gettime(CLOCK_MONOTONIC_RAW)`:
- Monotonic: Time never goes backwards
- Raw: Unaffected by NTP adjustments
- Resolution: Nanosecond-capable on most systems
- Zero-allocation: Struct is on stack

### Histogram Buckets

```
bucket[i] = count of samples with latency in [i*1ns, (i+1)*1ns)
bucket[0] = 0-1ns samples
bucket[1] = 1-2ns samples
...
bucket[999] = 999-1000ns samples
bucket[999] also captures >= 1000ns (overflow)
```

Raw samples stored separately for accurate percentile computation.

### CPU Affinity

Uses `sched_setaffinity()` with CPU_SET macros:
- Prevents context switching during measurements
- Ensures reproducible timing
- Enables same-core vs cross-core comparison

NUMA topology queried from `/sys/devices/system/node/` for cross-node testing.

## Contributing

To add a new benchmark:

1. Create `src/bench_mytest.c`:
  - Implement `int bench_mytest(int iterations, histogram_t *hist)`
  - Use `TIMING_START`/`TIMING_STOP` macros
  - Call `histogram_add_sample()` for each measurement

2. Update `include/bench.h`:
  - Declare the benchmark function

3. Update `src/main.c`:
  - Add to benchmark list parsing
  - Call the benchmark function

4. Update `Makefile`:
  - Add `src/bench_mytest.o` to OBJS

## Performance Tips

- Run with `-O2` or higher compiler optimization
- Use `-march=native` for CPU-specific optimizations
- Pin to isolated CPUs (kernel boot `isolcpus=2,3`)
- Disable frequency scaling: `echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor`
- Disable SMT (hyperthreading) for more consistent results
- Use large sample counts (50k+) for statistical significance

## Troubleshooting

### Latencies are very high (>100 microseconds)

- Check for CPU context switches: `vmstat 1` in another terminal
- Check for frequency scaling: `grep MHz /proc/cpuinfo`
- Try pinning to isolated CPUs: `taskset -c 2 ./bin/syscall-bench ...`

### Results vary widely between runs

- Disable frequency scaling
- Run on isolated CPUs
- Increase iteration count with `-i`
- Check system load with `top`

### epoll_wait latency increases with fd_count

- Expected behavior: O(log n) to O(n) depending on kernel version
- Use `-v` flag to see per-count results
- Kernel >= 4.10 uses optimized algorithms

## License

This is a portfolio project demonstrating systems programming knowledge.

## See Also

- Linux man pages: `man epoll_wait`, `man futex`, `man clock_gettime`
- LWN.net articles on performance: https://lwn.net/
- Kernel source: https://github.com/torvalds/linux
