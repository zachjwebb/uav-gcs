# Performance Measurements

## Test Environment

| Property | Value |
| -------- | ----- |
| OS | Debian ... |
| Kernel | (uname -r output) |
| Architecture | aarch64 |
| CPU | (lscpu Model name) |
| Cores | (CPU(s)) |
| Scheduler policy | SCHED_OTHER (default) |
| Build flags | -O0 -g -fsanitize=address,undefined |

Recording the environment matters because these numbers are not portable.
A different kernel, a different scheduler policy, or a machine under
different load produces different results. A performance claim without
its environment is not reproducible.

## Cyclic Executive: Relative vs Absolute Sleep

**Method.** 1500 iterations at a nominal 50 Hz (20 ms period), with 2 ms of
simulated work per cycle. Timestamps captured with `clock_gettime(CLOCK_MONOTONIC)`
at the top of each iteration. Intervals computed between consecutive timestamps.
p99 taken as the value at index `floor(0.99 * (N-1))` of the sorted intervals.
Source: `tools/timing_experiment.c`.

| Metric | Relative (`nanosleep`) | Absolute (`clock_nanosleep` + `TIMER_ABSTIME`) |
| ------ | ---------------------: | ---------------------------------------------: |
| Target elapsed | 30.000 s | 30.000 s |
| Actual elapsed | 37.213 s | 29.986 s |
| Rate error | +24.0% | −0.05% |
| Interval min | 22,147 µs | 2,989 µs |
| Interval mean | 24,824 µs | 20,002 µs |
| Interval p99 | 33,191 µs | 26,340 µs |
| Interval max | 42,449 µs | 36,971 µs |

### Drift

Relative sleeps accumulate error: each iteration costs `work_time + sleep_time`,
so the 2 ms of work plus scheduling overhead compounds across all 1500 cycles.
The loop delivered an effective 40 Hz against a 50 Hz target.

Absolute deadlines derive each wake time from a fixed schedule rather than from
the previous wake time, so per-cycle execution time does not accumulate. Mean
interval error was 2 µs per cycle.

### Jitter

The absolute version's worst cycle ran 36,971 µs against a 20,000 µs period —
a deadline miss of roughly one full cycle. p99 was 26,340 µs. This is scheduler
preemption on a general-purpose kernel, not a defect in the loop.

This measurement is the basis for TM-001 being specified as a statistical bound
plus overrun detection (SIM-006) rather than a hard per-cycle guarantee. A hard
±2 ms requirement would fail on this platform regardless of implementation quality.

Reducing jitter to the tens-of-microseconds range would require a PREEMPT_RT
kernel, `SCHED_FIFO` scheduling priority, CPU isolation (`isolcpus`), and
`mlockall()` to prevent page faults. These are outside the scope of this project.

### Recovery behaviour

The absolute version's minimum interval (2,989 µs) is well below the period.
This is the schedule catching up: after an overrun, the next deadline is already
in the past, so `clock_nanosleep` returns immediately and the following cycle
starts without delay. The relative version cannot recover, since each sleep is a
fresh full period regardless of how late the loop already is.