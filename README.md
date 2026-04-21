# SPSC Queue Benchmark

Lock-free SPSC ring buffer vs mutex queue benchmark for latency and throughput analysis.

---

## Overview

This project compares a lock-free single-producer single-consumer (SPSC) ring buffer against a pthread mutex-based queue.

The goal is to quantify the impact of synchronization strategy on:

- Latency (P50, P99, worst-case)
- Throughput
- Tail latency behavior under load

---

## Key Takeaways

- **Throughput (burst):** ~18–28 M msgs/sec (SPSC) vs ~5 M (mutex) -> ~3–5× higher
- **Latency (median):** ~3–5× lower than mutex under load
- **Tail latency (P99):** significantly lower than mutex in burst scenarios
- **Low-load case:** differences narrow; OS effects dominate

---

## System Context

Results depend heavily on CPU architecture, cache topology, and OS scheduling behavior.

Test environment:

- OS: Ubuntu 24.04.4 LTS
- Architecture: x86_64
- CPU: Intel Core i5-10310U @ 1.70GHz
- Cores / Threads: 4 cores / 8 threads (SMT enabled)
- Frequency range: 0.4 GHz – 4.4 GHz (turbo enabled)

Cache hierarchy:
- L1: 128 KiB total (4 × 32 KiB instruction/data)
- L2: 1 MiB (4 × 256 KiB)
- L3: 6 MiB shared

NUMA:
- Single node (0–7 logical CPUs)

Core pinning:
- Producer pinned to CPU 0
- Consumer pinned to CPU 1
- (Note: logical CPUs; SMT effects may still influence results)

This benchmark compares a lock-free SPSC queue against a mutex-based queue under the same system conditions.

Because it runs on a normal operating system, results vary between runs due to scheduling, caching, and background activity.

Even with this variation, the relative performance differences between the two implementations remain consistent when CPU cores are pinned.

---

## Key Concepts

- Lock-free SPSC queue using atomic operations
- Acquire/release memory ordering semantics
- Cache-line padding to avoid false sharing
- Busy-wait (spin) synchronization
- Cycle-level latency measurement using timestamp counters

---

## Build

Compile the benchmark:

```bash
gcc -O3 -std=c11 -Wall -Wextra -Wpedantic -march=native -pthread spsc_zerocopy_benchmark.c -o spsc_benchmark
```

---

## How to Run

Optional system tuning (reduces scheduling and frequency noise):

```bash
./tune_system.sh
```

Run benchmark:

```bash
./spsc_benchmark
```

Recommended (for stability and variance observation):

```bash
./tune_system.sh
./spsc_benchmark
./spsc_benchmark
./spsc_benchmark
```

---

## Notes on System Tuning

`tune_system.sh` reduces variability from:

- CPU frequency scaling
- OS scheduler jitter
- interrupt migration
- background process noise

It does not affect correctness, only measurement stability.

---

## Results Summary

### Saturated workload
- Mutex queue shows higher median and tail latency
- Lock-free SPSC significantly reduces latency under contention
- Throughput is substantially higher for lock-free design

### Heavy Telemetry workload
- Mutex exhibits higher variability in latency
- Lock-free remains stable under steady ingestion

### Ultra-Low Latency workload
- Differences shrink under low contention
- OS noise dominates tail behavior

Full raw output is available in `results.txt`.

---

## Results

An example run output is included in `results.txt`.

Results will vary slightly between runs due to OS scheduling and caching effects, but relative performance trends remain consistent.

---

## File Structure

```
spsc_benchmark/
├── spsc_zerocopy_benchmark.c
├── tune_system.sh
├── results.txt
└── README.md
```

---
## Reproducibility Notes

- Run multiple times to see consistent trends
- Keep CPU pinning enabled
- Close heavy background applications while testing
- Expect variation in exact numbers between runs

