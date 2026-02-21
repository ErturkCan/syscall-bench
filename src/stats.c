#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../include/bench.h"

histogram_t* histogram_alloc(void) {
    histogram_t *h = malloc(sizeof(histogram_t));
    if (!h) {
        perror("malloc");
        return NULL;
    }

    memset(h->buckets, 0, sizeof(h->buckets));
    h->count = 0;
    h->min = UINT64_MAX;
    h->max = 0;
    h->sample_count = 0;

    return h;
}

void histogram_free(histogram_t *h) {
    if (h) {
        free(h);
    }
}

void histogram_add(histogram_t *h, ns_t latency) {
    if (!h) return;

    if (latency < HISTOGRAM_BUCKETS) {
        h->buckets[latency]++;
    } else {
        h->buckets[HISTOGRAM_BUCKETS - 1]++;
    }

    h->count++;
    if (latency < h->min) h->min = latency;
    if (latency > h->max) h->max = latency;
}

void histogram_add_sample(histogram_t *h, ns_t latency) {
    if (!h) return;

    histogram_add(h, latency);

    if (h->sample_count < MAX_BENCHMARK_SAMPLES) {
        h->samples[h->sample_count] = latency;
        h->sample_count++;
    }
}

/* Compare function for qsort */
static int compare_uint64(const void *a, const void *b) {
    uint64_t va = *(const uint64_t *)a;
    uint64_t vb = *(const uint64_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

double histogram_percentile(histogram_t *h, double percentile) {
    if (!h || h->sample_count == 0) {
        return 0.0;
    }

    if (percentile < 0.0) percentile = 0.0;
    if (percentile > 100.0) percentile = 100.0;

    /* Create sorted copy of samples */
    ns_t *sorted = malloc(h->sample_count * sizeof(ns_t));
    if (!sorted) {
        perror("malloc");
        return 0.0;
    }

    memcpy(sorted, h->samples, h->sample_count * sizeof(ns_t));
    qsort(sorted, h->sample_count, sizeof(ns_t), compare_uint64);

    uint64_t index = (uint64_t)((percentile / 100.0) * h->sample_count);
    if (index >= h->sample_count) {
        index = h->sample_count - 1;
    }

    double result = (double)sorted[index];
    free(sorted);
    return result;
}

void histogram_print_summary(histogram_t *h) {
    if (!h) {
        printf("Histogram is NULL\n");
        return;
    }

    printf("    Samples: %lu\n", h->sample_count);
    printf("    Min: %lu ns\n", h->min);
    printf("    Max: %lu ns\n", h->max);
    printf("    P50: %.1f ns\n", histogram_percentile(h, 50.0));
    printf("    P90: %.1f ns\n", histogram_percentile(h, 90.0));
    printf("    P99: %.1f ns\n", histogram_percentile(h, 99.0));
    printf("    P999: %.1f ns\n", histogram_percentile(h, 99.9));
}
