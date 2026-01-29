
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

/* ECE552 Assignment 3 - BEGIN CODE */
#define INSTR_QUEUE_SIZE         16

#define RESERV_INT_SIZE    5
#define RESERV_FP_SIZE     3
#define FU_INT_SIZE        3
#define FU_FP_SIZE         1

#define FU_INT_LATENCY     5
#define FU_FP_LATENCY      7
/* ECE552 Assignment 3 - END CODE */

/* IDENTIFYING INSTRUCTIONS */

//unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                         MD_OP_FLAGS(op) & F_UNCOND)

//conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

//floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

//integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

//load instruction
#define IS_LOAD(op)  (MD_OP_FLAGS(op) & F_LOAD)

//store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

//trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP) 

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

//prints info about an instruction
#define PRINT_INST(out,instr,str,cycle)	\
  myfprintf(out, "%d: %s", cycle, str);		\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

#define PRINT_REG(out,reg,str,instr) \
  myfprintf(out, "reg#%d %s ", reg, str);	\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

/* VARIABLES */

//instruction queue for tomasulo
static instruction_t* instr_queue[INSTR_QUEUE_SIZE];
//number of instructions in the instruction queue
static int instr_queue_size = 0;

//reservation stations (each reservation station entry contains a pointer to an instruction)
static instruction_t* reservINT[RESERV_INT_SIZE];
static instruction_t* reservFP[RESERV_FP_SIZE];

//functional units
static instruction_t* fuINT[FU_INT_SIZE];
static instruction_t* fuFP[FU_FP_SIZE];

//common data bus
static instruction_t* commonDataBus = NULL;

//The map table keeps track of which instruction produces the value for each register
static instruction_t* map_table[MD_TOTAL_REGS];

//the index of the last instruction fetched
static int fetch_index = 0;

/* FUNCTIONAL UNITS */


/* RESERVATION STATIONS */


/* 
 * Description: 
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  // All instructions fetched and pipeline empty
  if (fetch_index < sim_num_insn)
    return false;

  // Check if all reservation stations are free
  int i;
  for (i = 0; i < RESERV_INT_SIZE; i++) {
    if (reservINT[i] != NULL)
      return false;
  }
  for (i = 0; i < RESERV_FP_SIZE; i++) {
    if (reservFP[i] != NULL)
      return false;
  }

  // Check if all functional units are free
  for (i = 0; i < FU_INT_SIZE; i++) {
    if (fuINT[i] != NULL)
      return false;
  }
  for (i = 0; i < FU_FP_SIZE; i++) {
    if (fuFP[i] != NULL)
      return false;
  }

  // Check if CDB is free
  if (commonDataBus != NULL)
    return false;

  // Check if instruction queue is empty
  if (instr_queue_size != 0)
    return false;

  return true;
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  if (commonDataBus == NULL || !WRITES_CDB(commonDataBus->op)) {
    return;
  }

  instruction_t *inst = commonDataBus;

  // For each reservation station, check if waiting on this producer
  for (int i = 0; i < RESERV_INT_SIZE; i++) {
    if (reservINT[i] == NULL)
      continue;
    for (int j = 0; j < 3; j++) {
      if (reservINT[i]->Q[j] == inst)
        reservINT[i]->Q[j] = NULL;
    }
  }
  for (int i = 0; i < RESERV_FP_SIZE; i++) {
    if (reservFP[i] == NULL)
      continue;
    for (int j = 0; j < 3; j++) {
      if (reservFP[i]->Q[j] == inst)
        reservFP[i]->Q[j] = NULL;
    }
  }

  // Update register map table
  for (int i = 0; i < 2; i++) {
    int reg = inst->r_out[i];
    if (reg >= 0 && map_table[reg] == inst)
      map_table[reg] = NULL;
  }

  // Clear CDB for next cycle
  commonDataBus = NULL;
  /* ECE552 Assignment 3 - END CODE */
}



/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  if (commonDataBus) return;
  instruction_t *oldest_ready = NULL;
  int oldest_cycle = INT_MAX;

  // Phase 1: Scan INT FUs
  for (int i = 0; i < FU_INT_SIZE; i++) {
    instruction_t *inst = fuINT[i];
    if (!inst) continue;

    // If instruction finished execution
    if (current_cycle >= inst->tom_execute_cycle + FU_INT_LATENCY - 1) {
      // Non-CDB writers complete immediately
      if (!WRITES_CDB(inst->op)) {
        fuINT[i] = NULL;
        for (int j = 0; j < RESERV_INT_SIZE; j++){
          if (reservINT[j] == inst){
            reservINT[j] = NULL;
            break;
          }
        }
        inst->tom_cdb_cycle = 0;
        continue;
      }

      // Potential CDB candidate
      if (inst->tom_dispatch_cycle < oldest_cycle) {
        oldest_ready = inst;
        oldest_cycle = inst->tom_dispatch_cycle;
      }
    }
  }

  // Phase 2: Scan FP FUs
  for (int i = 0; i < FU_FP_SIZE; i++) {
    instruction_t *inst = fuFP[i];
    if (!inst) continue;

    if (current_cycle >= inst->tom_execute_cycle + FU_FP_LATENCY - 1) {
      if (inst->tom_dispatch_cycle < oldest_cycle) {
        oldest_ready = inst;
        oldest_cycle = inst->tom_dispatch_cycle;
      }
    }
  }

  // Phase 3: Broadcast oldest instruction (if CDB available)
  if (oldest_ready) {
    commonDataBus = oldest_ready;

    commonDataBus->tom_cdb_cycle = current_cycle + 1 ;

    // Free FU and RS entry
    bool is_fp = USES_FP_FU(commonDataBus->op);
    if (is_fp) {
      for (int i = 0; i < FU_FP_SIZE; i++)
        if (fuFP[i] == commonDataBus)
            fuFP[i] = NULL;
      for (int j = 0; j < RESERV_FP_SIZE; j++)
        if (reservFP[j] == commonDataBus)
            reservFP[j] = NULL;
    } else {
      for (int i = 0; i < FU_INT_SIZE; i++)
        if (fuINT[i] == commonDataBus)
          fuINT[i] = NULL;
      for (int j = 0; j < RESERV_INT_SIZE; j++)
        if (reservINT[j] == commonDataBus)
          reservINT[j] = NULL;
    }
  }
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  for(int i = 0; i < FU_INT_SIZE; i++){
    if(fuINT[i] != NULL) continue;
    instruction_t* oldest_instr_execute = NULL;
    int dispatch_cycle = INT_MAX;

    for(int j = 0; j < RESERV_INT_SIZE; j++){
      instruction_t *execute_instr = reservINT[j];
      // look for instr that has not yet start execution and is not issued in the same cycle
      if(execute_instr == NULL || execute_instr->tom_execute_cycle > 0 || execute_instr->tom_issue_cycle == current_cycle) continue;

      bool ready = true;
      bool using_cdb_this_cycle = false;

      for (int k = 0; k < 3; k++) {
        if (execute_instr->Q[k] != NULL) {
          if (execute_instr->Q[k] == commonDataBus) {
            using_cdb_this_cycle = true;
          } else {
            ready = false;
            break;
          }
        }
      }

      if(ready && !using_cdb_this_cycle && execute_instr->tom_dispatch_cycle < dispatch_cycle){
        oldest_instr_execute = execute_instr;
        dispatch_cycle = execute_instr->tom_dispatch_cycle;
      }
    }

    if(oldest_instr_execute != NULL){
      fuINT[i] = oldest_instr_execute;
      oldest_instr_execute->tom_execute_cycle = current_cycle;
    }else{
      break;
    }
  }

  for(int i = 0; i < FU_FP_SIZE; i++){
    if(fuFP[i] != NULL) continue;
    instruction_t* oldest_instr_execute = NULL;
    int dispatch_cycle = INT_MAX;
    for(int j = 0; j < RESERV_FP_SIZE; j++){
      instruction_t *execute_instr = reservFP[j];
      // look for instr that has not yet start execution and is not issued in the same cycle
      if(execute_instr == NULL || execute_instr->tom_execute_cycle > 0) continue;

      bool ready = true;
      bool using_cdb_this_cycle = false;

      for (int k = 0; k < 3; k++) {
        if (execute_instr->Q[k] != NULL) {
          if (execute_instr->Q[k] == commonDataBus) {
            using_cdb_this_cycle = true;
          } else {
            ready = false;
            break;
          }
        }
      }

      if(ready && !using_cdb_this_cycle && execute_instr->tom_dispatch_cycle < dispatch_cycle){
        oldest_instr_execute = execute_instr;
        dispatch_cycle = execute_instr->tom_dispatch_cycle;
      }
    }

    if(oldest_instr_execute != NULL){
      fuFP[i] = oldest_instr_execute;
      oldest_instr_execute->tom_execute_cycle = current_cycle;
    }else{
      break;
    }
  }
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  instruction_t* issue_instr = instr_queue[0];
  if(issue_instr != NULL && issue_instr->tom_dispatch_cycle != current_cycle){
    bool filled_RS = false;
    if(USES_FP_FU(issue_instr->op)){
      for(int i = 0; i <RESERV_FP_SIZE; ++i){
        if(!reservFP[i]){
          reservFP[i] = issue_instr;
          issue_instr->tom_issue_cycle = current_cycle;
          filled_RS = true;
          break;
        }
      }
    }
    if(USES_INT_FU(issue_instr->op)){
      for(int i = 0; i <RESERV_INT_SIZE; i++){
        if(!reservINT[i]){
          reservINT[i] = issue_instr;
          issue_instr->tom_issue_cycle = current_cycle;
          filled_RS = true;
          break;
        }
      }
    }
    if(filled_RS){
      // shift every remaining element forward by one slot
      for (int i = 0; i < instr_queue_size - 1; i++) {
        instr_queue[i] = instr_queue[i + 1];
      }
      instr_queue[instr_queue_size - 1] = NULL;
      instr_queue_size--;
    }
    // If there are no stalls, a reservation station entry is allocated based on each instruction’s
    // opcode. Any RAW dependences are marked in the reservation entry.
    // map table value in RS entry, first input, then output
    for(int i = 0; i < 3; i++){
      if(issue_instr->r_in[i] != DNA && filled_RS && map_table[issue_instr->r_in[i]]){
        issue_instr->Q[i] = map_table[issue_instr->r_in[i]];
      }
    }
    //store doesn't have output reg
    for(int i = 0; i < 2; i++){
      if(!IS_STORE(issue_instr->op)){
        if(issue_instr->r_out[i] != DNA && filled_RS){
          map_table[issue_instr->r_out[i]] = issue_instr;
        }
      }
    }
  } 
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace, int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  if(instr_queue_size < INSTR_QUEUE_SIZE && fetch_index < sim_num_insn){
    instruction_t* dispatch_instr = NULL;
    do{
      dispatch_instr = get_instr(trace, ++fetch_index);
    }while(IS_TRAP(dispatch_instr->op)); 
    dispatch_instr->tom_dispatch_cycle = current_cycle;
    instr_queue[instr_queue_size] = dispatch_instr;
    instr_queue_size++;
  }
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  instruction_t* issue_instr = instr_queue[0];
  if(issue_instr != NULL && (IS_UNCOND_CTRL(issue_instr->op) || IS_COND_CTRL(issue_instr->op))){
    for (int i = 0; i < instr_queue_size - 1; i++) {
      instr_queue[i] = instr_queue[i + 1];
    }
    instr_queue[instr_queue_size - 1] = NULL;
    instr_queue_size--;
  }
  fetch(trace,current_cycle);
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace)
{
  //initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
    instr_queue[i] = NULL;
  }

  //initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE; i++) {
      reservINT[i] = NULL;
  }

  for(i = 0; i < RESERV_FP_SIZE; i++) {
      reservFP[i] = NULL;
  }

  //initialize functional units
  for (i = 0; i < FU_INT_SIZE; i++) {
    fuINT[i] = NULL;
  }

  for (i = 0; i < FU_FP_SIZE; i++) {
    fuFP[i] = NULL;
  }

  //initialize map_table to no producers
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
    map_table[reg] = NULL;
  }
  
  int cycle = 1;
  while (true) {
    /* ECE552 Assignment 3 - BEGIN CODE */
    fetch_To_dispatch(trace,cycle);
    dispatch_To_issue(cycle);
    issue_To_execute(cycle);
    // CDB_To_retire runs before execute_To_CDB effectively models the situation 
    // where an instruction finish EXECUTION stage in cycle 9 and uses CDB in cycle 10.
    CDB_To_retire(cycle);
    execute_To_CDB(cycle);
    /* ECE552 Assignment 3 - END CODE */

    cycle++;

    if (is_simulation_done(sim_num_insn))
      break;
  }
  
  return cycle;
}
