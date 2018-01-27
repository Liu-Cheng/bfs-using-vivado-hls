/*
 * hkuCl.cpp: hku OpenCl API implementations
 */
#include "xcl.h"
#include "clblast_half.h"
#include "hkuCommon.h" // for debug
#include "hkuCl.h" // for OCL_CHECK

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
        )
{
    int rowsA = M;
    int colsA = N;
    unsigned char colMajor = (isColMajor==true?1:0);
    int narg = 0;
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &A);
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &AT);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &rowsA);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &colsA);
    xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &colMajor);
    #define LAUNCH_METHOD 1
    #if LAUNCH_METHOD == 1
    OCL_CHECK( clEnqueueTask(command_queue, krnl, num_events_in_wait_list, event_wait_list, event));
    #elif LAUNCH_METHOD == 2
    size_t global = 1;
    size_t local = 1;
    OCL_CHECK( clEnqueueNDRangeKernel(command_queue, krnl, 1, NULL, &global, &local, num_events_in_wait_list, event_wait_list, event));
    #endif
    return 0;
}

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
        )
{
    int rowsA = M;
    int colsA = K;
    int colsB = N;
    unsigned char cacheA = (K * ((M-1)/WrdHalfSize + 1) <= BUF_SIZE_A)? 1:0;
    unsigned char cacheB = (K <= BUF_SIZE_B)? 1:0;
    unsigned char rshft = 16;
    char im2col = 0;
    //unsigned char transA = 0;
    //unsigned char transB = 0;
    int number      = 0; //parser.value_to_int("imN");
    int channels    = 3; //parser.value_to_int("imC");
    int height      = 3; //parser.value_to_int("imH");
    int width       = 3; //parser.value_to_int("imW");
    int kernel_h    = 3; //parser.value_to_int("imKh");
    int kernel_w    = 3; //parser.value_to_int("imKw");
    int pad_h       = 0; //parser.value_to_int("imPh");
    int pad_w       = 0; //parser.value_to_int("imPw");
    int stride_h    = 1; //parser.value_to_int("imSh");
    int stride_w    = 1; //parser.value_to_int("imSw");
    int dilation_h  = 1; //parser.value_to_int("imDh");
    int dilation_w  = 1; //parser.value_to_int("imDw");
    const int output_h = (height + 2 * pad_h
            - (dilation_h * (kernel_h - 1) + 1)) / stride_h + 1;
    const int output_w = (width + 2 * pad_w 
            - (dilation_w * (kernel_w - 1) + 1)) / stride_w + 1;
    //if (K > CACHE_W) {
    //    cacheA = 0;
    //    DBG_cout << "hkuHmmult: cacheA only works if K <= CACHE_W" << std::endl;
    //    DBG_cout << "hkuHmmult: K=" << K << ", CACHE_W=" << CACHE_W << std::endl;
    //} else {
    //    cacheA = 1;
    //    DBG_cout << "hkuHmmult: cacheA set to 1" << std::endl;
    //    DBG_cout << "hkuHmmult: K=" << K << ", CACHE_W=" << CACHE_W << std::endl;
    //}
    int narg = 0;
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &A);
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &B);
    xcl_set_kernel_arg(krnl, narg++, sizeof(cl_mem), &C);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &rowsA);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &colsA);
    xcl_set_kernel_arg(krnl, narg++, sizeof(int), &colsB);
    xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &rshft);
    xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &cacheA);
    xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &cacheB);
    xcl_set_kernel_arg(krnl, narg++, sizeof(char), &im2col);
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
    //xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &transA);
    //xcl_set_kernel_arg(krnl, narg++, sizeof(unsigned char), &transB);
    #define LAUNCH_METHOD 1
    //#if LAUNCH_METHOD == 0
    //xcl_run_kernel3d(world, krnl, 1, 1, 1);
    #if LAUNCH_METHOD == 1
    OCL_CHECK( clEnqueueTask(command_queue, krnl, num_events_in_wait_list, event_wait_list, event));
    #elif LAUNCH_METHOD == 2
    size_t global = 1;
    size_t local = 1;
    OCL_CHECK( clEnqueueNDRangeKernel(command_queue, krnl, 1, NULL, &global, &local, num_events_in_wait_list, event_wait_list, event));
    #endif
    return 0;
}

void hkuGenHCntMat(size_t rows, size_t cols, cl_half* mat)
{
    size_t colsUp = RndWrdHalf(cols);
    float cnt = 1.0f;
    DEBUG_PRINT("hkuGenHCntMat: ");
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            mat[colsUp*i + j] = FloatToHalf(cnt);
            cnt += 1.0f;
            DBG_cout << mat[colsUp*i + j] << " ";
        }
        DEBUG_PRINT("\n");
    }
}

void hkuGenHIMat(size_t rows, size_t cols, cl_half* mat)
{
    size_t colsUp = RndWrdHalf(cols);
    cl_half one = FloatToHalf(1.0f);
    cl_half zero = FloatToHalf(0.0f);
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            if (i == j)
                mat[colsUp*i + j] = one;
            else
                mat[colsUp*i + j] = zero;
        }
    }
}

void hkuHmmultSW(
        size_t M, 
        size_t K, 
        size_t N, 
        cl_half *A, 
        cl_half *B, 
        cl_half *C)
{
    size_t upK = RndWrdHalf(K);
    size_t upN = RndWrdHalf(N);
    std::cout << "hkuHmmultSW: M, K, N = " << M << ", " << K << ", " << N << std::endl;
    for (size_t i = 0; i < M; i++) {
        std::cout << ".";
        for (size_t j = 0; j < N; j++) {
            for (size_t kk = 0; kk < K; kk++) {
                cl_half rdA = A[upK*i + kk];
                cl_half rdB = B[upN*kk + j];
                cl_half rdC = (kk == 0 ? 0 : C[upN*i + j]);
                float elemA = HalfToFloat(rdA);
                float elemB = HalfToFloat(rdB);
                float elemC = HalfToFloat(rdC);
                elemC += elemA * elemB;
                C[upN*i + j] = FloatToHalf(elemC);
            }
        }
    }
    std::cout << std::endl;
}

void hkuPrintHMat(size_t rows, size_t cols, cl_half *mat)
{
    size_t colsUp = RndWrdHalf(cols);
    std::cout << "---- hkuPrintHMat: ----" << std::endl;
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < colsUp; j++) {
            float tmp = HalfToFloat(mat[colsUp*i + j]);
            std::cout << tmp << "  ";
        }
        std::cout << std::endl;
    }
}

double norm1(size_t rows, size_t cols, cl_half *in)
{
    size_t colsUp = RndWrdHalf(cols);
    double norm = 0;
    double* norm_cols = (double*)malloc(cols * sizeof(double));

    for (size_t c = 0; c < cols; c++) {
        norm_cols[c] = 0;
    }
    for (size_t r = 0; r < rows; r++) {
        for (unsigned c = 0; c < cols; c++) {
            norm_cols[c] += HalfToFloat(in[colsUp*r + c]);
        }
    }
    for (size_t c = 0; c < cols; c++) {
        if (norm_cols[c] > norm)
            norm = norm_cols[c];
    }

    free(norm_cols);
    return norm;
}

void msub(size_t rows, size_t cols, cl_half *A, cl_half *B, cl_half *C)
{
    size_t colsUp = RndWrdHalf(cols);
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            float a = HalfToFloat(A[colsUp*i + j]);
            float b = HalfToFloat(B[colsUp*i + j]);
            float diff = a - b;
            C[colsUp*i + j] = FloatToHalf(diff);
        }
    }
}

double hkuHMatDiffRatio(size_t rows, size_t cols, cl_half *in, cl_half *ref)
{
    const size_t MAX_DIM = (rows > cols ? rows : cols);
    size_t colsUp = RndWrdHalf(cols);
    //const half eps_base_t = hls::numeric_limits<half>::epsilon();
    const float eps_base_t = HalfToFloat((unsigned short)0x1400);
    const double eps = (double)eps_base_t;
    
    cl_half* diff = (cl_half*)malloc(rows * colsUp * sizeof(cl_half));
    double diff_norm, ref_norm;

    msub(rows, cols, in, ref, diff);
    diff_norm = norm1(rows, cols, diff);
    ref_norm = norm1(rows, cols, ref);

    free(diff);
    return (diff_norm / (ref_norm * MAX_DIM * eps));
}

