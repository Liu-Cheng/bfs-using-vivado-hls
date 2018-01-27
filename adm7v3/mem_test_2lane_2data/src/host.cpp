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
		return 16*1024;
	}
	else{
		return 64;
	}
}


int main(int argc, char **argv) {
int burst_len = 2;
int stride = 1379;
	int inc = 10;
    int burst_num = adaptive_burst_num(burst_len);

    int *input = (int*) malloc(MEM_SIZE * sizeof(int));
    int *output0 = (int*) malloc(MEM_SIZE * sizeof(int)); 
    int *output1 = (int*) malloc(MEM_SIZE * sizeof(int)); 
    int *ref_output0 = (int*) malloc(MEM_SIZE * sizeof(int)); 
    int *ref_output1 = (int*) malloc(MEM_SIZE * sizeof(int)); 
	int result0 = 0;
	int result1 = 0;
	int ref_result0 = 0;
	int ref_result1 = 0;

	//Init the memory
	for(int i = 0; i < MEM_SIZE; i++){
		input[i] = rand();
		output0[i] = 0;
		output1[i] = 0;
		ref_output0[i] = 0;
		ref_output1[i] = 0;
	}

	//-----------------------------------------------
	// Software data movement
	// ----------------------------------------------
	int sum0 = 0;
	int sum1 = 0;
	int base_read_addr0 = 0x20000;
	int base_read_addr1 = 0x40000;
	for(int i = 0; i < burst_num; i++){
		for(int j = 0; j < burst_len; j++){
			sum0 += input[base_read_addr0 + i * stride + j];
			sum1 += input[base_read_addr1 + i * stride + j];
		}
	}
	ref_result0 = sum0;
	ref_result1 = sum1;

	int base_write_addr0 = 0x80000;
	int base_write_addr1 = 0xA0000;
	for(int i = 0; i < burst_num; i++){
		for(int j = 0; j < burst_len; j++){
			ref_output0[base_write_addr0 + i * stride + j] = inc;
			ref_output1[base_write_addr1 + i * stride + j] = inc;
		}
	}

	xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "mem_test");
	cl_kernel krnl_mem_test = xcl_get_kernel(program, "mem_test");

    cl_mem devMemInput = xcl_malloc(world, CL_MEM_READ_ONLY, MEM_SIZE * sizeof(int));
	cl_mem devMemOutput0 = xcl_malloc(world, CL_MEM_READ_WRITE, MEM_SIZE * sizeof(int));
	cl_mem devMemOutput1 = xcl_malloc(world, CL_MEM_READ_WRITE, MEM_SIZE * sizeof(int));
	cl_mem devMemResult0 = xcl_malloc(world, CL_MEM_WRITE_ONLY, sizeof(int));
	cl_mem devMemResult1 = xcl_malloc(world, CL_MEM_WRITE_ONLY, sizeof(int));

	xcl_memcpy_to_device(world, devMemInput, input, MEM_SIZE * sizeof(int));
	xcl_memcpy_to_device(world, devMemOutput0, output0, MEM_SIZE * sizeof(int));
	xcl_memcpy_to_device(world, devMemOutput1, output1, MEM_SIZE * sizeof(int));

	int nargs = 0;
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemOutput0);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemOutput1);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &inc);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &burst_num);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &stride);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult0);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult1);

	std::clock_t begin = clock();
	unsigned long duration = xcl_run_kernel3d(world, krnl_mem_test, 1, 1, 1);
	std::clock_t end = clock();

	xcl_memcpy_from_device(world, &result0, devMemResult0, sizeof(int));
	xcl_memcpy_from_device(world, &result1, devMemResult1, sizeof(int));
	xcl_memcpy_from_device(world, output0, devMemOutput0, MEM_SIZE*sizeof(int));
	xcl_memcpy_from_device(world, output1, devMemOutput1, MEM_SIZE*sizeof(int));

	//Verification
	if(result0 == ref_result0 && result1 == ref_result1){
		std::cout << "Read verification passed." << std::endl;
	}
	else{
		std::cout << "Read verfication failed." << std::endl;
	}

	bool pass = true;
	for(int i = 0; i < MEM_SIZE; i++){
		if(output0[i] != ref_output0[i] || output1[i] != ref_output1[i]){
			std::cout << "Write verification failed." << std::endl;
			pass = false;
			break;
		}
	}
	if(pass) std::cout << "Write verification passed." << std::endl;

	double elapsed_time = (end - begin) * 1.0/CLOCKS_PER_SEC;
	elapsed_time = duration * 1.0/1e9;
	double average_latency = elapsed_time * 1e9 /(4.0 * burst_num * burst_len);
	std::cout << "average latency: " << average_latency << "," << std::endl;

	double bandwidth = 4.0 * burst_num * burst_len * sizeof(int)/1024/1024/elapsed_time;
	std::cout << "Measure bandwidth: " << bandwidth << "," << std::endl;

	clReleaseMemObject(devMemInput);
	clReleaseMemObject(devMemOutput0);
	clReleaseMemObject(devMemOutput1);
	clReleaseMemObject(devMemResult0);
	clReleaseMemObject(devMemResult1);
	clReleaseKernel(krnl_mem_test);
	clReleaseProgram(program);
	xcl_release_world(world);

	free(input);
	free(output0);
	free(output1);
	free(ref_output0);
	free(ref_output1);

	return 0;
}
