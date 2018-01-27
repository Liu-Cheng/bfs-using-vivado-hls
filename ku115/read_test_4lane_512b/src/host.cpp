/**********
Author: Cheng Liu
Email: st.liucheng@gmail.com
Software: SDx 2016.4
Date: July 6th 2017
**********/

#include "xcl.h"
#include <cstdio>
#include <vector>
#include <ctime>
#include <iostream>

#define MEM_SIZE (256*1024*1024)

int adaptive_burst_num(const int &burst_len){
	if(burst_len <= 16){
		return 1024 * 1024;
	}
	else if(burst_len <= 16 * 1024){
		return 1024;
	}
	else{
		return 64;
	}
}


int main(int argc, char **argv) {
	int burst_len = 16384;
	int stride = 1024;

    int burst_num = adaptive_burst_num(burst_len);
    int *input = (int*) malloc(MEM_SIZE * sizeof(int));
	int *scattered_result0 = (int*)malloc(16*sizeof(int));
	int *scattered_result1 = (int*)malloc(16*sizeof(int));
	int *scattered_result2 = (int*)malloc(16*sizeof(int));
	int *scattered_result3 = (int*)malloc(16*sizeof(int));
	int result0 = 0;
	int result1 = 0;
	int result2 = 0;
	int result3 = 0;
	int ref_result0 = 0;
	int ref_result1 = 0;
	int ref_result2 = 0;
	int ref_result3 = 0;

	//Init the memory
	for(int i = 0; i < MEM_SIZE; i++){
		input[i] = rand()%10;
	}

	//-----------------------------------------------
	// Software data movement
	// ----------------------------------------------
	int base_read_addr0 = 0x20000;
	int base_read_addr1 = 0x30000;
	int base_read_addr2 = 0x40000;
	int base_read_addr3 = 0x50000;
	for(int i = 0; i < burst_num; i++){
		for(int j = 0; j < burst_len; j++){
			ref_result0 += input[base_read_addr0 + i * stride + j];
			ref_result1 += input[base_read_addr1 + i * stride + j];
			ref_result2 += input[base_read_addr2 + i * stride + j];
			ref_result3 += input[base_read_addr3 + i * stride + j];
		}
	}

	// Data movement using FPGA
	xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "mem_test");
	cl_kernel krnl_mem_test = xcl_get_kernel(program, "mem_test");

    cl_mem devMemInput = xcl_malloc(world, CL_MEM_READ_ONLY, MEM_SIZE * sizeof(int));
	cl_mem devMemResult0 = xcl_malloc(world, CL_MEM_WRITE_ONLY, 16*sizeof(int));
	cl_mem devMemResult1 = xcl_malloc(world, CL_MEM_WRITE_ONLY, 16*sizeof(int));
	cl_mem devMemResult2 = xcl_malloc(world, CL_MEM_WRITE_ONLY, 16*sizeof(int));
	cl_mem devMemResult3 = xcl_malloc(world, CL_MEM_WRITE_ONLY, 16*sizeof(int));
	xcl_memcpy_to_device(world, devMemInput, input, MEM_SIZE * sizeof(int));

	int nargs = 0;
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &burst_num);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &burst_len);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &stride);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult0);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult1);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult2);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult3);
	unsigned long duration_ns = xcl_run_kernel3d(world, krnl_mem_test, 1, 1, 1);
	xcl_memcpy_from_device(world, scattered_result0, devMemResult0, 16*sizeof(int));
	xcl_memcpy_from_device(world, scattered_result1, devMemResult1, 16*sizeof(int));
	xcl_memcpy_from_device(world, scattered_result2, devMemResult2, 16*sizeof(int));
	xcl_memcpy_from_device(world, scattered_result3, devMemResult3, 16*sizeof(int));

	//Verification
	for(int i = 0; i < 16; i++){
		result0 += scattered_result0[i];
		result1 += scattered_result1[i];
		result2 += scattered_result2[i];
		result3 += scattered_result3[i];
	}

	if(result0 == ref_result0 && result1 == ref_result1 && result2 == ref_result2 && result3 == ref_result3){
		std::cout << "Read verification passed." << std::endl;
	}
	else{
		std::cout << "Read verfication failed." << std::endl;
	}

	//double average_latency = duration_ns*1.0/(4*burst_num * burst_len);
	//std::cout << "average latency: ";
	//std::cout << average_latency << ", " << std::endl;

	double elapsed_time = duration_ns * 1.0 /(1000000000.0);
    //std::cout << "total mem transfer time: " << elapsed_time << "s" << std::endl;
	//double elapsed_time = (end - begin)*1.0/CLOCKS_PER_SEC;

	double bandwidth = 4 * burst_num * burst_len * sizeof(int)/1024/1024/elapsed_time;
	std::cout << "Average bandwidth: ";
	std::cout << bandwidth << " MB/s" << std::endl;

	clReleaseMemObject(devMemInput);
	clReleaseMemObject(devMemResult0);
	clReleaseMemObject(devMemResult1);
	clReleaseMemObject(devMemResult2);
	clReleaseMemObject(devMemResult3);
	clReleaseKernel(krnl_mem_test);
	clReleaseProgram(program);
	xcl_release_world(world);

	free(input);
	free(scattered_result0);
	free(scattered_result1);
	free(scattered_result2);
	free(scattered_result3);

	return 0;
}
