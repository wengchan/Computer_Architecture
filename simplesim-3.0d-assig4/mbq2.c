/* mbq2.c - Microbenchmark for verifying stride prefetcher correctness
 *
 * This benchmark accesses a large array using a fixed positive stride.
 * With stride = 8 integers (32 bytes), each memory access is exactly
 * one half-block (for 32-byte block size) or one quarter-block (for 64-byte block size)
 * apart. This creates a perfectly regular access pattern that a stride
 * prefetcher should detect after observing two consecutive differences.
 *
 * When implemented correctly, the stride prefetcher should prefetch
 * A[i + stride] after A[i] is accessed, and we should observe:
 *   - many DL1/L2 prefetch accesses,
 *   - many L2 prefetch hits,
 *   - a large reduction in DL1 miss rate compared to no prefetching.
 */

#define N (1<<20)     /* 1M elements = 4MB */
#define STRIDE 8      /* access A[0], A[8], A[16], ... */
int A[N];
volatile long long sink;

int main() {
    int i;
    long long sum = 0;

    for (i = 0; i < N; i += STRIDE) {
        sum += A[i];
    }

    sink = sum;  /* prevent compiler optimization */
    return 0;
}
