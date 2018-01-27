/*
 * Host code for testing the krnlMmult design
 */
//#define DEBUG // needs to go before hkuCommon.h

#include <iostream>
#include <cmath> // for log2
#include "cmdlineparser.h"
#include "oclHelper.h"
#include "xcl.h"
#include "clblast_half.h"
#include "hkuCommon.h"
#include "hkuCl.h"

#ifndef USE_INT
typedef cl_half DType;
typedef float HType;
#else
typedef short DType;
typedef short HType;
#endif

#define WrdSize WrdHalfSize 

int mmultTest(sda::utils::CmdLineParser &parser)
{
    int ret = 0;
    size_t M = parser.value_to_int("rowsA");
    size_t K = parser.value_to_int("colsA");
    size_t N = parser.value_to_int("colsB");
    bool skipTest = (parser.value("skipTest").compare(string("true")) == 0);

    cl_int err;

    std::cout << "Try to xcl_world_single" << std::endl;
    xcl_world world = xcl_world_single();
    //clReleaseCommandQueue(world.command_queue);
    //world.command_queue = clCreateCommandQueue(world.context, world.device_id, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);

    std::cout << "Try to xcl_import_binary" << std::endl;
    cl_program program = xcl_import_binary(world, "hku");
    
    std::cout << "Try to xcl_get_kernel" << std::endl;
    cl_kernel krnlMmult = xcl_get_kernel(program, "mmult");

    std::cout << "Finished xcl_get_kernel" << std::endl;

    size_t upM = RndWrdHalf(M);
    size_t upN = RndWrdHalf(N);
    size_t bufA_size = upM * K * sizeof(DType);
    size_t bufB_size = K * upN * sizeof(DType);
    size_t bufC_size = upM * N * sizeof(DType);
    std::cout << "Host bufA size is " << bufA_size << std::endl;
    std::cout << "Host bufB size is " << bufB_size << std::endl;
    std::cout << "Host bufC size is " << bufC_size << std::endl;

    DType* A = (DType*)malloc(bufA_size);
    DType* B = (DType*)malloc(bufB_size);
    DType* C = (DType*)malloc(bufC_size);
    HType* tbA = (HType*)malloc(M * K * sizeof(HType));
    HType* tbB = (HType*)malloc(K * N * sizeof(HType));
    HType* tbC = (HType*)malloc(M * N * sizeof(HType));

    std::cout << "Filling host matrix A and B" << std::endl;
    //hkuGenHCntMat(M, upK, A);
    size_t tileRowsA = (M-1)/WrdSize + 1;
    for (size_t tr = 0; tr < tileRowsA; tr++) {
        for (size_t c = 0; c < K; c++) {
            for (size_t r = 0; r < WrdSize; r++) {
                size_t idx = K * WrdSize * tr + WrdSize * c + r;
                if (WrdSize * tr + r < M) {
                    size_t val = ((WrdSize * tr + r) * K + c + 1) % 2048;
                    #ifndef USE_INT
                    A[idx] = FloatToHalf((HType)val);
                    DEBUG_PRINT("A[%d] %f\n", idx, HalfToFloat(A[idx]));
                    #else
                    A[idx] = (HType)val;
                    DEBUG_PRINT("A[%d] %d\n", idx, A[idx]);
                    #endif
                    DEBUG_PRINT("WrdSize %d tr %d r %d K %d c %d\n", WrdSize, tr, r, K, c);
                    DEBUG_PRINT("val %d\n", val);
                } else {
                    #ifndef USE_INT
                    A[idx] = FloatToHalf(0.);
                    #else
                    A[idx] = 0;
                    #endif
                }
            }
        }
    }
    //hkuGenHIMat(K, upN, B);
    size_t tileColsB = (N-1)/WrdSize + 1;
    for (size_t tc = 0; tc < tileColsB; tc++) {
        for (size_t r = 0; r < K; r++) {
            for (size_t c = 0; c < WrdSize; c++) {
                size_t idx = K * WrdSize * tc + WrdSize * r + c;
                if (r == WrdSize * tc + c) {
                    #ifndef USE_INT
                    B[idx] = FloatToHalf(1.);
                    #else
                    B[idx] = 1;
                    #endif
                    DEBUG_PRINT("B[%d] 1.\n", idx);
                } else {
                    #ifndef USE_INT
                    B[idx] = FloatToHalf(0.);
                    #else
                    B[idx] = 0;
                    #endif
                }
            }
        }
    }
    int cnt = 1;
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < K; j++) {
            tbA[K * i + j] = (HType)(cnt++ % 2048);
        }
    }
    for (size_t i = 0; i < K; i++) {
        for (size_t j = 0; j < N; j++) {
            if (i == j) {
                #ifndef USE_INT
                tbB[N * i + j] = 1.;
                #else
                tbB[N * i + j] = 1;
                #endif
            } else {
                #ifndef USE_INT
                tbB[N * i + j] = 0.;
                #else
                tbB[N * i + j] = 0;
                #endif
            }
        }
    }

    /* ---- begin test 1, A x I == A ---- */
    #define WRITE_TO_DEVICE 1
    #define READ_FROM_DEVICE 1
    cl_mem bufA, bufB, bufC;
    cl_event write_events[2], kernel_events[1], read_events[1];

    #if WRITE_TO_DEVICE == 2
    bufA = clCreateBuffer(world.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, bufA_size, A, &err);
    bufB = clCreateBuffer(world.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, bufB_size, B, &err);
    bufC = clCreateBuffer(world.context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, bufC_size, C, &err);
    #else
    bufA = xcl_malloc(world, CL_MEM_READ_ONLY, bufA_size);
    bufB = xcl_malloc(world, CL_MEM_READ_ONLY, bufB_size);
    bufC = xcl_malloc(world, CL_MEM_WRITE_ONLY, bufC_size);
    #endif

    std::cout << "Migrating bufA and bufB" << std::endl;
    //#if WRITE_TO_DEVICE == 0
    //xcl_memcpy_to_device(world, bufA, A, bufA_size);
    //xcl_memcpy_to_device(world, bufB, B, bufB_size);
    #if WRITE_TO_DEVICE == 1
    OCL_CHECK( clEnqueueWriteBuffer(world.command_queue, bufA, CL_TRUE, 0, bufA_size, A, 0, NULL, &write_events[0]));
    OCL_CHECK( clEnqueueWriteBuffer(world.command_queue, bufB, CL_TRUE, 0, bufB_size, B, 0, NULL, &write_events[1]));
    #elif WRITE_TO_DEVICE == 2
    OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufA, 0 /* flags, 0 means from host*/, 0, NULL, &write_events[0]));
    OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufB, 0 /* flags, 0 means from host*/, 0, NULL, &write_events[1]));
    #endif
    // if host->global write and kernel->global write deadlocks, need wait
    //clFinish(world.command_queue); 

    std::cout << "Launching kernel 1" << std::endl;
    err = hkuHmmult(M, N, K, bufA, bufB, bufC, world.command_queue, krnlMmult, 2, write_events, &kernel_events[0]);

    //#if READ_FROM_DEVICE == 0
    //xcl_memcpy_from_device(world, C, bufC, bufC_size);
    #if READ_FROM_DEVICE == 1
    OCL_CHECK( clEnqueueReadBuffer(world.command_queue, bufC, CL_TRUE, 0, bufC_size, C, 1, &kernel_events[0], &read_events[0]));
    #elif READ_FROM_DEVICE == 2
    cl_event map_events[1];
    OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufC, CL_MIGRATE_MEM_OBJECT_HOST, 1, &kernel_events[0], &read_events[0]));
    clEnqueueMapBuffer(world.command_queue, bufC, CL_FALSE, CL_MAP_READ, 0, bufC_size, 1, &read_events[0], &map_events[0], &err);
    #endif

    clFlush(world.command_queue);
    clFinish(world.command_queue);
    /* ---- end test 1, A x I == A ---- */
    #if READ_FROM_DEVICE == 2
    OCL_CHECK( clWaitForEvents(1, &map_events[0]));
    #endif

    double sizeA_MB         = (double)bufA_size / (1024*1024);
    unsigned long timeA_ns  = xcl_get_event_duration(write_events[0]);
    double timeA_s          = timeA_ns / 1000000000.0;
    printf("INFO: Write matrix A: time %f secs, size %f MB, throughput %f MB/s\n", timeA_s, sizeA_MB, sizeA_MB / timeA_s);

    double sizeB_MB         = (double)bufB_size / (1024*1024);
    unsigned long timeB_ns  = xcl_get_event_duration(write_events[1]);
    double timeB_s          = timeB_ns / 1000000000.0;
    printf("INFO: Write matrix B: time %f secs, size %f MB, throughput %f MB/s\n", timeB_s, sizeB_MB, sizeB_MB / timeB_s);

    double numOps               = M / 1000.0 * N / 1000.0 * K * 2.0;
    unsigned long timeKrnl_ns   = xcl_get_event_duration(kernel_events[0]);
    double timeKrnl_s           = timeKrnl_ns / 1000000000.0;
    double efficiency           = (numOps / timeKrnl_s) / 1000.0;
    printf("INFO: Exe MMult Krnl: time %f secs, numOps %f M, Efficiency %f GOPs\n", timeKrnl_s, numOps, efficiency);

    double sizeC_MB         = (double)bufC_size / (1024*1024);
    unsigned long timeC_ns  = xcl_get_event_duration(read_events[0]);
    double timeC_s          = timeC_ns / 1000000000.0;
    printf("INFO: Read matrix C: time %f secs, size %f MB, throughput %f MB/s\n", timeC_s, sizeC_MB, sizeC_MB / timeC_s);

    #if READ_FROM_DEVICE == 2
    unsigned long timeMap_ns    = xcl_get_event_duration(map_events[0]);
    double timeMap_s            = timeMap_ns / 1000000000.0;
    printf("INFO: Read matrix C: map time %f secs\n", timeMap_s);
    #endif

    OCL_CHECK( clReleaseMemObject(bufA));
    OCL_CHECK( clReleaseMemObject(bufB));
    OCL_CHECK( clReleaseMemObject(bufC));

    OCL_CHECK( clReleaseEvent(write_events[0]));
    OCL_CHECK( clReleaseEvent(write_events[1]));
    OCL_CHECK( clReleaseEvent(kernel_events[0]));
    OCL_CHECK( clReleaseEvent(read_events[0]));
    #if READ_FROM_DEVICE == 2
    OCL_CHECK( clReleaseEvent(map_events[0]));
    #endif

    OCL_CHECK( clReleaseKernel(krnlMmult));
    OCL_CHECK( clReleaseProgram(program));
    xcl_release_world(world);
//OPENCL HOST CODE AREA END

    if (!skipTest) {
        std::cout << "Calculating golden output on host" << std::endl;
        for (size_t i = 0; i < M; i++) {
            for (size_t j = 0; j < N; j++) {
                #ifndef USE_INT
                HType tmp = 0;
                #else
                int tmp = 0;
                #endif
                for (size_t kk = 0; kk < K; kk++) {
                    tmp += tbA[K*i + kk] * tbB[N*kk + j];
                }
                #ifndef USE_INT
                tbC[N*i + j] = tmp;
                #else
                tbC[N*i + j] = (tmp>>16);
                #endif
            }
        }
        std::cout << "Start comparing C against golden output on host" << std::endl;
        size_t count = 0;
        #ifndef USE_INT
        HType epsilon = powf(2.0, -24); // 2^-24 is smallest 1-ULP in half, excluding denormals
        #endif
        //for (size_t tr = 0; tr < tileRowsA; tr++) 
        //    for (size_t r = 0; r < WrdSize; r++) 
        //        for (size_t c = 0; c < N; c++) 
        for (size_t tc = 0; tc < tileColsB; tc++) {
            for (size_t r = 0; r < M; r++) {
                for (size_t c = 0; c < WrdSize; c++) {
                    //size_t idx = N * WrdSize * tr + WrdSize * c + r;
                    //size_t idx = M * WrdSize * tc + WrdSize * M + c;
                    size_t idx = r * N + WrdSize * tc + c;
#ifndef USE_INT
                    HType hwC = HalfToFloat(C[count]);
                    if (tc + c < N && fabs(hwC - tbC[idx]) > epsilon) {
                        printf("ERROR in - %ld - actual=%f, expected=%f\n", count, hwC, tbC[idx]);
                        ret = 1;
                        goto DONE;
                    }
#else
                    HType hwC = C[count];
                    if (tc + c < N && hwC != tbC[idx]) {
                        printf("ERROR in - %ld - actual=%d, expected=%d\n", count, hwC, tbC[idx]);
                        ret = 1;
                        goto DONE;
                    }
#endif
                    count++;
                }
            }
        }
    } else {
        std::cout << "SW Test skipped!" << std::endl;
    }
DONE: 
    free(A);
    free(B);
    free(C);
    free(tbA);
    free(tbB);
    free(tbC);
    if (ret) {
        printf("INFO: Test Failed\n");
    } else {
        printf("INFO: Test Passed\n");
    }

    return ret;
}


template <typename DType, typename HType, int DMax>
int transTest(sda::utils::CmdLineParser &parser)
{
    int ret = 0;
    size_t M = parser.value_to_int("rowsA");
    size_t K = parser.value_to_int("colsA");

    cl_int err;

    std::cout << "Try to xcl_world_single" << std::endl;
    xcl_world world = xcl_world_single();
    //clReleaseCommandQueue(world.command_queue);
    //world.command_queue = clCreateCommandQueue(world.context, world.device_id, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);

    std::cout << "Try to xcl_import_binary" << std::endl;
    cl_program program = xcl_import_binary(world, "hku");
    
    std::cout << "Try to xcl_get_kernel" << std::endl;
    cl_kernel krnlTrans = xcl_get_kernel(program, "trans");

    std::cout << "Finished xcl_get_kernel" << std::endl;

    size_t upM = RndWrdHalf(M);
    size_t upK = RndWrdHalf(K);
    size_t bufA_size = upM * K * sizeof(DType);
    size_t bufAT_size = upK * M * sizeof(DType);
    std::cout << "Host bufA size is " << bufA_size << std::endl;
    std::cout << "Host bufAT size is " << bufAT_size << std::endl;

    DType* A = (DType*)malloc(bufA_size);
    DType* AT = (DType*)malloc(bufAT_size);
    HType* tbAT = (HType*)malloc(K * M * sizeof(HType));

    std::cout << "Filling host matrix A" << std::endl;
    size_t tileRowsA = (M-1)/WrdSize + 1;
    for (size_t tr = 0; tr < tileRowsA; tr++) {
        for (size_t c = 0; c < K; c++) {
            for (size_t r = 0; r < WrdSize; r++) {
                size_t idx = K * WrdSize * tr + WrdSize * c + r;
                if (WrdSize * tr + r < M) {
                    size_t val = ((WrdSize * tr + r) * K + c + 1) % DMax;
                    //A[idx] = FloatToHalf((HType)val);
                    //DEBUG_PRINT("A[%d] %f\n", idx, HalfToFloat(A[idx]));
                    A[idx] = (HType)val;
                    DEBUG_PRINT("A[%d] %f\n", idx, A[idx]);
                    DEBUG_PRINT("WrdSize %d tr %d r %d K %d c %d\n", WrdSize, tr, r, K, c);
                    DEBUG_PRINT("val %d\n", val);
                } else {
                    //A[idx] = FloatToHalf(0.0);
                    A[idx] = 0;
                }
            }
        }
    }
    int cnt = 1;
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < K; j++) {
            //tbAT[M * j + i] = (float)(cnt++ % 2048);
            tbAT[M * j + i] = (HType)(cnt++ % DMax);
        }
    }

    #define WRITE_TO_DEVICE 1
    #define READ_FROM_DEVICE 1
    cl_mem bufA, bufAT;
    cl_event write_events[1], kernel_events[1], read_events[1];

    #if WRITE_TO_DEVICE == 2
    bufA = clCreateBuffer(world.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, bufA_size, A, &err);
    bufAT = clCreateBuffer(world.context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, bufAT_size, AT, &err);
    #else
    bufA = xcl_malloc(world, CL_MEM_READ_ONLY, bufA_size);
    bufAT = xcl_malloc(world, CL_MEM_WRITE_ONLY, bufAT_size);
    #endif

    std::cout << "Migrating bufA" << std::endl;
    #if WRITE_TO_DEVICE == 1
    OCL_CHECK( clEnqueueWriteBuffer(world.command_queue, bufA, CL_TRUE, 0, bufA_size, A, 0, NULL, &write_events[0]));
    #elif WRITE_TO_DEVICE == 2
    OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufA, 0 /* flags, 0 means from host*/, 0, NULL, &write_events[0]));
    #endif
    // if host->global write and kernel->global write deadlocks, need wait
    //clFinish(world.command_queue); 

    std::cout << "Launching kernel 1" << std::endl;
    //TODO!!!
    //err = hkuHmmult(M, N, K, bufA, bufB, bufC, world.command_queue, krnlMmult, 2, write_events, &kernel_events[0]);
    err = hkuHtrans(M, K, true, bufA, bufAT, world.command_queue, krnlTrans, 1, write_events, &kernel_events[0]);

    #if READ_FROM_DEVICE == 1
    OCL_CHECK( clEnqueueReadBuffer(world.command_queue, bufAT, CL_TRUE, 0, bufAT_size, AT, 1, &kernel_events[0], &read_events[0]));
    #elif READ_FROM_DEVICE == 2
    cl_event map_events[1];
    OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufAT, CL_MIGRATE_MEM_OBJECT_HOST, 1, &kernel_events[0], &read_events[0]));
    clEnqueueMapBuffer(world.command_queue, bufAT, CL_FALSE, CL_MAP_READ, 0, bufAT_size, 1, &read_events[0], &map_events[0], &err);
    #endif

    clFlush(world.command_queue);
    clFinish(world.command_queue);
    /* ---- end test 1, A x I == A ---- */
    #if READ_FROM_DEVICE == 2
    OCL_CHECK( clWaitForEvents(1, &map_events[0]));
    #endif

    double sizeA_MB         = (double)bufA_size / (1024*1024);
    unsigned long timeA_ns  = xcl_get_event_duration(write_events[0]);
    double timeA_s          = timeA_ns / 1000000000.0;
    printf("INFO: Write matrix A: time %f secs, size %f MB, throughput %f MB/s\n", timeA_s, sizeA_MB, sizeA_MB / timeA_s);

    double sizeAT_MB         = (double)bufAT_size / (1024*1024);
    unsigned long timeAT_ns  = xcl_get_event_duration(read_events[0]);
    double timeAT_s          = timeAT_ns / 1000000000.0;
    printf("INFO: Read matrix AT: time %f secs, size %f MB, throughput %f MB/s\n", timeAT_s, sizeAT_MB, sizeAT_MB / timeAT_s);

    unsigned long timeKrnl_ns   = xcl_get_event_duration(kernel_events[0]);
    double timeKrnl_s           = timeKrnl_ns / 1000000000.0;
    printf("INFO: Exe Trans Krnl: time %f secs\n", timeKrnl_s);

    #if READ_FROM_DEVICE == 2
    unsigned long timeMap_ns    = xcl_get_event_duration(map_events[0]);
    double timeMap_s            = timeMap_ns / 1000000000.0;
    printf("INFO: Read matrix AT: map time %f secs\n", timeMap_s);
    #endif

    OCL_CHECK( clReleaseMemObject(bufA));
    OCL_CHECK( clReleaseMemObject(bufAT));

    OCL_CHECK( clReleaseEvent(write_events[0]));
    OCL_CHECK( clReleaseEvent(kernel_events[0]));
    OCL_CHECK( clReleaseEvent(read_events[0]));
    #if READ_FROM_DEVICE == 2
    OCL_CHECK( clReleaseEvent(map_events[0]));
    #endif

    OCL_CHECK( clReleaseKernel(krnlTrans));
    OCL_CHECK( clReleaseProgram(program));
    xcl_release_world(world);
//OPENCL HOST CODE AREA END

    std::cout << "Start comparing AT against tbAT on host" << std::endl;
    size_t count = 0;
    size_t tileRowsAT = (K-1)/WrdSize + 1;
    //float epsilon = powf(2.0, -24); // 2^-24 is smallest 1-ULP in half, excluding denormals
    for (size_t tr = 0; tr < tileRowsAT; tr++) {
        for (size_t r = 0; r < WrdSize; r++) {
            for (size_t c = 0; c < M; c++) {
                size_t idx = M * WrdSize * tr + WrdSize * c + r;
                //float hwAT = HalfToFloat(AT[idx]);
                //if (count < M*K && fabs(hwAT -  tbAT[count]) > epsilon) 
                HType hwAT = AT[idx];
                if (count < M*K && hwAT != tbAT[count]) {
                    printf("ERROR in - %ld - actual=%d, expected=%d\n", count, hwAT, tbAT[count]);
                    ret = 1;
                    goto DONE;
                }
                count++;
            }
        }
    }
DONE: 
    free(A);
    free(AT);
    free(tbAT);
    if (ret) {
        printf("INFO: Test Failed\n");
    } else {
        printf("INFO: Test Passed\n");
    }

    return ret;
}


inline bool is_a_ge_zero_and_a_lt_b(int a, int b) {
  return static_cast<unsigned>(a) < static_cast<unsigned>(b);
}

template<typename Dtype>
void im2col_cpu(const Dtype* data_im, const int channels,
                const int height, const int width, const int kernel_h,
                const int kernel_w, const int pad_h, const int pad_w,
                const int stride_h, const int stride_w,
                const int dilation_h, const int dilation_w,
                Dtype* data_col) {
  const int output_h = (height + 2 * pad_h
      - (dilation_h * (kernel_h - 1) + 1)) / stride_h + 1;
  const int output_w =
      (width + 2 * pad_w - (dilation_w * (kernel_w - 1) + 1)) / stride_w + 1;
  const int channel_size = height * width;
  for (int channel = channels; channel--; data_im += channel_size) {
    for (int kernel_row = 0; kernel_row < kernel_h; kernel_row++) {
      for (int kernel_col = 0; kernel_col < kernel_w; kernel_col++) {
        int input_row = -pad_h + kernel_row * dilation_h;
        for (int output_rows = output_h; output_rows; output_rows--) {
          if (!is_a_ge_zero_and_a_lt_b(input_row, height)) {
            for (int output_cols = output_w; output_cols; output_cols--) {
              *(data_col++) = 0;
            }
          } else {
            int input_col = -pad_w + kernel_col * dilation_w;
            for (int output_col = output_w; output_col; output_col--) {
              if (is_a_ge_zero_and_a_lt_b(input_col, width)) {
                *(data_col++) = data_im[input_row * width + input_col];
              } else {
                *(data_col++) = 0;
              }
              input_col += stride_w;
            }
          }
          input_row += stride_h;
        }
      }
    }
  }
}


template <typename DType>
int im2colTest(sda::utils::CmdLineParser &parser) 
{
    int ret = 0;
    cl_int err;

    std::cout << "Try to xcl_world_single" << std::endl;
    xcl_world world = xcl_world_single();

    std::cout << "Try to xcl_import_binary" << std::endl;
    cl_program program = xcl_import_binary(world, "hku");
    
    std::cout << "Try to xcl_get_kernel" << std::endl;
    cl_kernel krnl = xcl_get_kernel(program, "im2col");

    std::cout << "Finished xcl_get_kernel" << std::endl;

    int number = parser.value_to_int("imN");
    int channels = parser.value_to_int("imC");
    int height = parser.value_to_int("imH");
    int width = parser.value_to_int("imW");
    int kernel_h = parser.value_to_int("imKh");
    int kernel_w = parser.value_to_int("imKw");
    int pad_h = parser.value_to_int("imPh");
    int pad_w = parser.value_to_int("imPw");
    int stride_h = parser.value_to_int("imSh");
    int stride_w = parser.value_to_int("imSw");
    int dilation_h = parser.value_to_int("imDh");
    int dilation_w = parser.value_to_int("imDw");

    int w_rnd = ((width - 1) / WrdSize + 1) * WrdSize;

    const int output_h = (height + 2 * pad_h
            - (dilation_h * (kernel_h - 1) + 1)) / stride_h + 1;
    const int output_w = (width + 2 * pad_w 
            - (dilation_w * (kernel_w - 1) + 1)) / stride_w + 1;

    //int output_w_rnd = ((output_w - 1) / WrdSize + 1) * WrdSize;

    int col_h = channels * kernel_h * kernel_w;
    int col_w = output_h * output_w;
    //int col_w_rnd = ((col_w - 1) / WrdSize + 1) * WrdSize;
    int output_w_rnd = ((output_w - 1) / WrdSize + 1) * WrdSize;

    size_t host_im_size = number * channels * height * width * sizeof(DType);
    size_t dev_im_size = number * channels * height * w_rnd * sizeof(DType);
    size_t host_col_size = number * col_h * col_w * sizeof(DType);
    //size_t dev_col_size = number * col_h * col_w_rnd * sizeof(DType);
    size_t dev_col_size = number * col_h * output_h * output_w_rnd * sizeof(DType);
    printf("host_im_size %zu, dev_im_size %zu\n", host_im_size, dev_im_size);
    printf("host_col_size %zu, dev_col_size %zu\n", host_col_size, dev_col_size);

    DType* host_im_ptr = (DType*)malloc(host_im_size);
    DType* host_col_ptr = (DType*)malloc(host_col_size);
    DType* dev_im_ptr = (DType*)malloc(dev_im_size);
    DType* dev_col_ptr = (DType*)malloc(dev_col_size);

    int width_tiles = ((width - 1) / WrdSize + 1);
    int dev_im_addr = 0;

    for (int i = 0; i < number * channels; i++) {
        for (int tc = 0; tc < width_tiles; tc++) {
            for (int r = 0; r < height; r++) {
                for (int j = 0; j < WrdSize; j++) {
                    int col = WrdSize * tc + j;
                    if (col < width) {
                        dev_im_addr = ((width_tiles * i + tc) * height + r) * WrdSize + j;
                        dev_im_ptr[dev_im_addr] = width * r + col;
                        host_im_ptr[width * height * i + width * r + col] = width * r + col;
                    } else {
                        dev_im_addr = ((width_tiles * i + tc) * height + r) * WrdSize + j;
                        dev_im_ptr[dev_im_addr] = -1;
                    }
                }
            }
        }
    }

    //cl_event write_events[1], kernel_events[1], read_events[1];
    cl_mem dev_im_buf, dev_col_buf;

    dev_im_buf = xcl_malloc(world, CL_MEM_READ_ONLY, dev_im_size);
    dev_col_buf = xcl_malloc(world, CL_MEM_WRITE_ONLY, dev_col_size);

    xcl_memcpy_to_device(world, dev_im_buf, dev_im_ptr, dev_im_size);

    int narg = 0;
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &dev_im_buf);
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &dev_col_buf);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &number);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &channels);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &height);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &width);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &kernel_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &kernel_w);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &pad_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &pad_w);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &stride_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &stride_w);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &dilation_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &dilation_w);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &output_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &output_w);

    unsigned long duration = xcl_run_kernel3d(world, krnl, 1, 1, 1);

    xcl_memcpy_from_device(world, dev_col_ptr, dev_col_buf, dev_col_size);

    clFlush(world.command_queue);
    clFinish(world.command_queue);
    
    OCL_CHECK( clReleaseMemObject(dev_im_buf));
    OCL_CHECK( clReleaseMemObject(dev_col_buf));
    OCL_CHECK( clReleaseKernel(krnl));
    OCL_CHECK( clReleaseProgram(program));
    xcl_release_world(world);

    std::cout << "im2col duration = " << duration/1000000.0 << " ms" << std::endl;

    int im_off = 0, col_off = 0;

    for (int n = 0; n < number; n++) {
        im2col_cpu<DType>(
                host_im_ptr + im_off, 
                channels, height, width,
                kernel_h, kernel_w,
                pad_h, pad_w,
                stride_h, stride_w,
                dilation_h, dilation_w,
                host_col_ptr + col_off);
        im_off += channels * height * width;
        col_off += col_h * col_w;
    }

    int output_w_tiles = (output_w - 1) / WrdSize + 1;
    //int output_tiles = (col_w - 1) / WrdSize + 1;
    int output_tiles = output_h * output_w_tiles;

    int dev_col_addr = 0;
    int count = 0;

    // Compare dev_col_ptr and host_col_ptr
    for (int n = 0; n < number; n++) {
        for (int tc = 0, out_r = 0, out_tc = 0; tc < output_h * output_w_tiles; tc++) {

            for (int r = 0; r < col_h; r++) {
                for (int j = 0; j < WrdSize; j++) {
                    //int col = WrdSize * tc + j;
                    //if (col < col_w) 
                    int col = out_r * output_w + out_tc * WrdSize + j;
                    if (out_tc * WrdSize + j < output_w) {
                        //dev_col_addr = ((n * output_h * output_w_tiles + tc) * col_h + r) * WrdSize + j;
                        int index = col_h * col_w * n + col_w * r + col;
                        if (dev_col_ptr[count/*dev_col_addr*/] != host_col_ptr[index]) {
                            printf("ERROR in - %d - actual=%d, expected=%d\n", count, dev_col_ptr[count], host_col_ptr[index]);
                            printf("number=%d, output_tiles=%d, col_h=%d\n", number, output_tiles, col_h);
                            printf("n=%d, tc=%d, r=%d, j=%d\n", n, tc, r, j);
                            //printf("col_w=%d, col_w_rnd=%d\n", col_w, col_w_rnd);
                            printf("col_w=%d\n", col_w);
                            printf("col=%d, index=%d, output_h=%d\n", col, index, output_h);

                            ret = 1;
                            goto DONE;
                        }
                    }
                    count++;
                }
            }
            if (out_tc == output_w_tiles - 1) {
                out_tc = 0; out_r++;
            } else {
                out_tc++;
            }
        }
    }

DONE:
    free(host_im_ptr);
    free(host_col_ptr);
    free(dev_im_ptr);
    free(dev_col_ptr);
    if (ret) {
        printf("INFO: Test Failed\n");
    } else {
        printf("INFO: Test Passed\n");
    }
    return ret;
}

unsigned long xcl_run_kernel3d_mod(xcl_world world, cl_kernel krnl,
                               size_t x, size_t y, size_t z
) {
	size_t size[3] = {x, y, z};
	cl_event event;

	//int err = clEnqueueNDRangeKernel(world.command_queue, krnl, 3,
	//                                 NULL, size, size, 0, NULL, &event);
    OCL_CHECK(clEnqueueNDRangeKernel(world.command_queue, krnl, 3,
                NULL, size, size, 0, NULL, &event));
	//if( err != CL_SUCCESS) {
	//	printf("Error: failed to execute kernel! %d\n", err);
	//	exit(EXIT_FAILURE);
	//}

	clFinish(world.command_queue);

	return xcl_get_event_duration(event);
}

template <typename DType>
int convTest(sda::utils::CmdLineParser &parser) 
{
    int ret = 0;
    cl_int err;

    std::cout << "Try to xcl_world_single" << std::endl;
    xcl_world world = xcl_world_single();

    std::cout << "Try to xcl_import_binary" << std::endl;
    cl_program program = xcl_import_binary(world, "hku");
    
    std::cout << "Try to xcl_get_kernel" << std::endl;
    cl_kernel krnl = xcl_get_kernel(program, "mmult");

    std::cout << "Finished xcl_get_kernel" << std::endl;

    int number = parser.value_to_int("imN");
    int channels = parser.value_to_int("imC");
    int height = parser.value_to_int("imH");
    int width = parser.value_to_int("imW");
    int kernel_h = parser.value_to_int("imKh");
    int kernel_w = parser.value_to_int("imKw");
    int pad_h = parser.value_to_int("imPh");
    int pad_w = parser.value_to_int("imPw");
    int stride_h = parser.value_to_int("imSh");
    int stride_w = parser.value_to_int("imSw");
    int dilation_h = parser.value_to_int("imDh");
    int dilation_w = parser.value_to_int("imDw");

    int width_wrd = ((width - 1)/WrdSize + 1);
    int w_rnd = width_wrd * WrdSize;

    const int output_h = (height + 2 * pad_h
            - (dilation_h * (kernel_h - 1) + 1)) / stride_h + 1;
    const int output_w = (width + 2 * pad_w 
            - (dilation_w * (kernel_w - 1) + 1)) / stride_w + 1;

    //int output_w_rnd = ((output_w - 1) / WrdSize + 1) * WrdSize;

    int col_h = channels * kernel_h * kernel_w;
    int col_w = output_h * output_w;
    //int col_w_rnd = ((col_w - 1) / WrdSize + 1) * WrdSize;
    int output_w_wrd = ((output_w - 1)/WrdSize + 1);
    int output_w_rnd = output_w_wrd * WrdSize;

    int rowsA = parser.value_to_int("rowsA");
    //int colsA = parser.value_to_int("colsA");
    //int colsB = parser.value_to_int("colsB");
    int colsA = col_h;
    int colsB = number * output_h * output_w_wrd * WrdSize;

    int rowsA_wrd = ((rowsA - 1)/WrdSize + 1);
    int rowsA_rnd = rowsA_wrd * WrdSize;

    size_t host_im_size = number * channels * height * width * sizeof(DType);
    size_t dev_im_size = number * channels * height * w_rnd * sizeof(DType);

    size_t host_col_size = number * col_h * col_w * sizeof(DType);
    //size_t dev_col_size = number * col_h * output_h * output_w_rnd * sizeof(DType);
    size_t host_out_size = rowsA * number * col_w * sizeof(DType);
    size_t dev_out_size = rowsA * number * output_h * output_w_rnd * sizeof(DType);

    size_t host_filter_size = rowsA * colsA * sizeof(DType);
    size_t dev_filter_size = rowsA_rnd * colsA * sizeof(DType);

    printf("host_im_size %zu, dev_im_size %zu\n", host_im_size, dev_im_size);
    //printf("host_col_size %zu, dev_col_size %zu\n", host_col_size, dev_col_size);
    printf("host_col_size %zu\n", host_col_size);
    printf("host_out_size %zu, dev_out_size %zu\n", host_out_size, dev_out_size);
    printf("host_filter_size %zu, dev_filter_size %zu\n", host_filter_size, dev_filter_size);

    DType* host_im_ptr = (DType*)malloc(host_im_size);
    DType* host_out_ptr = (DType*)malloc(host_out_size);
    DType* dev_im_ptr = (DType*)malloc(dev_im_size);
    DType* dev_out_ptr = (DType*)malloc(dev_out_size);
    DType* host_col_ptr = (DType*)malloc(host_col_size);
    //DType* dev_col_ptr = (DType*)malloc(dev_col_size);

    DType* host_filter_ptr = (DType*)malloc(host_filter_size);
    DType* dev_filter_ptr = (DType*)malloc(dev_filter_size);

    int width_tiles = ((width - 1) / WrdSize + 1);
    int dev_im_addr = 0;

    for (int i = 0; i < number * channels; i++) {
        for (int tc = 0; tc < width_tiles; tc++) {
            for (int r = 0; r < height; r++) {
                for (int j = 0; j < WrdSize; j++) {
                    int col = WrdSize * tc + j;
                    if (col < width) {
                        //dev_im_addr = ((width_tiles * i + tc) * height + r) * WrdSize + j;
                        dev_im_ptr[dev_im_addr] = width * r + col;
                        host_im_ptr[width * height * i + width * r + col] = width * r + col;
                    } else {
                        //dev_im_addr = ((width_tiles * i + tc) * height + r) * WrdSize + j;
                        dev_im_ptr[dev_im_addr] = -1;
                    }
                    dev_im_addr++;
                }
            }
        }
    }
    int count = 0;
    for (int ra = 0; ra < rowsA_wrd; ra++) {
        for (int c = 0; c < colsA; c++) {
            for (int r = 0; r < WrdSize; r++) {
                int idx = (ra * WrdSize + r) * colsA + c;
                if (ra * WrdSize + r < rowsA) {
                    host_filter_ptr[idx] = idx;
                    dev_filter_ptr[count++] = idx;
                } else {
                    dev_filter_ptr[count++] = -1;
                }
            }
        }
    }

    //cl_event write_events[1], kernel_events[1], read_events[1];
    //cl_mem dev_im_buf, dev_col_buf;
    cl_mem dev_im_buf, dev_filter_buf, dev_out_buf;

    dev_im_buf = xcl_malloc(world, CL_MEM_READ_ONLY, dev_im_size);
    dev_filter_buf = xcl_malloc(world, CL_MEM_READ_ONLY, dev_filter_size);
    //dev_col_buf = xcl_malloc(world, CL_MEM_WRITE_ONLY, dev_col_size);
    dev_out_buf = xcl_malloc(world, CL_MEM_WRITE_ONLY, dev_out_size);

    xcl_memcpy_to_device(world, dev_im_buf, dev_im_ptr, dev_im_size);
    xcl_memcpy_to_device(world, dev_filter_buf, dev_filter_ptr, dev_filter_size);

    unsigned char rshft = 16;
    unsigned char cacheA = (rowsA_wrd * colsA <= 2048 ? 1 : 0);
    unsigned char cacheB = (channels * height * width_wrd <= 6144 ? 1 : 0);
    unsigned char im2col = 1;
    printf("cacheA=%d, cacheB=%d\n", cacheA, cacheB);

    int narg = 0;
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &dev_filter_buf);
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &dev_im_buf);
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &dev_out_buf);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &rowsA);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &colsA);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &colsB);
    xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &rshft);
    xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &cacheA);
    xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &cacheB);
    xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &im2col);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &number);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &channels);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &height);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &width);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &kernel_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &kernel_w);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &pad_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &pad_w);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &stride_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &stride_w);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &dilation_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &dilation_w);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &output_h);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &output_w);

    unsigned long duration = xcl_run_kernel3d_mod(world, krnl, 1, 1, 1);

    //xcl_memcpy_from_device(world, dev_col_ptr, dev_col_buf, dev_col_size);
    xcl_memcpy_from_device(world, dev_out_ptr, dev_out_buf, dev_out_size);

    clFlush(world.command_queue);
    clFinish(world.command_queue);
    
    OCL_CHECK( clReleaseMemObject(dev_im_buf));
    //OCL_CHECK( clReleaseMemObject(dev_col_buf));
    OCL_CHECK( clReleaseMemObject(dev_filter_buf));
    OCL_CHECK( clReleaseMemObject(dev_out_buf));
    OCL_CHECK( clReleaseKernel(krnl));
    OCL_CHECK( clReleaseProgram(program));
    xcl_release_world(world);

    std::cout << "conv duration = " << duration/1000000.0 << " ms" << std::endl;

    int im_off = 0, col_off = 0;

    for (int n = 0; n < number; n++) {
        im2col_cpu<DType>(
                host_im_ptr + im_off, 
                channels, height, width,
                kernel_h, kernel_w,
                pad_h, pad_w,
                stride_h, stride_w,
                dilation_h, dilation_w,
                host_col_ptr + col_off);
        im_off += channels * height * width;
        col_off += col_h * col_w;
    }
    int out_off = 0;
    col_off = 0;
    for (int n = 0; n < number; n++) {
        for (int i = 0; i < rowsA; i++) {
            for (int j = 0; j < col_w; j++) {
                int out_idx = i * col_w + j;
                //DType tmp;
                int tmp;
                for (int k = 0; k < col_h; k++) {
                    int f_idx = i * col_h + k;
                    int c_idx = k * col_w + j;
                    if (k == 0)
                        tmp = host_filter_ptr[f_idx] * host_col_ptr[col_off + c_idx];
                    else
                        tmp += host_filter_ptr[f_idx] * host_col_ptr[col_off + c_idx];
                    //if (out_off + out_idx == 12096) {
                    //    printf("host_filter_ptr[%d]=%d, host_col_ptr[%d]=%d, product=%d, tmp=%d\n",
                    //            f_idx, host_filter_ptr[f_idx],
                    //            col_off + c_idx, host_col_ptr[col_off + c_idx],
                    //            host_filter_ptr[f_idx] * host_col_ptr[col_off + c_idx],
                    //            tmp);
                    //}
                }
                host_out_ptr[out_off + out_idx] = (tmp >> 16);
                //if (out_off + out_idx == 12096) {
                //    printf("out_off=%d, out_idx=%d, tmp=%d, host_out_ptr[%d]=%d\n",
                //            out_off, out_idx, tmp, out_off + out_idx, host_out_ptr[out_off + out_idx]);
                //    printf("i=%d, j=%d\n", i, j);
                //}
            }
        }
        col_off += col_h * col_w;
        out_off += rowsA * col_w;
    }

    int output_w_tiles = (output_w - 1) / WrdSize + 1;
    //int output_tiles = (col_w - 1) / WrdSize + 1;
    int output_tiles = output_h * output_w_tiles;

    int dev_col_addr = 0;
    count = 0;

    for (int n = 0; n < number; n++) {
        for (int tc = 0, out_r = 0, out_tc = 0; tc < output_h * output_w_tiles; tc++) {
            for (int r = 0; r < rowsA; r++) {
                for (int j = 0; j < WrdSize; j++) {
                    int col = out_r * output_w + out_tc * WrdSize + j;
                    int index = (n * rowsA + r) * col_w + col;
                    if (out_tc * WrdSize + j < output_w) {
                        if (dev_out_ptr[count] != host_out_ptr[index]) {
                            printf("ERROR in - %d - actual=%d, expected=%d\n", count, dev_out_ptr[count], host_out_ptr[index]);
                            printf("n=%d, tc=%d, out_r=%d, out_tc=%d, r=%d, j=%d\n", n, tc, out_r, out_tc, r, j);
                            printf("col=%d, index=%d\n", col, index);
                            printf("col_h=%d, col_w=%d\n", col_h, col_w);
                            ret = 1;
                            goto DONE;
                        }
                    }
                    count++;
                }
            }
            if (out_tc == output_w_tiles - 1) {
                out_tc = 0; out_r++;
            } else {
                out_tc++;
            }
        }
    }
    // Compare dev_col_ptr and host_col_ptr
    //for (int n = 0; n < number; n++) {
    //    for (int tc = 0, out_r = 0, out_tc = 0; tc < output_h * output_w_tiles; tc++) {
    //        for (int r = 0; r < col_h; r++) {
    //            for (int j = 0; j < WrdSize; j++) {
    //                //int col = WrdSize * tc + j;
    //                //if (col < col_w) 
    //                int col = out_r * output_w + out_tc * WrdSize + j;
    //                if (out_tc * WrdSize + j < output_w) {
    //                    //dev_col_addr = ((n * output_h * output_w_tiles + tc) * col_h + r) * WrdSize + j;
    //                    int index = col_h * col_w * n + col_w * r + col;
    //                    if (dev_col_ptr[count/*dev_col_addr*/] != host_col_ptr[index]) {
    //                        printf("ERROR in - %d - actual=%d, expected=%d\n", count, dev_col_ptr[count], host_col_ptr[index]);
    //                        printf("number=%d, output_tiles=%d, col_h=%d\n", number, output_tiles, col_h);
    //                        printf("n=%d, tc=%d, r=%d, j=%d\n", n, tc, r, j);
    //                        //printf("col_w=%d, col_w_rnd=%d\n", col_w, col_w_rnd);
    //                        printf("col_w=%d\n", col_w);
    //                        printf("col=%d, index=%d, output_h=%d\n", col, index, output_h);
    //                        ret = 1;
    //                        goto DONE;
    //                    }
    //                }
    //                count++;
    //            }
    //        }
    //        if (out_tc == output_w_tiles - 1) {
    //            out_tc = 0; out_r++;
    //        } else {
    //            out_tc++;
    //        }
    //    }
    //}

DONE:
    free(host_im_ptr);
    free(host_out_ptr);
    free(host_col_ptr);
    free(host_filter_ptr);
    free(dev_im_ptr);
    free(dev_out_ptr);
    free(dev_filter_ptr);
    //free(dev_col_ptr);
    if (ret) {
        printf("INFO: Test Failed\n");
    } else {
        printf("INFO: Test Passed\n");
    }
    return ret;
}


int main(int argc, char **argv)
{
    int ret = 0;
    //Command Line Parser
    sda::utils::CmdLineParser parser;

    parser.addSwitch("--rowsA", "-m", "Number of rows in matrix A",             "16");
    parser.addSwitch("--colsA", "-k", "Number of columns/rows in matrix A/B",   "16");
    parser.addSwitch("--colsB", "-n", "Number of columns in matrix B",          "16");
    parser.addSwitch("--skipTest", "-s", "To skip compare with SW golden output", "", true);
    //parser.addSwitch("--transTest", "-t", "Test krnlTrans instead of krnlMmult", "", true);
    parser.addSwitch("--test", "-t", "HW kernel to test; 0(default) for mmult; 1 for trans; 2 for im2col", "0");
    parser.addSwitch("--imN", "-imn", "im2col N", "2");
    parser.addSwitch("--imC", "-imc", "im2col C", "3");
    parser.addSwitch("--imH", "-imh", "im2col H", "16");
    parser.addSwitch("--imW", "-imw", "im2col W", "16");
    parser.addSwitch("--imKh", "-kh", "im2col kernel_h", "3");
    parser.addSwitch("--imKw", "-kw", "im2col kernel_w", "3");
    parser.addSwitch("--imPh", "-ph", "im2col pad_h", "1");
    parser.addSwitch("--imPw", "-pw", "im2col pad_w", "1");
    parser.addSwitch("--imSh", "-sh", "im2col stride_h", "1");
    parser.addSwitch("--imSw", "-sw", "im2col stride_w", "1");
    parser.addSwitch("--imDh", "-dh", "im2col dilation_h", "1");
    parser.addSwitch("--imDw", "-dw", "im2col dilation_w", "1");
    parser.parse(argc, argv);

    //bool testTrans = (parser.value("transTest").compare(string("true")) == 0);

    //if (testTrans) {
    //    ret = transTest<short, short, 16384>(parser);
    //} else {
    //    ret = mmultTest(parser);
    //}

    switch (parser.value_to_int("test")) {
        case 1:
            ret = transTest<short, short, 16384>(parser);
            break;
        case 2:
            ret = im2colTest<unsigned short>(parser);
            break;
        case 3:
            ret = convTest<short>(parser);
            break;
        default:
            ret = mmultTest(parser);
    }

    return ret;
}
