# Project Structure

## Directory Layout

```
syscall-bench/
├── Makefile                 # Build configuration
├── README.md               # Comprehensive documentation
├── .gitignore             # Git ignore patterns
│
├── include/
│   └── bench.h            # Common header with types, macros, API
│
├── src/
│   ├── main.c             # CLI argument parsing and orchestration
│   ├── bench_epoll.c      # epoll_wait latency measurements
│   ├── bench_futex.c      # futex wake latency measurements
│   ├── bench_io.c         # read/write latency measurements
│   ├── stats.c            # Histogram and percentile computation
│   └── affinity.c         # CPU affinity and NUMA topology
│
├── scripts/
│   └── plot.py            # Matplotlib visualization tool
│
└── bin/
    └── syscall-bench      # Compiled binary
```

## File Descriptions

### Makefile
- Compilation with `-O2 -march=native -Wall -Wextra`
- Targets: `all`, `clean`, `bench`, `help`
- Proper dependency management with `DEPS`
- Pre-allocated bin directory

### include/bench.h
**Key Types:**
- `ns_t`: 64-bit nanosecond timestamps
- `timing_t`: Start/end timespec pair
- `histogram_t`: Fixed-size histogram with pre-allocated sample array
  - `buckets[1000]`: Nanosecond-resolution histogram buckets
  - `samples[MAX_BENCHMARK_SAMPLES]`: Raw sample storage for percentiles
  - `count`, `min`, `max`, `sample_count`: Statistics

**Key Macros:**
- `TIME_NOW(ts)`: Get current time via clock_gettime(CLOCK_MONOTONIC_RAW)
- `TIMING_START/TIMING_STOP`: Measure elapsed time
- `timing_elapsed_ns()`: Convert timespec pair to nanoseconds

**API Functions:**
- `histogram_*`: Histogram allocation, adding samples, percentile computation
- `cpu_pin_current()`: Pin thread to specific CPU
- `cpu_get_nprocs()`: Get number of CPUs
- `numa_get_node_for_cpu()`: Map CPU to NUMA node
- `numa_get_cpus_for_node()`: Get CPUs in NUMA node

### src/main.c
**Responsibilities:**
- getopt-based argument parsing
- Benchmark selection and orchestration
- JSON result serialization
- Histogram summary printing

**Command-line Options:**
```
-a <name>       Add benchmark (epoll, futex, io)
-c <count>      FD count for epoll (default: 10, 100, 1000)
-i <iterations> Iterations per benchmark (default: 10000)
-o <file>       Output JSON file (default: results.json)
-v              Verbose output
-h              Help message
```

### src/bench_epoll.c
**What it measures:** epoll_wait syscall latency

**How it works:**
1. Creates N pipe pairs
2. Registers all read ends with epoll
3. Writes single byte to pipe
4. Measures time from write to epoll_wait return
5. Drains pipe for next iteration

**Variations:**
- Tests with 10, 100, 1000 file descriptors
- Shows O(log n) or O(n) scaling depending on kernel

**Typical results:** 1-10 microseconds

### src/bench_futex.c
**What it measures:** futex wake latency via FUTEX_WAIT_PRIVATE

**Two configurations:**
1. **Same-core**: Waiter and waker on CPU 0
   - Measures pure futex syscall overhead
   - ~100-500 nanoseconds

2. **Cross-core**: Waiter on CPU N-1, waker on CPU 0
   - Measures futex + IPI (Inter-Processor Interrupt)
   - ~1-5 microseconds

**Implementation:**
- Uses pthread_barrier_t for synchronization
- Uses sched_setaffinity for CPU pinning
- Direct syscall via SYS_futex

### src/bench_io.c
**What it measures:** Read/write syscall latency

**Test types:**
1. **Pipe** (64B and 4KB)
2. **Unix domain socket** (socketpair, 64B and 4KB)
3. **Regular file** (mkstemp, 64B and 4KB)

**Measures:**
- write() syscall entry/exit + memcpy
- Kernel buffering behavior
- Impact of payload size

**Typical results:** 100-1000 nanoseconds

### src/stats.c
**Core functions:**
- `histogram_alloc()`: Allocate with pre-allocated buckets and samples
- `histogram_add()`: Add sample to buckets
- `histogram_add_sample()`: Add to both buckets and raw array
- `histogram_percentile()`: Compute P50, P90, P99, P999
  - Sorts raw samples via qsort
  - Indexes into sorted array

**Hot path optimization:**
- No allocations in add/sample path
- All storage pre-allocated at creation
- Percentile computation uses existing samples array

### src/affinity.c
**Functions:**
- `cpu_pin_current(cpu_id)`: Use sched_setaffinity to pin current thread
- `cpu_get_nprocs()`: sysconf(_SC_NPROCESSORS_ONLN)
- `numa_get_node_for_cpu()`: Query /sys/devices/system/cpu/
- `numa_get_cpus_for_node()`: Parse /sys/devices/system/node/nodeN/cpulist

**Uses:**
- Reduces jitter by eliminating context switches
- Enables same-core vs cross-core measurements
- Detects multi-socket systems for NUMA testing

### scripts/plot.py
**Capabilities:**
- Load JSON results
- Plot latency histograms (bar charts)
- Compare percentiles across benchmarks
- Print text summary table

**Output formats:**
- PNG plots via matplotlib
- Formatted text summary to stdout

**Features:**
- Multiple benchmark support
- Customizable output directory
- Optional summary or plot generation

## Build Output

After `make all`:
- **bin/syscall-bench**: 50KB stripped binary
  - Statically linked to libc, libm, libpthread
  - No external dependencies beyond glibc

## Performance Characteristics

### Memory Usage
- Fixed overhead: ~320KB for histogram
- Per-sample: 8 bytes (uint64_t)
- Total with 100k samples: ~320 + 800 = 1.1 MB per histogram

### CPU Usage
- Measurement loop: ~10-50 nanoseconds overhead
- Hot path has zero allocations
- Instruction cache friendly

### Timing Accuracy
- Uses CLOCK_MONOTONIC_RAW
- Nanosecond resolution on modern x86_64
- Typical precision: ±100-500 nanoseconds

## Example Workflow

```bash
# Compile
make all

# Run all benchmarks
./bin/syscall-bench -a epoll -a futex -a io -o results.json

# Visualize
python3 scripts/plot.py results.json

# Clean
make clean
```

## Design Decisions

1. **No dynamic allocation in hot path**: All measurements are free() safe
2. **Fixed histogram buckets**: Predictable performance, no need to handle overflow
3. **Raw sample storage**: Allows accurate percentile computation without re-measurement
4. **CPU pinning via affinity**: Eliminates context switch jitter
5. **CLOCK_MONOTONIC_RAW**: Immune to NTP adjustments
6. **Direct syscalls**: Using syscall() for futex to measure exact latency
7. **Separate benchmark modules**: Easy to add new tests without modifying others
8. **JSON output**: Machine-readable, good for automation
