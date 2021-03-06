#pragma once
#include <limits.h>

#define MAX_EVALS_PER_CONTROLLER_NEUROEVOLUTION 2160
#define EVAL_MIN_STEP 72

#define MAX(A, B) ((A > B) ? A : B)
#define MIN(A, B) ((A < B) ? A : B)

#define SMALLEST_POSITIVE_DOUBLE  10e-70

#define TEMP_double_ARRAY_SIZE 30


#define DEPTH_OF_ORDER_MARGINAL 4
#define COMPUTE_ORDER_MARGINAL_EVERY_K_ITERATIONS 6





#define CUTOFF_0 0.25


#define MAX_POPSIZE 40
#define MIN_POPSIZE 4

#define MAX_TABU_LENGTH 100
#define MIN_TABU_LENGTH 5
#define LENGTH_CHANGE_STEP 5