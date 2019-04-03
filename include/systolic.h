// See LICENSE for license details.

#ifndef SRC_MAIN_C_SYSTOLIC_H
#define SRC_MAIN_C_SYSTOLIC_H

#include <stdint.h>
#include <assert.h>

// Dimension of the systolic array
// Should be tileColumns*meshColumns
#define DIM 4
// Datatype of the systolic array
// TODO: Should be signed, but then need to add a bias term below
typedef uint16_t elem_t;

// Matmul utility functions
void matmul(elem_t A[DIM][DIM], elem_t B[DIM][DIM], elem_t D[DIM][DIM], elem_t C[DIM][DIM]) {
  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++) {
      C[r][c] = D[r][c];
      for (size_t k = 0; k < DIM; k++)
        C[r][c] += A[r][k]*B[k][c];
    }
}

void matadd(elem_t x[DIM][DIM], elem_t y[DIM][DIM], elem_t sum[DIM][DIM]) {
  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++)
        sum[r][c] = x[r][c] + y[r][c];
}

void transpose(elem_t in[DIM][DIM], elem_t out[DIM][DIM]) {
  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++)
      out[c][r] = in[r][c];
}

void printMatrix(elem_t m[DIM][DIM]) {
  for (size_t i = 0; i < DIM; ++i) {
    for (size_t j = 0; j < DIM; ++j)
      printf("%d ", m[i][j]);
    printf("\n");
  }
}

int is_equal(elem_t x[DIM][DIM], elem_t y[DIM][DIM]) {
  for (size_t i = 0; i < DIM; ++i)
    for (size_t j = 0; j < DIM; ++j)
      if (x[i][j] != y[i][j])
          return 0;
  return 1;
}

int rand() {
  static uint32_t x = 777;
  x = x * 1664525 + 1013904223;
  return x >> 24;
}

// Accelerator interface
#include "rocc-software/src/xcustom.h"

#define k_SETMODE 0
#define k_MVIN 2
#define k_MVOUT 3
#define k_COMPUTE_PRELOADED 4
#define k_COMPUTE_ACCUMULATE 5
#define k_PRELOAD 6

#define XCUSTOM_ACC 3

#define GARBAGE_ADDR ((uint64_t)(-1))
#define OUTPUT_STATIONARY 0
#define WEIGHT_STATIONARY 1

#define ROCC_INSTRUCTION_RS1_RS2(x, rs1, rs2, funct) \
  ROCC_INSTRUCTION_0_R_R(x, rs1, rs2, funct, 10, 11)

#define to_deps(push1, pop1, push2, pop2) \
  (((push1 << 3) | (pop1 << 2) | (push2 << 1) | pop2) << 3)

// mvin and mvout
#define matmul_mvin(dram_addr, spad_addr, push_mvout, pop_mvout, push_ex, pop_ex) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, dram_addr, spad_addr, to_deps(push_mvout, pop_mvout, push_ex, pop_ex) | k_MVIN)

#define matmul_mvout(dram_addr, spad_addr, push_mvin, pop_mvin, push_ex, pop_ex) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, dram_addr, spad_addr, to_deps(push_mvin, pop_mvin, push_ex, pop_ex) | k_MVOUT)

// compute
#define matmul_compute_preloaded(A, B, push_mvin, pop_mvin, push_mvout, pop_mvout) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, A, B, to_deps(push_mvin, pop_mvin, push_mvout, pop_mvout) | k_COMPUTE_PRELOADED)

#define matmul_compute_accumulated(A, B, push_mvin, pop_mvin, push_mvout, pop_mvout) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, A, B,  to_deps(push_mvin, pop_mvin, push_mvout, pop_mvout) | k_COMPUTE_ACCUMULATE)

// preload
#define matmul_preload(D, C, push_mvin, pop_mvin, push_mvout, pop_mvout) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, D, C, to_deps(push_mvin, pop_mvin, push_mvout, pop_mvout) | k_PRELOAD)

#define matmul_preload_zeros(C, push_mvin, pop_mvin, push_mvout, pop_mvout) \
  matmul_preload(GARBAGE_ADDR, C, push_mvin, pop_mvin, push_mvout, pop_mvout)

// config
#define matmul_setmode(mode, push_mvin, pop_mvin, push_mvout, pop_mvout) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, mode, 0, to_deps(push_mvin, pop_mvin, push_mvout, pop_mvout) | k_SETMODE)

#endif  // SRC_MAIN_C_SYSTOLIC_H

