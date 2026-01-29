//mbq1.c -- microbenchmark with both Case1 and Case2 RAW hazards /
#include <stdio.h>

// from running sim-safe, this is result of benchmark:
// sim_num_RAW_hazard_q1     4001965   # total number of RAW hazards (q1)
// sim_num_RAW_hazard_q1_1stall      3001620 # total number of 1 cycle stall RAW hazards (q1)
// sim_num_RAW_hazard_q1_2stall      1000345 # total number of 2 cycle stall RAW hazards (q1)
// expected ratio of RAW hazard 1 stall/RAW hazard is 75%, and the result of ratio of benchmark is around 75%,
// therefore our benchmark shows that the sim-safe is working correctly.
int main(void)
{
    register int a = 1, b = 2, c = 3, d = 4;
    register int loop_cond = 1;
    register int random_inst = 0;
    register int N = 1000000;   

    /* large enough to dominate startup costs */
    while(loop_cond) {
        random_inst = 0;
        random_inst = 0;
        
        /* ---- Force Case1 2 Stalls(75%) ---- */
        a = b + c;   /* produces a */
        b = a + d;   /* consumes a immediately -> 2 Stalls  & produces b */

        c = b + d;   /* consumes b immediately -> 2 Stalls & produces c */
        d = c + a;   /* consumes c immediately -> 2 Stalls */

        /* ---- Force Case2 1 Stall(25%) ---- */
        a = b + c;   /* produces a */
        c = c + b;   /* unrelated instruction in between */
        d = a + b;   /* consumes a after one gap -> 1 Stall */

        /*
        $L5:   # Shows this 
        addu $16,$17,$18    # a = b + c   
        addu $17,$16,$19    # b = a + d   → 2 Stalls (on a)
        addu $18,$17,$19    # c = b + d   → 2 Stalls (on b)
        addu $19,$18,$16    # d = c + a   → 2 Stalls (on c) 
        addu $16,$17,$18    # a = b + c   
        addu $18,$18,$19    # c = c + b   
        addu $19,$16,$17    # d = a + b   → 1 Stall (on a)
        */

        loop_cond++;
        random_inst = 0;
        random_inst = 0;
        if(loop_cond==N){
            break;
        }
    }

    /* Prevent optimizer from eliminating code */
    printf("%d\n", a + b + c + d);
    return 0;
}


