#include "predictor.h"
#include "utils.h"
#include <vector>

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////
#define PHT_SIZE_2BITSAT  4096
static std::vector<UINT32> PHT_2bitsat;

void InitPredictor_2bitsat() {
  PHT_2bitsat.resize(PHT_SIZE_2BITSAT , 1);    // initialize the PHT to weak not-taken (01)
}

bool GetPrediction_2bitsat(UINT32 PC) {
  UINT32 index = PC % PHT_SIZE_2BITSAT;        // use lower bits of PC as index
  UINT32 counter = PHT_2bitsat[index];
  return (counter >= 2) ? TAKEN : NOT_TAKEN;   // predict taken if counter is 2 or 3
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  UINT32 index = PC % PHT_SIZE_2BITSAT;
  UINT32 &counter = PHT_2bitsat[index];

  if (resolveDir == TAKEN)
    counter = SatIncrement(counter, 3);  // saturate at 3
  else
    counter = SatDecrement(counter);     // saturate at 0
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////
#define BHT_SIZE       512              // Number of local history entries
#define HISTORY_BITS   6                // Each history entry = 6 bits
#define PHT_GROUPS     8                // Number of PHTs
#define PHT_ENTRIES    (1 << HISTORY_BITS)  // 64 entries per PHT
static UINT32 BHT[BHT_SIZE];                // Branch History Table
static UINT32 PHT[PHT_GROUPS][PHT_ENTRIES]; // Pattern History Tables (2-bit counters)

void InitPredictor_2level() {
  // Initialize all BHT entries to 0
  for (int i = 0; i < BHT_SIZE; i++) {
      BHT[i] = 0;
  }

  // Initialize all PHT counters to weak not-taken
  for (int i = 0; i < PHT_GROUPS; i++) {
      for (int j = 0; j < PHT_ENTRIES; j++) {
          PHT[i][j] = 1;
      }
  }
}

bool GetPrediction_2level(UINT32 PC) {
  UINT32 PHT_index = PC & (PHT_GROUPS - 1);           // lowest 3 bits
  UINT32 BHT_index = (PC >> 3) & (BHT_SIZE - 1);      // next 9 bits
  UINT32 history   = BHT[BHT_index] & ((1 << HISTORY_BITS) - 1);
  UINT32 counter = PHT[PHT_index][history];
  return (counter >= 2) ? TAKEN : NOT_TAKEN; // Predict TAKEN if counter is 2 or 3
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  UINT32 PHT_index = PC & (PHT_GROUPS - 1);
  UINT32 BHT_index = (PC >> 3) & (BHT_SIZE - 1);
  UINT32 history   = BHT[BHT_index] & ((1 << HISTORY_BITS) - 1);

  // Update the corresponding PHT counter
  UINT32 &counter = PHT[PHT_index][history];
  if (resolveDir == TAKEN) {
      counter = SatIncrement(counter, 3);
  } else {
      counter = SatDecrement(counter);
  }

  // Update the BHT entry by shifting in the latest outcome
  BHT[BHT_index] = ((BHT[BHT_index] << 1) | (resolveDir ? 1 : 0)) & ((1 << HISTORY_BITS) - 1);
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////

// Configuration for the TAGE predictor
#define T0_SIZE        8192u   // base predictor entries, 2 bits counter

#define NUM_TAGS       6u      // six tagged tables
static const UINT32 TAG_SIZES[NUM_TAGS] = {1024u, 1024u, 512u, 512u, 256u, 128u}; // entries per tagged table
static const UINT32 HIST_LEN[NUM_TAGS]  = {4u, 7u, 13u, 24u, 44u, 80u};         // geometric history lengths for each table

#define TAG_BITS       12u     // bits for tags
#define U_BITS         2u      // usefulness counter bits
#define COUNTER_BITS   2u      // 2-bit prediction counters
#define GHIST_LEN      128u     // global history bits

#define COUNTER_MAX    ((1u << COUNTER_BITS) - 1u) // 3
#define U_MAX          ((1u << U_BITS) - 1u)       // 3

// Base predictor
static UINT32 basePHT[T0_SIZE];

// Tag table backing stores
static const UINT32 TAG_MAX_SIZE = 1024u;
static UINT32 tagCounterStore[NUM_TAGS][TAG_MAX_SIZE];
static UINT32 tagTagStore[NUM_TAGS][TAG_MAX_SIZE];
static UINT32 tagUStore[NUM_TAGS][TAG_MAX_SIZE];

// Pointers to backing stores
static UINT32 *tagCounter[NUM_TAGS];
static UINT32 *tagTag[NUM_TAGS];
static UINT32 *tagU[NUM_TAGS];

// Global history register
static UINT32 global_history = 0u;
static const UINT32 ghist_mask = (GHIST_LEN >= 32) ? 0xFFFFFFFFu : ((1u << GHIST_LEN) - 1u);

// Adaptive housekeeping
static UINT32 update_counter = 0u;
static const UINT32 U_DECAY_PERIOD = 4096u;

// Helper functions
static inline UINT32 p2mask(UINT32 x) { 
  return x - 1u; 
}

static inline UINT32 fold_history(UINT32 hist, UINT32 len, UINT32 out_bits) {
  if (out_bits == 0) return 0u;
  UINT32 res = 0u;
  UINT32 i = 0;
  while (i < len) {
    UINT32 chunk = (hist >> i) & ((1u << out_bits) - 1u);
    res ^= chunk;
    i += out_bits;
  }
  return res & ((1u << out_bits) - 1u);
}

static inline UINT32 compute_index_t(UINT32 PC, UINT32 t) {
  UINT32 size = TAG_SIZES[t];
  UINT32 hlen = HIST_LEN[t];
  UINT32 idx_bits = 0;
  { UINT32 tmp = size; while(tmp>1u){ tmp>>=1u; ++idx_bits; } }
  UINT32 folded = fold_history(global_history, hlen, idx_bits);
  return ((PC ^ folded) & p2mask(size));
}

static inline UINT32 compute_tag_t(UINT32 PC, UINT32 t) {
  UINT32 hlen = HIST_LEN[t];
  UINT32 folded = fold_history(global_history, hlen, TAG_BITS);
  return (((PC >> 2) ^ folded) & ((1u << TAG_BITS) - 1u));
}

// Initialization
void InitPredictor_openend() {
  for (UINT32 i = 0; i < T0_SIZE; ++i) basePHT[i] = 1u;
  for (UINT32 t = 0; t < NUM_TAGS; ++t) {
    tagCounter[t] = tagCounterStore[t];
    tagTag[t]     = tagTagStore[t];
    tagU[t]       = tagUStore[t];
    UINT32 sz = TAG_SIZES[t];
    for (UINT32 e = 0; e < sz; ++e) {
      tagCounter[t][e] = 1u;
      tagTag[t][e]     = 0u;
      tagU[t][e]       = 0u;
    }
  }
  global_history = 0u;
  update_counter = 0u;
}

// Prediction
bool GetPrediction_openend(UINT32 PC) {
  UINT32 base_idx = PC & p2mask(T0_SIZE);
  bool basePred = (basePHT[base_idx] >= 2u);

  int provider_t = -1;
  UINT32 provider_idx = 0u;
  for (int t = (int)NUM_TAGS - 1; t >= 0; --t) {
    UINT32 idx = compute_index_t(PC, (UINT32)t);
    UINT32 tag = compute_tag_t(PC, (UINT32)t);
    if (tagTag[t][idx] == tag && tagTag[t][idx] != 0u) {
      provider_t = t;
      provider_idx = idx;
      break;
    }
  }

  if (provider_t == -1) return basePred ? TAKEN : NOT_TAKEN;

  return (tagCounter[provider_t][provider_idx] >= 2u) ? TAKEN : NOT_TAKEN;
}

// Update
void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  ++update_counter;
  UINT32 base_idx = PC & p2mask(T0_SIZE);
  bool basePred = (basePHT[base_idx] >= 2u);

  int provider_t = -1, alt_t = -1;
  UINT32 provider_idx = 0u, alt_idx = 0u;
  for (int t = (int)NUM_TAGS - 1; t >= 0; --t) {
    UINT32 idx = compute_index_t(PC, (UINT32)t);
    UINT32 tag = compute_tag_t(PC, (UINT32)t);
    if (tagTag[t][idx] == tag && tagTag[t][idx] != 0u) {
      if (provider_t == -1) { provider_t = t; provider_idx = idx; }
      else if (alt_t == -1) { alt_t = t; alt_idx = idx; }
    }
  }

  bool altPred = (alt_t != -1) ? (tagCounter[alt_t][alt_idx] >= 2u) : basePred;
  bool providerPred = (provider_t != -1) ? (tagCounter[provider_t][provider_idx] >= 2u) : basePred;

  // Base table always updated
  if(resolveDir==TAKEN) basePHT[base_idx] = SatIncrement(basePHT[base_idx], COUNTER_MAX);
  else basePHT[base_idx] = SatDecrement(basePHT[base_idx]);

  // Provider update
  if(provider_t != -1) {
    if(resolveDir==TAKEN) tagCounter[provider_t][provider_idx] = SatIncrement(tagCounter[provider_t][provider_idx], COUNTER_MAX);
    else tagCounter[provider_t][provider_idx] = SatDecrement(tagCounter[provider_t][provider_idx]);

    bool provider_correct = (providerPred == (resolveDir==TAKEN));
    bool alt_correct = (altPred == (resolveDir==TAKEN));
    if(provider_correct && !alt_correct) tagU[provider_t][provider_idx] = SatIncrement(tagU[provider_t][provider_idx], U_MAX);
    else if(!provider_correct && alt_correct) tagU[provider_t][provider_idx] = SatDecrement(tagU[provider_t][provider_idx]);
  } else {
    bool finalPred = basePred;
    if(finalPred != (resolveDir==TAKEN)) {
      // allocate in longest first with u==0
      for(int t=(int)NUM_TAGS-1; t>=0; --t) {
        UINT32 idx = compute_index_t(PC, (UINT32)t);
        if(tagU[t][idx]==0u){
          tagTag[t][idx] = compute_tag_t(PC, (UINT32)t);
          // bias new allocation based on basePred
          tagCounter[t][idx] = (resolveDir==TAKEN) ? (basePred?3u:2u) : (basePred?1u:0u);
          tagU[t][idx] = 0u;
          break;
        }
      }
    }
  }

  // Partial U decay (half per period)
  if((update_counter & (U_DECAY_PERIOD-1u))==0u){
    static UINT32 decay_start=0u;
    for(UINT32 t=0; t<NUM_TAGS; ++t){
      UINT32 sz = TAG_SIZES[t];
      for(UINT32 e=0; e<sz/2; ++e){
        UINT32 idx = (decay_start+e) & p2mask(sz);
        tagU[t][idx] >>= 1u;
      }
    }
    decay_start = (decay_start + TAG_MAX_SIZE/2) & p2mask(TAG_MAX_SIZE);
  }

  // Update global history
  global_history = ((global_history << 1) | (resolveDir ? 1u : 0u)) & ghist_mask;
}