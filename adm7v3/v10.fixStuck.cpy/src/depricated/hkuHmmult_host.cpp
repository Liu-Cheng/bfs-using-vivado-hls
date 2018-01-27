
//#include <sys/types.h>
//#include <stdio.h>
//#include <CL/cl.h>
#include <iostream>
#include <cmath> // for log2

#include <clblast_half.h>
#include "xcl.h"
#include "oclHelper.h"
#include "cmdlineparser.h"

//#include "hls_half.h" // for fp_struct<half>, hls::numeric_limits
//#include "hls/utils/x_hls_utils.h"

#include "hku_defs.h"

#define halfToClHalf(x) fp_struct<half>(x).to_int()
#define clHalfToHalf(x) fp_struct<half>(x).to_ieee()

//#define floatToClHalf(x) fp_struct<half>((half)(x)).to_int()
//#define clHalfToFloat(x) (float)(fp_struct<half>(x).to_ieee())
#define floatToClHalf(x) FloatToHalf(x)
#define clHalfToFloat(x) HalfToFloat(x)

#define OCL_CHECK(call)                                                        \
  do {                                                                         \
    cl_int err = call;                                                         \
    if (err != CL_SUCCESS) {                                                   \
      printf("Error calling " #call ", error: %s\n", oclErrorCode(err));       \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0);

/* Include the clBLAS header. It includes the appropriate OpenCL headers */
//#include <clBLAS.h>

/* This example uses predefined matrices and their characteristics for
 * simplicity purpose.
 */

//#define M  4
//#define N  3
//#define K  5
#define HKU_VCTR_SIZE A_VCTR_SIZE
// currently TILE_W = 32x4, LCL_TILES = 64, => 128x64=8192
#define HKU_CACHE_W A_TILE_W*A_LCL_TILES

//static const cl_float alpha = 10;

//static const cl_float A[M*K] = {
//    11, 12, 13, 14, 15,
//    21, 22, 23, 24, 25,
//    31, 32, 33, 34, 35,
//    41, 42, 43, 44, 45,
//};
//static const size_t lda = K;        /* i.e. lda = K */

//static const cl_float B[K*N] = {
//    11, 12, 13,
//    21, 22, 23,
//    31, 32, 33,
//    41, 42, 43,
//    51, 52, 53,
//};
//static const size_t ldb = N;        /* i.e. ldb = N */

//static const cl_float beta = 20;

//static cl_float C[M*N] = {
//    11, 12, 13,
//    21, 22, 23,
//    31, 32, 33,
//    41, 42, 43,
//};
//static const size_t ldc = N;        /* i.e. ldc = N */

//static cl_float result[M*N];

int hkuHmmult(
		size_t M, 
		size_t N,
		size_t K, 
		const cl_mem A,
		const cl_mem B,
		cl_mem C,
		//cl_uint numCommandQueues,
		cl_command_queue command_queue,
		cl_kernel krnl_systolic_array,
		cl_uint num_events_in_wait_list,
		const cl_event *event_wait_list,
		cl_event *event
		)
{
    unsigned a_row = M;
    unsigned a_col = K;
    unsigned b_col = N;
	unsigned char a_cache;
	unsigned char b_trans = 0;
	if (K > HKU_CACHE_W) {
		a_cache = 0;
		DBG_cout << "hkuHmmult: a_cache only works if K <= HKU_CACHE_W" << std::endl;
		DBG_cout << "hkuHmmult: K=" << K << ", HKU_CACHE_W=" << HKU_CACHE_W << std::endl;
	} else {
		a_cache = 1;
		DBG_cout << "hkuHmmult: a_cache set to 1" << std::endl;
		DBG_cout << "hkuHmmult: K=" << K << ", HKU_CACHE_W=" << HKU_CACHE_W << std::endl;
	}
    int narg = 0;
    xcl_set_kernel_arg(krnl_systolic_array, narg++, sizeof(cl_mem), &A);
    xcl_set_kernel_arg(krnl_systolic_array, narg++, sizeof(cl_mem), &B);
    xcl_set_kernel_arg(krnl_systolic_array, narg++, sizeof(cl_mem), &C);
    xcl_set_kernel_arg(krnl_systolic_array, narg++, sizeof(unsigned), &a_row);
    xcl_set_kernel_arg(krnl_systolic_array, narg++, sizeof(unsigned), &a_col);
    xcl_set_kernel_arg(krnl_systolic_array, narg++, sizeof(unsigned), &b_col);
    xcl_set_kernel_arg(krnl_systolic_array, narg++, sizeof(unsigned char), &a_cache);
    xcl_set_kernel_arg(krnl_systolic_array, narg++, sizeof(unsigned char), &b_trans);
    //xcl_run_kernel3d(world, krnl_systolic_array, 1, 1, 1);
    //OCL_CHECK(clEnqueueTask(command_queue, krnl_systolic_array, num_events_in_wait_list, event_wait_list, event));
	const size_t global = 1;
	const size_t local = 1;
    OCL_CHECK(clEnqueueNDRangeKernel(command_queue, krnl_systolic_array, 1, NULL, &global, &local, num_events_in_wait_list, event_wait_list, event));
	return 0;
}

void hkuGenHCntMat(size_t rows, size_t cols, cl_half* mat)
{
	size_t cols_up = ((cols - 1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	float cnt = 1.0f;
	for (unsigned i = 0; i < rows; i++) {
		for (unsigned j = 0; j < cols; j++) {
			//mat[i * cols_up + j] = (cl_half)cnt;
			//mat[i * cols_up + j] = fp_struct<half>((half)cnt).to_int();
			mat[i * cols_up + j] = floatToClHalf(cnt);
			DBG_cout << "mat[i * cols_up + j] is " << mat[i * cols_up + j] << std::endl;
			cnt += 1.0f;
		}
	}
}

void hkuGenHIMat(size_t rows, size_t cols, cl_half* mat)
{
	size_t cols_up = ((cols - 1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	cl_half one = floatToClHalf(1.0f);
	cl_half zero = floatToClHalf(0.0f);
	for (unsigned i = 0; i < rows; i++) {
		for (unsigned j = 0; j < cols; j++) {
			if (i == j)
				//mat[i * cols_up + j] = (cl_half)1.0f;
				//mat[i * cols_up + j] = fp_struct<half>((half)1.0f).to_int();
				mat[i * cols_up + j] = one;
			else
				//mat[i * cols_up + j] = (cl_half)0.0f;
				//mat[i * cols_up + j] = fp_struct<half>((half)0.0f).to_int();
				mat[i * cols_up + j] = zero;
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
	size_t K_up = ((K - 1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	size_t N_up = ((N - 1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	std::cout << "hkuHmmultSW: M, K, N = " << M << ", " << K << ", " << N << std::endl;
	for (unsigned i = 0; i < M; i++) {
		std::cout << ".";
		for (unsigned j = 0; j < N; j++) {
			for (unsigned kk = 0; kk < K; kk++) {
				cl_half a = A[i * K_up + kk];
				cl_half b = B[kk * N_up + j];
				cl_half c = (kk == 0 ? 0 : C[i * N_up + j]);
				//half a_host = clHalfToHalf(a);
				//half b_host = clHalfToHalf(b);
				//half c_host = clHalfToHalf(c);
				// Using half in SW is slow like simulation, just use float
				float a_host = clHalfToFloat(a);
				float b_host = clHalfToFloat(b);
				float c_host = clHalfToFloat(c);
				c_host += a_host * b_host;
				//C[i * N_up + j] = halfToClHalf(c_host);
				C[i * N_up + j] = floatToClHalf(c_host);
			}
		}
	}
	std::cout << std::endl;
}

void hkuPrintHMat(size_t rows, size_t cols, cl_half *mat)
{
	size_t cols_up = ((cols - 1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	std::cout << "---- hkuPrintHMat: ----" << std::endl;
	for (unsigned i = 0; i < rows; i++) {
		for (unsigned j = 0; j < cols_up; j++) {
			//half tmp = fp_struct<half>(mat[i * cols_up +j]).to_ieee();
			//half tmp = clHalfToHalf(mat[i * cols_up +j]);
			float tmp = clHalfToFloat(mat[i * cols_up +j]);
			std::cout << tmp << "  ";
		}
		std::cout << std::endl;
	}
}

void msub(size_t rows, size_t cols, cl_half *A, cl_half *B, cl_half *C)
{
	size_t cols_up = ((cols - 1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	for (unsigned i = 0; i < rows; i++) {
		for (unsigned j = 0; j < cols; j++) {
			//half a = clHalfToHalf(A[i * cols_up + j]);
			//half b = clHalfToHalf(B[i * cols_up + j]);
			//half diff = a - b;
			//C[i * cols_up + j] = halfToClHalf(diff);
			float a = clHalfToFloat(A[i * cols_up + j]);
			float b = clHalfToFloat(B[i * cols_up + j]);
			float diff = a - b;
			C[i * cols_up + j] = floatToClHalf(diff);
		}
	}
}

double norm1(size_t rows, size_t cols, cl_half *in)
{
	size_t cols_up = ((cols - 1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	double norm = 0;
	double* norm_cols = (double*)malloc(cols * sizeof(double));

	for (unsigned c = 0; c < cols; c++) {
		norm_cols[c] = 0;
	}
	for (unsigned r = 0; r < rows; r++) {
		for (unsigned c = 0; c < cols; c++) {
			//norm_cols[c] += clHalfToHalf(in[r * cols_up + c]);
			norm_cols[c] += clHalfToFloat(in[r * cols_up + c]);
		}
	}
	for (unsigned c = 0; c < cols; c++) {
		if (norm_cols[c] > norm)
			norm = norm_cols[c];
	}

	free(norm_cols);
	return norm;
}

double hkuHMatDiffRatio(size_t rows, size_t cols, cl_half *in, cl_half *ref)
{
	const size_t MAX_DIM = (rows > cols ? rows : cols);
	size_t cols_up = ((cols - 1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	//const half eps_base_t = hls::numeric_limits<half>::epsilon();
	const float eps_base_t = clHalfToFloat((unsigned short)0x1400);
	const double eps = (double)eps_base_t;
	
	cl_half* diff = (cl_half*)malloc(rows * cols_up * sizeof(cl_half));
	double diff_norm, ref_norm;

	msub(rows, cols, in, ref, diff);
	diff_norm = norm1(rows, cols, diff);
	ref_norm = norm1(rows, cols, ref);

	free(diff);
	return (diff_norm / (ref_norm * MAX_DIM * eps));
}

int main(int argc, char **argv)
{
    int M, K, N;
    //Command Line Parser
    sda::utils::CmdLineParser parser;

    parser.addSwitch("--a_rows", "-m", "Number of rows in matrix A",             "256");
    parser.addSwitch("--a_cols", "-k", "Number of columns/rows in matrix A/B",   "256");
    parser.addSwitch("--b_cols", "-n", "Number of columns in matrix B",          "256");
    parser.parse(argc, argv);

	// 64k x 8k = 512M elem, x 2Bytes per half = 1GByte
	M = parser.value_to_int("a_rows");
	K = parser.value_to_int("a_cols");
	N = parser.value_to_int("b_cols");

    cl_int err;
    cl_mem bufA, bufB, bufC1, bufC2;
    cl_event write_events[2];
    cl_event kernel_events[2];
    cl_event read_events[2];
    cl_event map_events[2];
    int ret = 0;

	std::cout << "Try to xcl_world_single" << std::endl;
    xcl_world world = xcl_world_single();
	//clReleaseCommandQueue(world.command_queue);
	//world.command_queue = clCreateCommandQueue(world.context, world.device_id, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);

	std::cout << "Try to xcl_import_binary" << std::endl;
    cl_program program = xcl_import_binary(world, "mmult");
	std::cout << "Try to xcl_get_kernel" << std::endl;
    cl_kernel krnl_systolic_array = xcl_get_kernel(program, "mmult");
	std::cout << "Finished xcl_get_kernel" << std::endl;

	size_t K_up = ((K-1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	size_t N_up = ((N-1)/HKU_VCTR_SIZE + 1) * HKU_VCTR_SIZE;
	cl_half* A = (cl_half*)malloc(M * K_up * sizeof(cl_half));
	cl_half* B = (cl_half*)malloc(K * N_up * sizeof(cl_half));
	cl_half* C1 = (cl_half*)malloc(M * N_up * sizeof(cl_half));
	cl_half* C2;
	if (M == N && N == K) C2 = (cl_half*)malloc(M * N_up * sizeof(cl_half));
	std::cout << "Filling host matrix A and B" << std::endl;
	hkuGenHCntMat(M, K_up, A);
	hkuGenHIMat(K, N_up, B);

    bufA = xcl_malloc(world, CL_MEM_READ_ONLY, M * K_up * sizeof(*A));
    bufB = xcl_malloc(world, CL_MEM_READ_ONLY, K * N_up * sizeof(*B));
    bufC1 = xcl_malloc(world, CL_MEM_WRITE_ONLY, M * N_up * sizeof(*C1));
	if (M == N && N == K) bufC2 = xcl_malloc(world, CL_MEM_WRITE_ONLY, M * N_up * sizeof(*C2));
	//bufA = clCreateBuffer(world.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, M * K_up * sizeof(*A), A, &err);
	//bufB = clCreateBuffer(world.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, K * N_up * sizeof(*B), B, &err);
	//bufC1 = clCreateBuffer(world.context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, M * N_up * sizeof(*C1), C1, &err);
	//if (M == N && N == K) bufC2 = clCreateBuffer(world.context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, M * N_up * sizeof(*C2), C2, &err);

	std::cout << "Migrating bufA and bufB" << std::endl;
	/* ---- begin test 1, A x I == A ---- */
    //xcl_memcpy_to_device(world, bufA, A, M * K_up * sizeof(*A));
    //xcl_memcpy_to_device(world, bufB, B, K * N_up * sizeof(*B));
	OCL_CHECK( clEnqueueWriteBuffer(world.command_queue, bufA, CL_TRUE, 0, M * K_up * sizeof(*A), A, 0, NULL, &write_events[0]));
	OCL_CHECK( clEnqueueWriteBuffer(world.command_queue, bufB, CL_TRUE, 0, K * N_up * sizeof(*B), B, 0, NULL, &write_events[1]));
    //OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufA, 0 /* flags, 0 means from host*/, 0, NULL, &write_events[0]));
    //OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufB, 0 /* flags, 0 means from host*/, 0, NULL, &write_events[1]));
	//clFinish(world.command_queue); // if host->global write and kernel->global write deadlocks, need wait

	std::cout << "Launching kernel 1" << std::endl;
	err = hkuHmmult(M, N, K, bufA, bufB, bufC1, world.command_queue, krnl_systolic_array, 2, write_events, &kernel_events[0]);
	clFinish(world.command_queue);

    //xcl_memcpy_from_device(world, C1, bufC1, M * N_up * sizeof(*C1));
	OCL_CHECK( clEnqueueReadBuffer(world.command_queue, bufC1, CL_TRUE, 0, M * N_up * sizeof(*C1), C1, 1, &kernel_events[0], &read_events[0]));
    //OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufC1, CL_MIGRATE_MEM_OBJECT_HOST, 1, &kernel_events[0], &read_events[0]));
    //clEnqueueMapBuffer(world.command_queue, bufC1, CL_FALSE, CL_MAP_READ, 0, M * N_up * sizeof(*C1), 1, &read_events[0], &map_events[0], &err);
	/* ---- end test 1, A x I == A ---- */

	/* ---- begin test 2, I x A == A ---- */
	if (M == N && N == K) {
		std::cout << "Launching kernel 2" << std::endl;
		err = hkuHmmult(M, N, K, bufB, bufA, bufC2, world.command_queue, krnl_systolic_array, 0, NULL, &kernel_events[1]);
		clFinish(world.command_queue);
		//xcl_memcpy_from_device(world, C2, bufC2, M * N_up * sizeof(*C2));
		OCL_CHECK( clEnqueueReadBuffer(world.command_queue, bufC2, CL_TRUE, 0, M * N_up * sizeof(*C2), C2, 1, &kernel_events[1], &read_events[1]));
		//OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufC2, CL_MIGRATE_MEM_OBJECT_HOST, 1, &kernel_events[1], &read_events[1]));
		//clEnqueueMapBuffer(world.command_queue, bufC2, CL_FALSE, CL_MAP_READ, 0, M * N_up * sizeof(*C2), 1, &read_events[1], &map_events[1], &err);
	}
	/* ---- end test 2, I x A == A ---- */

    clFlush(world.command_queue);
    clFinish(world.command_queue);

	for(int i = 0 ; i < 2 ; i++){
		//OCL_CHECK( clWaitForEvents(1, &map_events[i]));
		OCL_CHECK( clWaitForEvents(1, &read_events[i]));
	}

	unsigned long nsduration;
	nsduration = xcl_get_event_duration(write_events[0]);
	double writeAsec = ((double)nsduration) / ((double) 1000000000);
	nsduration = xcl_get_event_duration(write_events[1]);
	double writeBsec = ((double)nsduration) / ((double) 1000000000);
	double AsizeMb = (double)(M * K_up * sizeof(*A))/(1024*1024);
	std::cout << "Write matrix A to global: size = " << AsizeMb << " MBytes" << std::endl;
	std::cout << "Write matrix A to global: time = " << writeAsec << " (sec)" << std::endl;
	std::cout << "Write matrix A to global: throughput = " << AsizeMb/writeAsec << " (MB/sec)" << std::endl;
	double BsizeMb = (double)(K * N_up * sizeof(*B))/(1024*1024);
	std::cout << "Write matrix B to global: size = " << BsizeMb << " MBytes" << std::endl;
	std::cout << "Write matrix B to global: time = " << writeBsec << " (sec)" << std::endl;
	std::cout << "Write matrix B to global: throughput = " << BsizeMb/writeBsec << " (MB/sec)" << std::endl;

	nsduration = xcl_get_event_duration(kernel_events[0]);
	double krnl0sec = ((double)nsduration) / ((double) 1000000000);
	nsduration = xcl_get_event_duration(read_events[0]);
	double readC1sec = ((double)nsduration) / ((double) 1000000000);
	//nsduration = xcl_get_event_duration(map_events[0]);
	//double mapC1sec = ((double)nsduration) / ((double) 1000000000);
	double CsizeMb = (double)(M * N_up * sizeof(*C1))/(1024*1024);

	double multOps = (double)M * (double)N * (double)K;
	double addOps = (double)M * (double)N * (double)(K-1); // assumming K is power of 2, and add by reduction tree
	std::cout << "kernel 1st execution: time = " << krnl0sec << " (sec)" << std::endl;
	std::cout << "kernel 1st execution: mult ops = " << multOps << std::endl;
	std::cout << "kernel 1st execution: add ops = " << addOps << std::endl;
	std::cout << "kernel 1st execution: compute throughput = " << (multOps + addOps)/(krnl0sec*1000*1000) << " (MFLOPS)" << std::endl;
	std::cout << "Read matrix C1 to global: size = " << CsizeMb << " MBytes" << std::endl;
	std::cout << "Read matrix C1 to global: time = " << readC1sec << " (sec)" << std::endl;
	//std::cout << "Map matrix C1 to global: time = " << mapC1sec << " (sec)" << std::endl;
	//std::cout << "Read matrix C1 to global: throughput = " << CsizeMb/readC1sec << " (MB/sec)" << std::endl;

	nsduration = xcl_get_event_duration(kernel_events[1]);
	double krnl1sec = ((double)nsduration) / ((double) 1000000000);
	nsduration = xcl_get_event_duration(read_events[1]);
	double readC2sec = ((double)nsduration) / ((double) 1000000000);
	//nsduration = xcl_get_event_duration(map_events[1]);
	//double mapC2sec = ((double)nsduration) / ((double) 1000000000);
	std::cout << "kernel 2nd execution: time = " << krnl1sec << " (sec)" << std::endl;
	std::cout << "Read matrix C2 to global: size = " << CsizeMb << " MBytes" << std::endl;
	std::cout << "Read matrix C2 to global: time = " << readC2sec << " (sec)" << std::endl;
	//std::cout << "Map matrix C2 to global: time = " << mapC2sec << " (sec)" << std::endl;
	//std::cout << "Read matrix C2 to global: throughput = " << CsizeMb/readC2sec << " (MB/sec)" << std::endl;

	for(int i = 0 ; i < 2 ; i++){
		OCL_CHECK( clReleaseEvent(write_events[i]));
		OCL_CHECK( clReleaseEvent(kernel_events[i]));
		OCL_CHECK( clReleaseEvent(read_events[i]));
		//OCL_CHECK( clReleaseEvent(map_events[i]));
	}
	if (M == N && N == K) clReleaseMemObject( bufC2 );
    clReleaseMemObject( bufC1 );
    clReleaseMemObject( bufB );
    clReleaseMemObject( bufA );

    OCL_CHECK( clReleaseKernel(krnl_systolic_array));
    OCL_CHECK( clReleaseProgram(program));
    xcl_release_world(world);
//OPENCL HOST CODE AREA END

	/*
	cl_half* C_sw = (cl_half*)malloc(M * N_up * sizeof(cl_half));
	std::cout << "Calculating golden output on host" << std::endl;
	hkuHmmultSW(M, K, N, A, B, C_sw);

	std::cout << "Start comparing C1 against golden output on host" << std::endl;
	double diffRatio = hkuHMatDiffRatio(M, N, C1, C_sw);
	if (diffRatio > 30.0) {
		std::cout << "---- Failed test hkuHmmult A x I == A, b_trans=false ----" << std::endl;
		std::cout << "---- diff ratio is: " << diffRatio << " ----" << std::endl;
		//hkuPrintHMat(M, K, A);
		//hkuPrintHMat(K, N, B);
		std::cout << "---- HW result: ----" << std::endl;
		hkuPrintHMat(M, N, C1);
		std::cout << "---- SW result: ----" << std::endl;
		hkuPrintHMat(M, N, C_sw);
	} else {
		std::cout << "---- Passed test hkuHmmult A x I == A, b_trans=false ----" << std::endl;
		std::cout << "---- diff ratio is: " << diffRatio << " ----" << std::endl;
	}

	if (M == N && N == K) {
		std::cout << "Start comparing C2 against golden output on host" << std::endl;
		diffRatio = hkuHMatDiffRatio(M, N, C2, C_sw);
		if (diffRatio > 30.0) {
			std::cout << "---- Failed test hkuHmmult I x A == A, b_trans=false ----" << std::endl;
			std::cout << "---- diff ratio is: " << diffRatio << " ----" << std::endl;
			//hkuPrintHMat(M, K, A);
			//hkuPrintHMat(K, N, B);
			std::cout << "---- HW result: ----" << std::endl;
			hkuPrintHMat(M, N, C2);
			std::cout << "---- SW result: ----" << std::endl;
			hkuPrintHMat(M, N, C_sw);
		} else {
			std::cout << "---- Passed test hkuHmmult I x A == A, b_trans=false ----" << std::endl;
			std::cout << "---- diff ratio is: " << diffRatio << " ----" << std::endl;
		}
	}

	free(C_sw);
	*/
	if (M == N && N == K) free(C2);
	free(C1);
	free(B);
	free(A);
    return ret;
}
