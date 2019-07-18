// See LICENSE for license details.

#ifndef SRC_MAIN_C_SYSTOLIC_H
#define SRC_MAIN_C_SYSTOLIC_H

#include <stdint.h>
#include <math.h>
#include <limits.h>
// TODO use stdbool.h as well

// Dimension of the systolic array
// Should be tileColumns*meshColumns
#define DIM 16
#define ADDR_LEN 32
#define BANK_NUM 4
// Unforunately, using sizeof in a macro is problematic, so we use 1 instead of
// sizeof(elem_t) and 4 instead of sizeof(acc_t)
#define BANK_ROWS (256 * 1024 / (BANK_NUM * DIM*1))
#define ACC_ROWS (64 * 1024 / (DIM*4))
#define MAX_BYTES 64
#define MAX_BLOCK_LEN (MAX_BYTES/(DIM*1))
#define MAX_BLOCK_LEN_ACC (MAX_BYTES/(DIM*4))

#define MAX_Q_LEN 256

// Datatype of the systolic array
typedef int8_t elem_t;
elem_t elem_t_max = SCHAR_MAX;
elem_t elem_t_min = SCHAR_MIN;
typedef int32_t acc_t;

#define row_align(blocks) __attribute__((aligned(blocks*DIM*sizeof(elem_t))))
#define row_align_acc(blocks) __attribute__((aligned(blocks*DIM*sizeof(acc_t))))

// Matmul utility functions
void matmul(elem_t A[DIM][DIM], elem_t B[DIM][DIM], elem_t D[DIM][DIM], int64_t C_full[DIM][DIM]) {
  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++) {
      C_full[r][c] = D[r][c];
      for (size_t k = 0; k < DIM; k++)
        C_full[r][c] += A[r][k]*B[k][c];
    }
}

void matmul_short(elem_t A[DIM][DIM], elem_t B[DIM][DIM], elem_t D[DIM][DIM], elem_t C[DIM][DIM]) {
  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++) {
      C[r][c] = D[r][c];
      for (size_t k = 0; k < DIM; k++)
        C[r][c] += A[r][k]*B[k][c];
    }
}

void matmul_full(elem_t A[DIM][DIM], elem_t B[DIM][DIM], int64_t D[DIM][DIM], int64_t C_full[DIM][DIM]) {
  // Identical to the other matmul function, but with a 64-bit bias
  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++) {
      C_full[r][c] = D[r][c];
      for (size_t k = 0; k < DIM; k++)
        C_full[r][c] += A[r][k]*B[k][c];
    }
}

void matadd(int64_t sum[DIM][DIM], int64_t m1[DIM][DIM], int64_t m2[DIM][DIM]) {
  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++)
      sum[r][c] = m1[r][c] + m2[r][c];
}

// THIS IS A ROUNDING SHIFT! It also performs a saturating cast
void matshift(int64_t full[DIM][DIM], elem_t out[DIM][DIM], int shift) {
  int divisor = 1 << shift;

  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++) {
      // Bitshift and round element
      int64_t abs = full[r][c] > 0 ? full[r][c] : -full[r][c];
      int64_t shifted = (abs + (divisor/2)) / divisor;
      if (full[r][c] < 0)
        shifted = -shifted;

      // Saturate and cast element
      int64_t elem = shifted > elem_t_max ? elem_t_max : (shifted < elem_t_min ? elem_t_min : shifted);
      out[r][c] = elem;
    }
}

void matrelu(elem_t in[DIM][DIM], elem_t out[DIM][DIM]) {
  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++)
      out[r][c] = in[r][c] > 0 ? in[r][c] : 0;
}

void matrelu6(elem_t in[DIM][DIM], elem_t out[DIM][DIM], int scale) {
  // int max = 6;
  int max = 6 * scale;

  for (size_t r = 0; r < DIM; r++)
    for (size_t c = 0; c < DIM; c++) {
      elem_t positive = in[r][c] > 0 ? in[r][c] : 0;
      out[r][c] = positive > max ? max : positive;
    }
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

unsigned long read_cycles() {
#ifndef TEST_QUEUES
    unsigned long cycles;
    asm volatile ("rdcycle %0" : "=r" (cycles));
    return cycles;
#else
    return 0;
#endif
}

// Accelerator interface
#ifndef TEST_QUEUES
#include "rocc-software/src/xcustom.h"
#endif

#define k_CONFIG 0
#define k_MVIN 2
#define k_MVOUT 3
#define k_COMPUTE_PRELOADED 4
#define k_COMPUTE_ACCUMULATE 5
#define k_PRELOAD 6
#define k_FLUSH 7

#define CONFIG_EX 0
#define CONFIG_LD 1
#define CONFIG_ST 2

#define XCUSTOM_ACC 3

#define GARBAGE_ADDR ((uint64_t)(-1))
#define OUTPUT_STATIONARY 0
#define WEIGHT_STATIONARY 1

#define NO_ACTIVATION 0
#define RELU 1
#define RELU6 2

#ifndef TEST_QUEUES
#define ROCC_INSTRUCTION_RS1_RS2(x, rs1, rs2, funct) \
  ROCC_INSTRUCTION_0_R_R(x, rs1, rs2, funct, 10, 11)
#else
static int ld_to_st_q = 0;
static int ld_to_ex_q = 0;
static int st_to_ld_q = 0;
static int st_to_ex_q = 0;
static int ex_to_ld_q = 0;
static int ex_to_st_q = 0;

static void push_pull_queues(int funct_, int rs1) {
  int deps = funct_ >> 3;
  int funct = funct_ & 7;
  int config = rs1 & 3;

  int push1 = (deps >> 3) & 1;
  int pull1 = (deps >> 2) & 1;
  int push2 = (deps >> 1) & 1;
  int pull2 = deps & 1;

  int is_ld = funct == k_MVIN || (funct == k_CONFIG && config == CONFIG_LD);
  int is_st = funct == k_MVOUT || (funct == k_CONFIG && config == CONFIG_ST);
  int is_ex = funct == k_PRELOAD || funct == k_COMPUTE_PRELOADED || funct == k_COMPUTE_ACCUMULATE || (funct == k_CONFIG && config == CONFIG_EX);

  if (is_ld) {
    ld_to_st_q += push1;
    ld_to_ex_q += push2;
    st_to_ld_q -= pull1;
    ex_to_ld_q -= pull2;
  } else if (is_st) {
    st_to_ld_q += push1;
    st_to_ex_q += push2;
    ld_to_st_q -= pull1;
    ex_to_st_q -= pull2;
  } else if (is_ex) {
    ex_to_ld_q += push1;
    ex_to_st_q += push2;
    ld_to_ex_q -= pull1;
    st_to_ex_q -= pull2;
  }

  if (ld_to_st_q > MAX_Q_LEN || ld_to_st_q < 0) {
    printf("ld_to_st_q has impossible size: %d\n", ld_to_st_q);
    exit(1);
  }
  if (ld_to_ex_q > MAX_Q_LEN || ld_to_ex_q < 0) {
    printf("ld_to_ex_q has impossible size: %d\n", ld_to_ex_q);
    exit(1);
  }
  if (st_to_ld_q > MAX_Q_LEN || st_to_ld_q < 0) {
    printf("st_to_ld_q has impossible size: %d\n", st_to_ld_q);
    exit(1);
  }
  if (st_to_ex_q > MAX_Q_LEN || st_to_ex_q < 0) {
    printf("st_to_ex_q has impossible size: %d\n", st_to_ex_q);
    exit(1);
  }
  if (ex_to_ld_q > MAX_Q_LEN || ex_to_ld_q < 0) {
    printf("ex_to_ld_q has impossible size: %d\n", ex_to_ld_q);
    exit(1);
  }
  if (ex_to_st_q > MAX_Q_LEN || ex_to_st_q < 0) {
    printf("ex_to_st_q has impossible size: %d\n", ex_to_st_q);
    exit(1);
  }
}

#define ROCC_INSTRUCTION_RS1_RS2(x, rs1, rs2, funct) \
  push_pull_queues(funct, rs1)
#endif

#define to_deps(push1, pop1, push2, pop2) \
  (((push1 << 3) | (pop1 << 2) | (push2 << 1) | pop2) << 3)

// mvin and mvout
#define matmul_mvin(dram_addr, spad_addr, push_mvout, pop_mvout, push_ex, pop_ex) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, dram_addr, ((uint64_t)1 << ADDR_LEN) | (spad_addr), to_deps(push_mvout, pop_mvout, push_ex, pop_ex) | k_MVIN)

#define matmul_block_mvin(dram_addr, spad_addr, len, push_mvout, pop_mvout, push_ex, pop_ex) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, dram_addr, ((uint64_t)(len) << ADDR_LEN) | (spad_addr), (to_deps(push_mvout, pop_mvout, push_ex, pop_ex)) | (k_MVIN))

#define matmul_mvout(dram_addr, spad_addr, push_mvin, pop_mvin, push_ex, pop_ex) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, dram_addr, spad_addr, (to_deps(push_mvin, pop_mvin, push_ex, pop_ex)) | (k_MVOUT))

// compute
#define matmul_compute_preloaded(A, BD) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, A, BD, k_COMPUTE_PRELOADED)

#define matmul_compute_accumulated(A, BD) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, A, BD, k_COMPUTE_ACCUMULATE)

// preload
#define matmul_preload(BD, C, push_mvin, pop_mvin, push_mvout, pop_mvout) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, BD, C, (to_deps(push_mvin, pop_mvin, push_mvout, pop_mvout)) | (k_PRELOAD))

#define matmul_preload_zeros(C, push_mvin, pop_mvin, push_mvout, pop_mvout) \
  matmul_preload(GARBAGE_ADDR, C, push_mvin, pop_mvin, push_mvout, pop_mvout)

// config
#define matmul_config_ex(mode, act, shift, relu6_shift, push_mvin, pop_mvin, push_mvout, pop_mvout) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, ((act) << 3) | ((mode) << 2) | CONFIG_EX, ((uint64_t)relu6_shift << 32) | shift, (to_deps(push_mvin, pop_mvin, push_mvout, pop_mvout)) | (k_CONFIG))

#define matmul_config_ld(stride, push_mvout, pop_mvout, push_ex, pop_ex) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, CONFIG_LD, stride, (to_deps(push_mvout, pop_mvout, push_ex, pop_ex)) | (k_CONFIG))

#define matmul_config_st(stride, push_mvin, pop_mvin, push_ex, pop_ex) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, CONFIG_ST, stride, (to_deps(push_mvin, pop_mvin, push_ex, pop_ex)) | (k_CONFIG))

// flush
#define matmul_flush(skip) \
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, skip, 0, k_FLUSH)

// fence
#ifndef TEST_QUEUES
#define matmul_fence() asm volatile("fence")
#else
#define matmul_fence()
#endif

// Tiling functions
static void sp_tiled_matmul_os(elem_t * A, elem_t * B, acc_t * D, elem_t * C,
        size_t I, size_t J, size_t K, size_t A_row_len,
        size_t B_row_len, size_t D_row_len, size_t C_row_len,
        int first_mvin, int last_mvout, int no_bias) {

  const uint32_t A_sp_addr_start = 0;
  const uint32_t B_sp_addr_start = (BANK_NUM/2)*BANK_ROWS;
  const uint32_t D_sp_addr_start = 1 << (ADDR_LEN-1);
  const uint32_t C_sp_addr_start = 3 << (ADDR_LEN-2);

  const int A_blocks = K <= MAX_BLOCK_LEN ? K : MAX_BLOCK_LEN;
  const int B_blocks = J <= MAX_BLOCK_LEN ? J : MAX_BLOCK_LEN;
  const int D_blocks = J <= MAX_BLOCK_LEN_ACC ? J : MAX_BLOCK_LEN_ACC;

  const int I_iterations = I;
  const int J_iterations = (J/B_blocks + (J % B_blocks != 0));
  const int K_iterations = (K/A_blocks + (K % A_blocks != 0));
  const int total_iterations = I_iterations * J_iterations * K_iterations;

  int old_iterations = total_iterations;

  // Move-in D
  if (D != NULL && !no_bias) {
    matmul_config_ld(D_row_len * sizeof(acc_t), 0, 0, 0, 0);

    for (size_t i = 0; i < I; i++) {
      for (size_t j = 0; j < J; j++) {
        acc_t * const D_dram_addr = D + (i*D_row_len + j)*DIM;
        const uint32_t D_sp_addr = D_sp_addr_start + (i*J + j)*DIM;

        int already_moved_in = j % D_blocks != 0;

        if (!already_moved_in) {
          int blocks = j + D_blocks <= J ? D_blocks : J-j;

          if (first_mvin) {
            matmul_block_mvin(D_dram_addr, D_sp_addr, blocks, 0, 0, 0, 0);
          } else {
            matmul_block_mvin(D_dram_addr, D_sp_addr, blocks, 0, 1, 0, 0);
          }
        } else {
          matmul_config_ld(D_row_len * sizeof(acc_t), 0, 1, 0, 0);
        }
      }
    }
  }

  if (first_mvin) {
    matmul_config_ld(A_row_len * sizeof(elem_t), 0, 0, 0, 0);
  }

  for (size_t i = 0; i < I; i++) {
    for (size_t j = 0; j < J; j++) {
      const uint32_t C_sp_addr = C_sp_addr_start + (i*J + j)*DIM;

      for (size_t k = 0; k < K; k++) {
        // printf("  i: %u, j: %u, k: %u\n", i, j, k);

        elem_t * const A_dram_addr = A + (i*A_row_len + k)*DIM;
        elem_t * const B_dram_addr = B + (k*B_row_len + j)*DIM;

        const uint32_t A_sp_addr = A_sp_addr_start + (i*K + k)*DIM;
        const uint32_t B_sp_addr = B_sp_addr_start + (k*J + j)*DIM;

        // Move-in A and B
        {
          int A_already_moved_in = j != 0 || k % A_blocks != 0;
          int B_already_moved_in = i != 0 || j % B_blocks != 0;

          // Make sure the address we are moving into is not still being used by old compute instructions
          // TODO better names
          int iterations = i*J_iterations*K_iterations + (j/B_blocks)*K_iterations + (k/A_blocks);
          iterations = total_iterations - 1 - iterations;

          while (!first_mvin && old_iterations > iterations) {
            // printf("old: %d, it: %d\n", old_iterations, iterations);
            matmul_config_ld(A_row_len * sizeof(elem_t), 0, 0, 0, 1);
            old_iterations--;
          }

          if (!A_already_moved_in) {
            int blocks = k + A_blocks <= K ? A_blocks : K-k;
            matmul_block_mvin(A_dram_addr, A_sp_addr, blocks, 0, 0, 0, 0);
          }

          if (!B_already_moved_in) {
            int blocks = j + B_blocks <= J ? B_blocks : J-j;
            matmul_config_ld(B_row_len * sizeof(elem_t), 0, 0, 0, 0);
            matmul_block_mvin(B_dram_addr, B_sp_addr, blocks, 0, 0, 0, 0);
          }

          matmul_config_ld(A_row_len * sizeof(elem_t), 0, 0, 1, 0);
        }

        // Compute
        {
          uint32_t out_sp_addr = k == K-1 ? C_sp_addr : GARBAGE_ADDR;

          // If we're not using a bias, then we want to overwrite what's in the
          // accumulator, rather than writing over it
          int no_bias_new_matrix = no_bias && D != NULL && k == K-1;
          if (no_bias_new_matrix) {
            out_sp_addr &= ~(1 << (ADDR_LEN-2));
          }

          int final_submatrix = i == I-1 && j == J-1 && k == K-1 && C != NULL;

          // int last_A = j == (J - 1) && (k % A_blocks == (A_blocks - 1) || k == K-1);
          // int last_B = i == (I - 1) && (j % B_blocks == (B_blocks - 1) || j == J-1);
          int last_A = (k % A_blocks == (A_blocks - 1) || k == K-1);
          int last_B = (j % B_blocks == (B_blocks - 1) || j == J-1);
          // int push_to_load = !last_mvout && (last_A || last_B);
          int push_to_load = !last_mvout && (last_A && last_B);

          if (!first_mvin && no_bias_new_matrix && final_submatrix) {
            // Both first and last iteration, but only relevant when bias isn't
            // used
            if (push_to_load) {
              matmul_preload(GARBAGE_ADDR, out_sp_addr, 1, 1, 1, 1);
            } else {
              matmul_preload(GARBAGE_ADDR, out_sp_addr, 0, 1, 1, 1);
            }
          } else if (!first_mvin && no_bias_new_matrix) {
            // First iteration, but only relevant when bias isn't used
            if (push_to_load) {
              matmul_preload(GARBAGE_ADDR, out_sp_addr, 1, 1, 0, 1);
            } else {
              matmul_preload(GARBAGE_ADDR, out_sp_addr, 0, 1, 0, 1);
            }
          } else if (final_submatrix) {
            // Last iteration, when we calculate final sub-matrix
            if (push_to_load) {
              matmul_preload(GARBAGE_ADDR, out_sp_addr, 1, 1, 1, 0);
            } else {
              matmul_preload(GARBAGE_ADDR, out_sp_addr, 0, 1, 1, 0);
            }
          } else {
            // All other iterations
            if (push_to_load) {
              matmul_preload(GARBAGE_ADDR, out_sp_addr, 1, 1, 0, 0);
            } else {
              matmul_preload(GARBAGE_ADDR, out_sp_addr, 0, 1, 0, 0);
            }
          }

          if (k == 0) { // First iteration
            matmul_compute_preloaded(A_sp_addr, B_sp_addr);
          } else { // All other iterations
            matmul_compute_accumulated(A_sp_addr, B_sp_addr);
          }
        }
      }
    }
  }

  // TODO this should be overlapped with the next "Move-in D"
  // Move-out C
  if (C != NULL) {
    matmul_config_st(C_row_len * sizeof(elem_t), 0, 0, 0, 1);

    for (size_t i = 0; i < I; i++) {
      for (size_t j = 0; j < J; j++) {
        elem_t * const C_dram_addr = C + (i*C_row_len + j)*DIM;
        const uint32_t C_sp_addr = C_sp_addr_start + (i*J + j)*DIM;

        if (last_mvout) {
          matmul_mvout(C_dram_addr, C_sp_addr, 0, 0, 0, 0);
        } else if (no_bias) {
          matmul_mvout(C_dram_addr, C_sp_addr, 0, 0, 1, 0);
        } else {
          matmul_mvout(C_dram_addr, C_sp_addr, 1, 0, 0, 0);
        }
      }
    }
  }
}


static void sp_tiled_matmul_ws(elem_t * A, elem_t * B, acc_t * D, elem_t * C,
        size_t I, size_t J, size_t K, size_t A_row_len,
        size_t B_row_len, size_t D_row_len, size_t C_row_len,
        int first_mvin, int last_mvout, int no_bias) {

  const uint32_t A_sp_addr_start = 0;
  const uint32_t B_sp_addr_start = (BANK_NUM/2)*BANK_ROWS;
  const uint32_t D_sp_addr_start = 1 << (ADDR_LEN-1);
  const uint32_t C_sp_addr_start = 3 << (ADDR_LEN-2);

  const int A_blocks = K <= MAX_BLOCK_LEN ? K : MAX_BLOCK_LEN;
  const int B_blocks = J <= MAX_BLOCK_LEN ? J : MAX_BLOCK_LEN;
  const int D_blocks = J <= MAX_BLOCK_LEN_ACC ? J : MAX_BLOCK_LEN_ACC;


  const int I_iterations = I;
  const int J_iterations = (J/B_blocks + (J % B_blocks != 0));
  const int K_iterations = (K/A_blocks + (K % A_blocks != 0));
  const int total_iterations = I_iterations * J_iterations * K_iterations;

  int old_iterations = total_iterations;

  // Move-in D
  if (D != NULL && !no_bias) {
    matmul_config_ld(D_row_len * sizeof(acc_t), 0, 0, 0, 0);

    for (size_t i = 0; i < I; i++) {
      for (size_t j = 0; j < J; j++) {
        acc_t * D_dram_addr = D + (i*D_row_len + j)*DIM;
        uint32_t D_sp_addr = D_sp_addr_start + (i*J + j)*DIM;

        int already_moved_in = j % D_blocks != 0;

        if (!already_moved_in) {
          int blocks = j + D_blocks <= J ? D_blocks : J-j;

          if (first_mvin) {
            matmul_block_mvin(D_dram_addr, D_sp_addr, blocks, 0, 0, 0, 0);
          } else {
            matmul_block_mvin(D_dram_addr, D_sp_addr, blocks, 0, 1, 0, 0);
          }
        } else {
          matmul_config_ld(D_row_len * sizeof(acc_t), 0, 1, 0, 0);
        }
      }
    }
  }

  if (first_mvin) {
    matmul_config_ld(A_row_len * sizeof(elem_t), 0, 0, 0, 0);
  }

  for (size_t j = 0; j < J; j++) {
    for (size_t k = 0; k < K; k++) {
      const uint32_t B_sp_addr = B_sp_addr_start + (k*J + j)*DIM;

      for (size_t i = 0; i < I; i++) {
        elem_t * const A_dram_addr = A + (i*A_row_len + k)*DIM;
        elem_t * const B_dram_addr = B + (k*B_row_len + j)*DIM;

        const uint32_t A_sp_addr = A_sp_addr_start + (i*K + k)*DIM;
        const uint32_t C_sp_addr = C_sp_addr_start + (i*J + j)*DIM;

        // Move-in A and B
        {
          int A_already_moved_in = j != 0 || k % A_blocks != 0;
          int B_already_moved_in = i != 0 || j % B_blocks != 0;

          // Make sure the address we are moving into is not still being used by old compute instructions
          // TODO better names
          int iterations = (j/B_blocks)*K_iterations*I_iterations + (k/A_blocks)*I_iterations + i;
          iterations = total_iterations - 1 - iterations;

          while (!first_mvin && old_iterations > iterations) {
            matmul_config_ld(A_row_len * sizeof(elem_t), 0, 0, 0, 1);
            old_iterations--;
          }

          if (!A_already_moved_in) {
            int blocks = k + A_blocks <= K ? A_blocks : K-k;
            // printf("Moving in %d blocks of A: %u\n", blocks, A_sp_addr);
            matmul_block_mvin(A_dram_addr, A_sp_addr, blocks, 0, 0, 0, 0);
          }

          if (!B_already_moved_in) {
            int blocks = j + B_blocks <= J ? B_blocks : J-j;
            // printf("Moving in %d blocks of B: %u\n", blocks, B_sp_addr);
            matmul_config_ld(B_row_len * sizeof(elem_t), 0, 0, 0, 0);
            matmul_block_mvin(B_dram_addr, B_sp_addr, blocks, 0, 0, 0, 0);
          }

          matmul_config_ld(A_row_len * sizeof(elem_t), 0, 0, 1, 0);
        }

        // Compute
        {
          uint32_t pre_sp_addr = i == 0 ? B_sp_addr : GARBAGE_ADDR;
          uint32_t out_sp_addr = C_sp_addr;

          // If we're not using a bias, then we want to overwrite what's in the
          // accumulator, rather than writing over it
          int no_bias_new_matrix = no_bias && D != NULL && k == 0;
          if (no_bias_new_matrix) {
            out_sp_addr &= ~(1 << (ADDR_LEN-2));
          }

          int final_submatrix = i == I-1 && j == J-1 && k == K-1 && C != NULL;

          int last_A = (k % A_blocks == (A_blocks - 1) || k == K-1);
          int last_B = (j % B_blocks == (B_blocks - 1) || j == J-1);
          int push_to_load = !last_mvout && (last_A && last_B);

          if (!first_mvin && no_bias_new_matrix && final_submatrix) {
            // Both first and last iteration, but only relevant when bias isn't
            // used
            if (push_to_load) {
              matmul_preload(pre_sp_addr, out_sp_addr, 1, 1, 1, 1);
            } else {
              matmul_preload(pre_sp_addr, out_sp_addr, 0, 1, 1, 1);
            }
          } else if (!first_mvin && no_bias_new_matrix) {
            // First iteration, but only relevant when bias isn't used
            if (push_to_load) {
              matmul_preload(pre_sp_addr, out_sp_addr, 1, 1, 0, 1);
            } else {
              matmul_preload(pre_sp_addr, out_sp_addr, 0, 1, 0, 1);
            }
          } else if (final_submatrix) { 
            // Last iteration, when we calculate final sub-matrix
            if (push_to_load) {
              matmul_preload(pre_sp_addr, out_sp_addr, 1, 1, 1, 0);
            } else {
              matmul_preload(pre_sp_addr, out_sp_addr, 0, 1, 1, 0);
            }
          } else {
            // All other iterations
            if (push_to_load) {
              matmul_preload(pre_sp_addr, out_sp_addr, 1, 1, 0, 0);
            } else {
              matmul_preload(pre_sp_addr, out_sp_addr, 0, 1, 0, 0);
            }
          }

          if (i == 0) { // First iteration
            matmul_compute_preloaded(A_sp_addr, GARBAGE_ADDR);
          } else { // All other iterations
            matmul_compute_accumulated(A_sp_addr, GARBAGE_ADDR);
          }
        }
      }
    }
  }

  // TODO this should be overlapped with the next "Move-in D"
  // Move-out C
  if (C != NULL) {
    matmul_config_st(C_row_len * sizeof(elem_t), 0, 0, 0, 1);

    for (size_t i = 0; i < I; i++) {
      for (size_t j = 0; j < J; j++) {
        elem_t * const C_dram_addr = C + (i*C_row_len + j)*DIM;
        const uint32_t C_sp_addr = C_sp_addr_start + (i*J + j)*DIM;

        if (last_mvout) {
          matmul_mvout(C_dram_addr, C_sp_addr, 0, 0, 0, 0);
        } else if (no_bias) {
          matmul_mvout(C_dram_addr, C_sp_addr, 0, 0, 1, 0);
        } else {
          matmul_mvout(C_dram_addr, C_sp_addr, 1, 0, 0, 0);
        }
      }
    }
  }
}

static void tiled_matmul_os(size_t DIM_I, size_t DIM_J, size_t DIM_K,
        elem_t A[DIM_I][DIM_K], elem_t B[DIM_K][DIM_J], acc_t D[DIM_I][DIM_J],
        elem_t C[DIM_I][DIM_J], size_t TILE_I, size_t TILE_J, size_t TILE_K,
        int no_bias, int act, int shift, int relu6_shift) {

    const int I0 = DIM_I / (TILE_I*DIM);
    const int J0 = DIM_J / (TILE_J*DIM);
    const int K0 = DIM_K / (TILE_K*DIM);

    if (no_bias) {
      D = (int (*)[DIM_J]) 1; // Dummy address which isn't NULL
    }

    matmul_config_ex(OUTPUT_STATIONARY, act, shift, relu6_shift, 0, 0, 0, 0);

    for (size_t i0 = 0; i0 < I0; i0++)
      for (size_t j0 = 0; j0 < J0; j0++)
        for (size_t k0 = 0; k0 < K0; k0++) {
          // printf("i0: %lu, j0: %lu, k0: %lu\n", i0, j0, k0);

          int first_mvin = i0 == 0 && j0 == 0 && k0 == 0;
          int last_mvout = (i0 == I0-1) && (j0 == J0-1) && (k0 == K0-1);

          acc_t * pre = k0 == 0 ? &D[i0*TILE_I*DIM][j0*TILE_J*DIM] : NULL;
          elem_t * out = k0 == K0-1 ? &C[i0*TILE_I*DIM][j0*TILE_J*DIM] : NULL;

          sp_tiled_matmul_os(&A[i0*TILE_I*DIM][k0*TILE_K*DIM],
              &B[k0*TILE_K*DIM][j0*TILE_J*DIM],
              pre, out,
              TILE_I, TILE_J, TILE_K,
              DIM_K, DIM_J, DIM_J, DIM_J,
              first_mvin, last_mvout, no_bias);
        }

    matmul_fence();

#ifdef TEST_QUEUES
	printf("ld_to_st_q: %d\n", ld_to_st_q);
	printf("ld_to_ex_q: %d\n", ld_to_ex_q);
	printf("st_to_ld_q: %d\n", st_to_ld_q);
	printf("st_to_ex_q: %d\n", st_to_ex_q);
	printf("ex_to_ld_q: %d\n", ex_to_ld_q);
	printf("ex_to_st_q: %d\n", ex_to_st_q);
#endif
}

static void tiled_matmul_ws(size_t DIM_I, size_t DIM_J, size_t DIM_K,
        elem_t A[DIM_I][DIM_K], elem_t B[DIM_K][DIM_J], acc_t D[DIM_I][DIM_J],
        elem_t C[DIM_I][DIM_J], size_t TILE_I, size_t TILE_J, size_t TILE_K,
        int no_bias, int act, int shift, int relu6_shift) {

    const int I0 = DIM_I / (TILE_I*DIM);
    const int J0 = DIM_J / (TILE_J*DIM);
    const int K0 = DIM_K / (TILE_K*DIM);

    if (no_bias) {
      D = (int (*)[DIM_J]) 1; // Dummy address which isn't NULL
    }

    matmul_config_ex(WEIGHT_STATIONARY, act, shift, relu6_shift, 0, 0, 0, 0);

    for (size_t i0 = 0; i0 < I0; i0++)
      for (size_t j0 = 0; j0 < J0; j0++)
        for (size_t k0 = 0; k0 < K0; k0++) {
          // printf("Outer: i0: %u, j0: %u, k0: %u\n", i0, j0, k0);

          int first_mvin = i0 == 0 && j0 == 0 && k0 == 0;
          int last_mvout = (i0 == I0-1) && (j0 == J0-1) && (k0 == K0-1);

          acc_t * pre = k0 == 0 ? &D[i0*TILE_I*DIM][j0*TILE_J*DIM] : NULL;
          elem_t * out = k0 == K0-1 ? &C[i0*TILE_I*DIM][j0*TILE_J*DIM] : NULL;

          sp_tiled_matmul_ws(&A[i0*TILE_I*DIM][k0*TILE_K*DIM],
              &B[k0*TILE_K*DIM][j0*TILE_J*DIM],
              pre, out,
              TILE_I, TILE_J, TILE_K,
              DIM_K, DIM_J, DIM_J, DIM_J,
              first_mvin, last_mvout, no_bias);
        }

    matmul_fence();
}

void matmul_cpu(size_t DIM_I, size_t DIM_J, size_t DIM_K,
        elem_t A[DIM_I][DIM_K], elem_t B[DIM_K][DIM_J], acc_t D[DIM_I][DIM_J],
        elem_t C[DIM_I][DIM_J], int no_bias, int shift, int act, int relu6_shift) {
  for (size_t i = 0; i < DIM_I; i++) {
    for (size_t j = 0; j < DIM_J; j++) {
      acc_t result = no_bias ? 0 : D[i][j];
      for (size_t k = 0; k < DIM_K; k++) {
        result += A[i][k] * B[k][j];
      }

      // Scale value down and round it
      const int divisor = 1 << shift;
      acc_t abs = result > 0 ? result : -result;
      acc_t shifted = (abs + (divisor/2)) / divisor;
      if (result < 0)
          result = -shifted;
      else
          result = shifted;

      // Clip result
      result = result > elem_t_max ? elem_t_max : (result < elem_t_min ? elem_t_min : result);

      // Apply activation function
      if (act == RELU) {
        result = result < 0 ? 0 : result;
      } else if (act == RELU6) {
        int max = 6 << relu6_shift;
        result = result < 0 ? 0 : (result > max ? max : result);
      }

      C[i][j] = (elem_t)result;
    }
  }
}

// General matmul which can be run with different dataflows, or on the CPU
enum tiled_matmul_type_t {OS, WS, CPU};

static size_t tiling_factor(const size_t dimension, const size_t max_tile_factor) {
    const size_t start = dimension < max_tile_factor ? dimension : max_tile_factor;

    for (size_t tile_factor = start; tile_factor >= 1; tile_factor--) {
        if (dimension % tile_factor == 0)
            return tile_factor;
    }
    return 1; // We should never reach here anyway
}

// TODO automatically calculate optimal tiling factors
static void tiled_matmul_option(size_t DIM_I, size_t DIM_J, size_t DIM_K,
        elem_t A[DIM_I][DIM_K], elem_t B[DIM_K][DIM_J], acc_t D[DIM_I][DIM_J],
        elem_t C[DIM_I][DIM_J], // size_t TILE_I, size_t TILE_J, size_t TILE_K,
        int no_bias, int act, int shift, int relu6_shift,
        enum tiled_matmul_type_t tiled_matmul_type) {
    printf("Entered function\n");

    const int partition_rows = (BANK_NUM/2)*BANK_ROWS;
    printf("partition_rows: %d\n", partition_rows);
    const int mats_in_acc = (ACC_ROWS/BANK_ROWS);
    printf("mats_in_acc: %d\n", mats_in_acc);
    const int max_tile_i_j = (int)sqrt(mats_in_acc);
    printf("max_tile_i_j: %d\n", max_tile_i_j);
    const int max_tile_k = (partition_rows/BANK_NUM) / max_tile_i_j;
    printf("max_tile_k: %d\n", max_tile_k);

    const size_t tile_i = tiling_factor(DIM_I, max_tile_i_j);
    printf("tile_i: %d\n", tile_i);
    const size_t tile_j = tiling_factor(DIM_J, max_tile_i_j);
    printf("tile_j: %d\n", tile_j);
    const size_t tile_k = tiling_factor(DIM_K, max_tile_k);
    printf("tile_k: %d\n", tile_k);

    if (tiled_matmul_type == OS) {
        printf("Entered OS\n");
        tiled_matmul_os(DIM_I, DIM_J, DIM_K,
                A, B, D, C,
                tile_i, tile_j, tile_k,
                no_bias, act, shift, relu6_shift);
    } else if (tiled_matmul_type == WS) {
        printf("Entered WS\n");
        tiled_matmul_ws(DIM_I, DIM_J, DIM_K,
                A, B, D, C,
                tile_i, tile_j, tile_k,
                no_bias, act, shift, relu6_shift);
    } else /*if (tiled_matmul_type == CPU)*/ {
        printf("Entered CPU\n");
        matmul_cpu(DIM_I, DIM_J, DIM_K,
                A, B, D, C,
                no_bias, act, shift, relu6_shift);
    }/* else {
        printf("unknown tiled matrix type");
        exit(1);
    }*/

    printf("finished function");
}

#endif  // SRC_MAIN_C_SYSTOLIC_H

