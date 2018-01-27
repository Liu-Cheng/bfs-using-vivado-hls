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
	int inc = 10;

    int burst_num = adaptive_burst_num(burst_len);
    int *input = (int*) malloc(MEM_SIZE * sizeof(int));
	int *scattered_result = (int*)malloc(16*sizeof(int));
	int result = 0;
	int ref_result = 0;

	//Init the memory
	for(int i = 0; i < MEM_SIZE; i++){
		input[i] = rand()%10;
	}

	//-----------------------------------------------
	// Software data movement
	// ----------------------------------------------
	int sum = 0;
	int base_read_addr = 0x20000;
	for(int i = 0; i < burst_num; i++){
		for(int j = 0; j < burst_len; j++){
			sum += input[base_read_addr + i * stride + j];
		}
	}
	ref_result = sum;

	// Data movement using FPGA
	xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "mem_test");
	cl_kernel krnl_mem_test = xcl_get_kernel(program, "mem_test");

    cl_mem devMemInput = xcl_malloc(world, CL_MEM_READ_ONLY, MEM_SIZE * sizeof(int));
	cl_mem devMemResult = xcl_malloc(world, CL_MEM_WRITE_ONLY, 16*sizeof(int));
	xcl_memcpy_to_device(world, devMemInput, input, MEM_SIZE * sizeof(int));

	int nargs = 0;
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &inc);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &burst_num);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &burst_len);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &stride);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult);
	unsigned long duration_ns = xcl_run_kernel3d(world, krnl_mem_test, 1, 1, 1);
	xcl_memcpy_from_device(world, scattered_result, devMemResult, 16*sizeof(int));

	//Verification
	for(int i = 0; i < 16; i++){
		result += scattered_result[i];
	}

	if(result == ref_result){
		std::cout << "Read verification passed." << std::endl;
	}
	else{
		std::cout << "result = " << result << " ref_result = " << ref_result << std::endl;
		std::cout << "Read verfication failed." << std::endl;
	}

	double average_latency = duration_ns*1.0/(burst_num * burst_len);
	std::cout << "average latency: ";
	std::cout << average_latency << ", " << std::endl;

	double elapsed_time = duration_ns * 1.0 /(1000000000.0);
    //std::cout << "total mem transfer time: " << elapsed_time << "s" << std::endl;
	//double elapsed_time = (end - begin)*1.0/CLOCKS_PER_SEC;

	double bandwidth = burst_num * burst_len * sizeof(int)/1024/1024/elapsed_time;
	std::cout << "Average bandwidth: ";
	std::cout << bandwidth << " MB/s" << std::endl;

	clReleaseMemObject(devMemInput);
	clReleaseMemObject(devMemResult);
	clReleaseKernel(krnl_mem_test);
	clReleaseProgram(program);
	xcl_release_world(world);

	free(input);
	free(scattered_result);

	return 0;
}
