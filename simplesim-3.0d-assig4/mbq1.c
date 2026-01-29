/* mbq1.c - Microbenchmark for verifying next-line prefetcher correctness
 *
 * This program performs a strictly sequential traversal over a large array.
 * The working set (~4MB) is much larger than the 4KB L1 data cache used in
 * q1-nextline.cfg, ensuring that accesses naturally miss in DL1 when no
 * prefetcher is used. A correct next-line prefetcher should recognize the
 * sequential pattern and prefetch block i+1 immediately after block i.
 *
 * When the prefetcher works correctly, the DL1 read miss rate should drop
 * dramatically, and the simulator should report a large number of
 * dl1.prefetch_accesses and ul2.prefetch_hits, demonstrating correct and
 * timely prefetches.
 */

#define N (1<<20)   /* 1M integers → 4MB total */
int A[N];
volatile long long sink;

int main() {
    int i;
    long long sum = 0;

    /* Sequential streaming access: perfect for next-line prefetching */
    for (i = 0; i < N; i++) {
        sum += A[i];
    }

    sink = sum; /* Prevent optimization */
    return 0;
}