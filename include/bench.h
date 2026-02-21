#ifndef BENCH_H
#define BENCH_H

#include <stdint.h>
#include <time.h>
#include <sched.h>

#define HISTOGRAM_BUCKETS 1000
#define MAX_BENCHMARK_SAMPLES 100000
#define WARMUP_ITERATIONS 100

/* Timing types */
typedef uint64_t ns_t;  /* nanoseconds */

typedef struct {
    struct timespec start;
    struct timespec end;
} timing_t;

/* Histogram for latency measurements */
typedef struct {
    uint64_t buckets[HISTOGRAM_BUCKETS];  /* Fixed-size histogram: bucket[i] = count of samples in [i*1ns, (i+1)*1ns) */
    uint64_t count;                       /* Total number of samples */
    uint64_t min;                         /* Minimum latency (ns) */
    uint64_t max;                         /* Maximum latency (ns) */
    ns_t samples[MAX_BENCHMARK_SAMPLES];  /* Store raw samples for percentile computation */
    uint64_t sample_count;
} histogram_t;

/* Benchmark result */
typedef struct {
    char *name;
    histogram_t *hist;
    uint64_t total_iterations;
    double duration_sec;
} bench_result_t;

/* Timing macros using CLOCK_MONOTONIC_RAW */
#define TIME_NOW(ts) clock_gettime(CLOCK_MONOTONIC_RAW, &(ts))

#define TIMING_START(timing) do { \
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &(timing).start) < 0) { \
        perror("clock_gettime start"); \
    } \
} while (0)

#define TIMING_STOP(timing) do { \
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &(timing).end) < 0) { \
        perror("clock_gettime stop"); \
    } \
} while (0)

/* Compute elapsed nanoseconds between two timestamps */
static inline ns_t timespec_to_ns(struct timespec ts) {
    return (ns_t)ts.tv_sec * 1000000000ULL + (ns_t)ts.tv_nsec;
}

static inline ns_t timing_elapsed_ns(timing_t timing) {
    ns_t start_ns = timespec_to_ns(timing.start);
    ns_t end_ns = timespec_to_ns(timing.end);
    return end_ns - start_ns;
}

/* Histogram functions */
histogram_t* histogram_alloc(void);
void histogram_free(histogram_t *h);
void histogram_add(histogram_t *h, ns_t latency);
void histogram_add_sample(histogram_t *h, ns_t latency);
double histogram_percentile(histogram_t *h, double percentile);
void histogram_print_summary(histogram_t *h);

/* CPU affinity helpers */
int cpu_pin_current(int cpu_id);
int cpu_get_nprocs(void);
int numa_get_node_for_cpu(int cpu_id);
int numa_get_cpus_for_node(int node_id, int *cpus, int max_cpus);

#endif /* BENCH_H */
