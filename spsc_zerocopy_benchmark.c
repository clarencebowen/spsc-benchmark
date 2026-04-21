/*
SPSC vs mutex queue benchmark.

Compares a lock-free single-producer single-consumer ring buffer
against a pthread mutex queue under different workloads.

Measures:
- latency distribution (P50 / P99 / max)
- throughput
*/

#define _GNU_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <assert.h>
#include <sched.h>
#include <stdalign.h>
#include <time.h>

#if defined(__x86_64__) || defined(__i386__)
    #include <x86intrin.h>
    #define CPU_RELAX() _mm_pause()

    static inline uint64_t core_local_tsc() {
        unsigned int aux;
        uint64_t tsc = __rdtscp(&aux);
        _mm_lfence();
        return tsc;
    }
#else
    #define CPU_RELAX() __asm__ volatile("yield" ::: "memory")
    static inline uint64_t core_local_tsc() { return 0; }
#endif

#define CACHE_LINE 64
#define RING_SIZE 128
#define MASK (RING_SIZE - 1)

_Static_assert((RING_SIZE & MASK) == 0, "RING_SIZE must be power of 2");

typedef struct {
    uint64_t enqueue_tsc;
    uint8_t payload[24];
} packet_t;

_Static_assert(sizeof(packet_t) == 32, "Packet must be 32 bytes");

typedef struct {
    pthread_mutex_t lock;
    uint32_t head;
    uint32_t tail;
    packet_t buffer[RING_SIZE];
} mutex_queue_t;

typedef struct {
    alignas(CACHE_LINE) _Atomic uint32_t head;
    uint32_t cached_tail;
    uint8_t pad0[CACHE_LINE - sizeof(_Atomic uint32_t) - sizeof(uint32_t)];

    alignas(CACHE_LINE) _Atomic uint32_t tail;
    uint32_t cached_head;
    uint8_t pad1[CACHE_LINE - sizeof(_Atomic uint32_t) - sizeof(uint32_t)];

    packet_t buffer[RING_SIZE];
} spsc_queue_t;

mutex_queue_t baseline_q;
spsc_queue_t zero_copy_q;

bool mutex_push(mutex_queue_t* q, const packet_t* p) {
    pthread_mutex_lock(&q->lock);

    uint32_t next = (q->head + 1) & MASK;
    if (next == q->tail) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }

    memcpy(&q->buffer[q->head], p, sizeof(packet_t));
    q->head = next;

    pthread_mutex_unlock(&q->lock);
    return true;
}

bool mutex_pop(mutex_queue_t* q, packet_t* out) {
    pthread_mutex_lock(&q->lock);

    if (q->head == q->tail) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }

    memcpy(out, &q->buffer[q->tail], sizeof(packet_t));
    q->tail = (q->tail + 1) & MASK;

    pthread_mutex_unlock(&q->lock);
    return true;
}

/* SPSC fast path */

static inline packet_t* spsc_prepare_push(spsc_queue_t* q) {
    uint32_t h = atomic_load_explicit(&q->head, memory_order_relaxed);
    uint32_t next = (h + 1) & MASK;

    if (next == q->cached_tail) {
        q->cached_tail = atomic_load_explicit(&q->tail, memory_order_acquire);
        if (next == q->cached_tail) return NULL;
    }

    return &q->buffer[h];
}

static inline void spsc_commit_push(spsc_queue_t* q) {
    uint32_t h = atomic_load_explicit(&q->head, memory_order_relaxed);
    atomic_store_explicit(&q->head, (h + 1) & MASK, memory_order_release);
}

static inline packet_t* spsc_prepare_pop(spsc_queue_t* q) {
    uint32_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);

    if (t == q->cached_head) {
        q->cached_head = atomic_load_explicit(&q->head, memory_order_acquire);
        if (t == q->cached_head) return NULL;
    }

    return &q->buffer[t];
}

static inline void spsc_commit_pop(spsc_queue_t* q) {
    uint32_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
    atomic_store_explicit(&q->tail, (t + 1) & MASK, memory_order_release);
}

/* benchmark state */

const int NUM_PACKETS = 100000;
uint64_t* latency_records;
bool use_mutex = true;
int pacing = 0;

void pin_thread(int core) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}

void* producer(void* _) {
    pin_thread(0);

    packet_t p;
    memset(p.payload, 0xAA, sizeof(p.payload));

    for (int i = 0; i < NUM_PACKETS; i++) {

        if (use_mutex) {
            p.enqueue_tsc = core_local_tsc();
            while (!mutex_push(&baseline_q, &p)) CPU_RELAX();
        } else {
            packet_t* slot;
            while (!(slot = spsc_prepare_push(&zero_copy_q))) CPU_RELAX();
            slot->enqueue_tsc = core_local_tsc();
            spsc_commit_push(&zero_copy_q);
        }

        for (volatile int j = 0; j < pacing; j++) {}
    }

    return NULL;
}

void* consumer(void* _) {
    pin_thread(1);

    packet_t p;

    for (int i = 0; i < NUM_PACKETS; i++) {

        if (use_mutex) {
            while (!mutex_pop(&baseline_q, &p)) CPU_RELAX();
            latency_records[i] = core_local_tsc() - p.enqueue_tsc;
        } else {
            packet_t* slot;
            while (!(slot = spsc_prepare_pop(&zero_copy_q))) CPU_RELAX();
            latency_records[i] = core_local_tsc() - slot->enqueue_tsc;
            spsc_commit_pop(&zero_copy_q);
        }
    }

    return NULL;
}
