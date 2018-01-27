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

#define MEM_SIZE (64*1024*1024)

int adaptive_burst_num(const int &burst_len){
	if(burst_len <= 16 * 1024){
		return 4*1024;
	}
	else{
		return 64;
	}
}

int main(int argc, char **argv) {

	// Initial setup
	int burst_len = 2;
	int stride = 1379;
	int inc = 10;
	int burst_num = adaptive_burst_num(burst_len);

	// Allocate memory for host data
    int *input = (int*) malloc(MEM_SIZE * sizeof(int));
    int *output = (int*) malloc(MEM_SIZE * sizeof(int)); 
	int *ref_output = (int*)malloc(MEM_SIZE * sizeof(int));
	int result = 0;
	int ref_result = 0;

	//Init the memory
	for(int i = 0; i < MEM_SIZE; i++){
		input[i] = rand()%100;
		output[i] = 0;
		ref_output[i] = 0;
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

	int base_write_addr = 0x30000;
	for(int i = 0; i < burst_num; i++){
		for(int j = 0; j < burst_len; j++){
			ref_output[base_write_addr + i * stride + j] = inc;
		}
	}

	//--------------------------------
	// Create OpenCl thread
	//--------------------------------
	xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "mem_test");
	cl_kernel krnl_mem_test = xcl_get_kernel(program, "mem_test");

    cl_mem devMemInput = xcl_malloc(world, CL_MEM_READ_ONLY, MEM_SIZE * sizeof(int));
	cl_mem devMemOutput = xcl_malloc(world, CL_MEM_READ_WRITE, MEM_SIZE * sizeof(int));
	cl_mem devMemResult = xcl_malloc(world, CL_MEM_WRITE_ONLY, sizeof(int));

	xcl_memcpy_to_device(world, devMemInput, input, MEM_SIZE * sizeof(int));
	xcl_memcpy_to_device(world, devMemOutput, output, MEM_SIZE * sizeof(int));

	int nargs = 0;
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemOutput);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &inc);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &burst_num);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &stride);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult);
	std::clock_t begin = clock();
	unsigned long duration = xcl_run_kernel3d(world, krnl_mem_test, 1, 1, 1);
	std::clock_t end = clock();
	xcl_memcpy_from_device(world, &result, devMemResult, sizeof(int));
	xcl_memcpy_from_device(world, output, devMemOutput, MEM_SIZE * sizeof(int));

	//Verification
	if(result == ref_result){
		std::cout << "Read verification passed." << std::endl;
	}
	else{
		std::cout << "Read verfication failed." << std::endl;
	}

	bool pass = true;
	for(int i = 0; i < MEM_SIZE; i++){
		if(output[i] != ref_output[i]){
			std::cout << "Write verification failed." << std::endl;
			pass = false;
			break;
		}
	}
	if(pass) std::cout << "Write verification passed." << std::endl;

	double elapsed_time = 1.0*(end - begin)/CLOCKS_PER_SEC;
	elapsed_time = duration * 1.0/1e9;
	double average_latency = elapsed_time * 1.0 * 1e9 /(2.0 * burst_len * burst_num);
	std::cout << "average latency: ";
	std::cout << average_latency << ", " << std::endl;

	double bandwidth = 2.0 * burst_len * burst_num * sizeof(int)/1024/1024/elapsed_time;
	std::cout << "Average bandwidth: ";
	std::cout << bandwidth << ", " << std::endl;

	clReleaseMemObject(devMemInput);
	clReleaseMemObject(devMemOutput);
	clReleaseMemObject(devMemResult);
	clReleaseKernel(krnl_mem_test);
	clReleaseProgram(program);
	xcl_release_world(world);

	free(input);
	free(output);
	free(ref_output);

	return 0;
}
