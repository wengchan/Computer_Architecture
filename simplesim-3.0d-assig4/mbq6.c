/* mbq6.c
 *
 * Microbenchmark for a "positive-stride + confidence" stride prefetcher.
 *
 * It contains:
 *
 *   Phase 1: perfect positive stride           → confidence should rise
 *   Phase 2: noisy / irregular stride pattern  → confidence should drop, stop prefetches
 *   Phase 3: perfect stride again              → prefetcher should re-learn the stride
 *
 * Expected behavior:
 *   - dl1.prefetch_accesses > 0 in Phases 1 and 3
 *   - dl1.prefetch_accesses ≈ 0 in Phase 2 (confidence too low)
 *   - dl1.miss_rate drops during Phase 1 & 3
 */

#include <stdio.h>

#define N        (1<<20)     /* 1M elements = large enough to overflow DL1 */
#define STRIDE   8           /* pure positive stride (block aligned on most configs) */

int A[N];
volatile long long sink;

int main() {
    long long sum = 0;
    int i;

    /* ------------------------------------ */
    /* Phase 1: Perfect positive stride      */
    /* ------------------------------------ */
    for (i = 0; i < N; i += STRIDE) {
        sum += A[i];
    }

    /* ------------------------------------ */
    /* Phase 2: Irregular stride (noise)     */
    /* Confidence should drop → STOP PF     */
    /* ------------------------------------ */
    for (i = 0; i < N - 200; ) {
        sum += A[i];
        i += 3;     /* small stride */
        sum += A[i];
        i += 20;    /* medium stride */
        sum += A[i];
        i += 1;     /* tiny stride */
        sum += A[i];
        i += 50;    /* big stride */
        sum += A[i];
        i += 7;     /* semi-random stride */
    }

    /* ------------------------------------ */
    /* Phase 3: Perfect positive stride again */
    /* Prefetcher must relearn the stride   */
    /* ------------------------------------ */
    for (i = 0; i < N; i += STRIDE) {
        sum += A[i];
    }

    sink = sum;  /* prevent compiler optimization */
    return 0;
}
