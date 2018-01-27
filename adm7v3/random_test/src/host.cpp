/**********
Copyright (c) 2017, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

/*******************************************************************************
Description: 

    This is a CNN (Convolutional Neural Network) convolutional layer based
    example to showcase the effectiveness of using multiple compute units when
    the base algorithm consists of multiple nested loops with large loop count.    

*******************************************************************************/

#include <iostream>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <ctime>

//OpenCL utility layer include
#include "xcl.h"

size_t offset[1] = {0};
size_t global[1] = {1};
size_t local[1] = {1};

#define MEM_SIZE (256*1024*1024)
#define DATA_SIZE (64*1024*1024) 

inline int get_random_data(int lfsr){
	int bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
	int out = (lfsr >> 1) | (bit << 15);
	return out;
}


int main(int argc, char** argv){

	//std::clock_t begin;
	//std::clock_t end;
	unsigned long duration_ns;
	double elapsed_time;

    int size = DATA_SIZE;
	int inc = 10;

    int *input0 = (int *) malloc(MEM_SIZE * sizeof(int));
    int *output0 = (int *) malloc(MEM_SIZE * sizeof(int)); 
    int *output = (int *) malloc(MEM_SIZE * sizeof(int)); 
	for(int i = 0; i < MEM_SIZE; i++){
		input0[i] = 1;
		output0[i] = 0;
		output[i] = 0;
	}

	int lfsr_rd = 0xACE;
	int lfsr_wr = 0xEAC;
	int sum = 0;
	for(int i = 0; i < size; i++){
		lfsr_rd = get_random_data(lfsr_rd);
		lfsr_wr = get_random_data(lfsr_wr);
		int rd_address = lfsr_rd & 0xFFFFFFF;
		sum += input0[rd_address];
		lfsr_wr = get_random_data(lfsr_wr);
		int wr_address = lfsr_wr & 0xFFFFFFF;
		output[wr_address] = inc + inc;
	}

    xcl_world world = xcl_world_single();
	cl_int err;
	cl_command_queue ooo_queue = clCreateCommandQueue(
			world.context, world.device_id,
			CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 
			&err);

	cl_program program = xcl_import_binary(world, "transfer");
	cl_kernel krnl_tran0 = xcl_get_kernel(program, "tran0");

    // Allocate Buffer in Global Memory
    cl_mem buffer_input0 = xcl_malloc(world, CL_MEM_READ_ONLY, MEM_SIZE * sizeof(int));
    cl_mem buffer_output0 = xcl_malloc(world, CL_MEM_WRITE_ONLY, MEM_SIZE * sizeof(int));

    //Set the Kernel Arguments
    int narg = 0;
    xcl_set_kernel_arg(krnl_tran0, narg++, sizeof(cl_mem), &buffer_input0);
    xcl_set_kernel_arg(krnl_tran0, narg++, sizeof(cl_mem), &buffer_output0);
	xcl_set_kernel_arg(krnl_tran0, narg++, sizeof(int), &size);
    xcl_set_kernel_arg(krnl_tran0, narg++, sizeof(int), &inc);

	//cl_event load_events[2];
	//cl_event ooo_events[2];

	/*
	clEnqueueWriteBuffer(ooo_queue, buffer_input0, CL_FALSE, 0, size * sizeof(int),
		                 input0, 0, nullptr, &load_events[0]);
	*/
    xcl_memcpy_to_device(world, buffer_input0, input0, MEM_SIZE * sizeof(int));

	//cl_event trans_event;
	//begin = clock();
	duration_ns = xcl_run_kernel3d(world, krnl_tran0, 1, 1, 1);
	/*
	clEnqueueNDRangeKernel(ooo_queue, krnl_tran0, 1, offset, global,
                           local, 0, nullptr, &trans_event);
						   */

	/*
	clEnqueueReadBuffer(ooo_queue, buffer_output0, CL_FALSE, 0, size * sizeof(int),
		                output0, 1, &ooo_events[0], nullptr);
	*/

	// wait for the complete of the kernel execution
	clFlush(ooo_queue);
	clFinish(ooo_queue);
	//unsigned long duration = xcl_get_event_duration(trans_event);
	//end = clock();
	std::cout << "duration = " << duration_ns << std::endl;
	elapsed_time = duration_ns * 1.0 /(1000000000.0);

    xcl_memcpy_from_device(world, output0, buffer_output0, MEM_SIZE * sizeof(int));

    std::cout << "simple transfer time: " << elapsed_time << std::endl;
	double bandwidth = 2.0 * size * sizeof(int)/1024/1024/elapsed_time;
	std::cout << "processing throughput: " << bandwidth << "MB/s." << std::endl;

    //Release Device Memories and Kernels
    clReleaseMemObject(buffer_input0);
    clReleaseMemObject(buffer_output0);
    clReleaseKernel(krnl_tran0);
    clReleaseProgram(program);

	clReleaseCommandQueue(ooo_queue);
    xcl_release_world(world);

    // Compare the results of the Device to the simulation
	bool match = true;
    for (int i = 0 ; i < MEM_SIZE; i++){
		if (output[i] != output0[i]){
			std::cout << "i = " << i << " TEST FAILED." << std::endl; 
			match = false;
            break;
        }
    }
	if(match) std::cout << "verfication passed." << std::endl;

    /* Release Memory from Host Memory*/
    free(input0);
    free(output0);

    return 0;
}


