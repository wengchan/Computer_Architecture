#include <stdio.h>

int main(){
    register int i = 0;
    register int j = 0;
    int dummy_variable = 0;

    // Expected outcome: near-perfect prediction accuracy
    // With a 6-bit history register, the repeating branch pattern (T T T T T T NT)
    // has a total period of 7, which is covered by predictor’s history window without aliasing.
    // Thus, the two-level predictor should achieve almost perfect accuracy.
    // Corresponding assembly structure:
    // .L4:
    //     movl    $1, -20(%rbp)
    //     addl    $1, %r12d
    // .L3:
    //     cmpl    $5, %r12d
    //     jle     .L4       // inner loop
    //     addl    $1, %ebx
    // .L2:
    //     cmpl    $99999, %ebx
    //     jle     .L5       // outer loop
    // Note: the two jump instructions are 8 bytes apart, so they occupy different PHT entries and do not alias.
    // Execution results:
    // NUM_INSTRUCTIONS           :  3177437
    // NUM_CONDITIONAL_BR         :    819165
    // 2level: NUM_MISPREDICTIONS :      1827
    // 2level: MISPRED_PER_1K_INST:      0.575
    //
    // The misprediction count (~1.8K) is dramatically lower than 100K,
    // confirming that the two-level predictor can handle this branch pattern
    // with near-perfect accuracy.
    //
    // for (i = 0; i < 100000; i++) {
    //     for (j = 0; j < 6; j++) {
    //         dummy_variable = 1;
    //     }
    // }
    //
    //
    //
    // Changing the loop structure:
    // When the inner loop count increases, the repeating branch pattern length
    // extends to 8, exceeding the 6-bit history coverage.
    // This produces significant aliasing and a dramatic increase in mispredictions.
    //
    // Expected outcome:
    // NUM_INSTRUCTIONS           :  3577437
    // NUM_CONDITIONAL_BR         :    919165
    // 2level: NUM_MISPREDICTIONS :    101825
    // 2level: MISPRED_PER_1K_INST:     28.463
    //
    // The sharp rise in mispredictions confirms that the predictor behaves
    // exactly as expected—its accuracy drops when the branch pattern period
    // exceeds the history length, validating the correctness of the implementation.
    for (i=0; i < 100000; i++){
        for (j = 0; j < 7; j++){
            dummy_variable = 1;
        }
    }
    return 0;
}
 