/*
 * hkuCl.h: hku OpenCl API header
 */
#include <cstdio>
#include "oclHelper.h"

// Word on AXI-M is 512-bit, below to translate bounds
#define WrdHalfSize 32
#define WrdFloatSize 16
#define RndWrdHalf(x) (((x - 1)/WrdHalfSize + 1)*WrdHalfSize)
#define RndWrdFloat(x) (((x - 1)/WrdFloatSize + 1)*WrdFloatSize)

#define OCL_CHECK(call)                                                         \
    do {                                                                        \
        cl_int err = call;                                                      \
        if (err != CL_SUCCESS) {                                                \
            printf("Error calling " #call ", error: %s\n", oclErrorCode(err));  \
            exit(EXIT_FAILURE);                                                 \
        }                                                                       \
    } while (0);

int hkuHtrans(
        size_t M, 
        size_t N,
        bool isColMajor,
        const cl_mem A,
        cl_mem AT,
        cl_command_queue command_queue,
        cl_kernel krnl,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event
        );

int hkuHmmult(
        size_t M, 
        size_t N,
        size_t K, 
        const cl_mem A,
        const cl_mem B,
        cl_mem C,
        //cl_uint numCommandQueues,
        cl_command_queue command_queue,
        cl_kernel krnl,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event
        );

void hkuGenHCntMat(size_t rows, size_t cols, cl_half* mat);
void hkuGenHIMat(size_t rows, size_t cols, cl_half* mat);

void hkuHmmultSW(
        size_t M, 
        size_t K, 
        size_t N, 
        cl_half *A, 
        cl_half *B, 
        cl_half *C);

void hkuPrintHMat(size_t rows, size_t cols, cl_half *mat);
double hkuHMatDiffRatio(size_t rows, size_t cols, cl_half *in, cl_half *ref);

