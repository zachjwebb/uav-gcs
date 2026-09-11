#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define FREQUENCY_HZ 50
#define TOTAL_ITERATIONS 1500
#define INTERVAL_NS 20000000L
#define WORK_US 2000

double get_delta_us(struct timespec start, struct timespec end) {
    
    return ((double)(end.tv_sec - start.tv_sec) * 1000000.0) + ((double)(end.tv_nsec - start.tv_nsec) / 1000.0);

}

int compare_doubles(const void *a, const void *b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

void print_metrics(const char* label, double intervals[], struct timespec total_start, struct timespec total_end) {
    // Sort intervals to find min, max, and percentiles
    qsort(intervals, TOTAL_ITERATIONS - 1, sizeof(double), compare_doubles);
    
    double min = intervals[0];
    double max = intervals[TOTAL_ITERATIONS - 2];
    double p99 = intervals[(int)((TOTAL_ITERATIONS - 1) * 0.99)];
    
    double sum = 0;
    for (int i = 0; i < TOTAL_ITERATIONS - 1; i++) {
        sum += intervals[i];
    }
    double mean = sum / (TOTAL_ITERATIONS - 1);
    double total_elapsed = get_delta_us(total_start, total_end) / 1000000.0;
    
    printf("=== %s RESULTS ===\n", label);
    printf("Total Elapsed Time: %.6f seconds\n", total_elapsed);
    printf("Interval Min:       %.2f us\n", min);
    printf("Interval Mean:      %.2f us\n", mean);
    printf("Interval P99:       %.2f us\n", p99);
    printf("Interval Max:       %.2f us\n", max);
    printf("\n");
}

int main(void) {
    struct timespec timestamps[TOTAL_ITERATIONS];
    double intervals[TOTAL_ITERATIONS - 1];
    struct timespec total_start, total_end;
    
    // =========================================================================
    // RUN 1: Relative Timing (nanosleep)
    // =========================================================================
    struct timespec rel_sleep = {0, INTERVAL_NS};
    
    clock_gettime(CLOCK_MONOTONIC, &total_start);
    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &timestamps[i]);
        
        // Simulate work
        usleep(WORK_US);
        
        // Sleep for a relative 20ms (ignores how long the work took)
        if (i < TOTAL_ITERATIONS - 1) {
            nanosleep(&rel_sleep, NULL);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &total_end);
    
    // Compute intervals for Run 1
    for (int i = 0; i < TOTAL_ITERATIONS - 1; i++) {
        intervals[i] = get_delta_us(timestamps[i], timestamps[i+1]);
    }
    print_metrics("RELATIVE (nanosleep)", intervals, total_start, total_end);

    // =========================================================================
    // RUN 2: Absolute Timing (clock_nanosleep + TIMER_ABSTIME)
    // =========================================================================
    struct timespec next_frame;
    
    clock_gettime(CLOCK_MONOTONIC, &total_start);
    clock_gettime(CLOCK_MONOTONIC, &next_frame);
    
    for (int i = 0; i < TOTAL_ITERATIONS; i++) {

        next_frame.tv_nsec += INTERVAL_NS;
        clock_gettime(CLOCK_MONOTONIC, &timestamps[i]);
        
        // Simulate work
        usleep(WORK_US);
        
        // Advance target time exactly 20ms
        if (next_frame.tv_nsec >= 1000000000L) {
            next_frame.tv_sec += 1;
            next_frame.tv_nsec -= 1000000000L;
        }
        
        // Sleep until the exact calculated timeline milestone
        if (i < TOTAL_ITERATIONS - 1) {
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_frame, NULL);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &total_end);
    
    // Compute intervals for Run 2
    for (int i = 0; i < TOTAL_ITERATIONS - 1; i++) {
        intervals[i] = get_delta_us(timestamps[i], timestamps[i+1]);
    }
    print_metrics("ABSOLUTE (clock_nanosleep)", intervals, total_start, total_end);

    return 0;
}
