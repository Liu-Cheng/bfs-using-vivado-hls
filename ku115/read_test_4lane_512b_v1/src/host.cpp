/**********
Author: Cheng Liu
Email: st.liucheng@gmail.com
Software: SDx 2016.4
Date: July 6th 2017
**********/

#include "xcl2.hpp"
#include <cstdio>
#include <vector>
#include <ctime>
#include <iostream>

#define MEM_SIZE (64*1024*1024)

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
    std::vector<int,aligned_allocator<int>> input0(MEM_SIZE);
    std::vector<int,aligned_allocator<int>> input1(MEM_SIZE);
    std::vector<int,aligned_allocator<int>> input2(MEM_SIZE);
    std::vector<int,aligned_allocator<int>> input3(MEM_SIZE);

    std::vector<int,aligned_allocator<int>> scattered_result0(MEM_SIZE);
    std::vector<int,aligned_allocator<int>> scattered_result1(MEM_SIZE);
    std::vector<int,aligned_allocator<int>> scattered_result2(MEM_SIZE);
    std::vector<int,aligned_allocator<int>> scattered_result3(MEM_SIZE);

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
		input0[i] = rand()%10;
		input1[i] = input0[i];
		input2[i] = input0[i];
		input3[i] = input0[i];
	}

	//-----------------------------------------------
	// Software data movement
	// ----------------------------------------------
	for(int i = 0; i < burst_num; i++){
		for(int j = 0; j < burst_len; j++){
			ref_result0 += input0[i * stride + j];
			ref_result1 += input1[i * stride + j];
			ref_result2 += input2[i * stride + j];
			ref_result3 += input3[i * stride + j];
		}
	}

	// Data movement using FPGA
	int err = 0;
	std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>(); 

    std::string binaryFile = xcl::find_binary_file(device_name,"mem_test");
    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    devices.resize(1);
    cl::Program program(context, devices, bins);
    cl::Kernel kernel(program,"mem_test");

	cl_mem_ext_ptr_t in_ext0, in_ext1, in_ext2, in_ext3;
	cl_mem_ext_ptr_t out_ext0, out_ext1, out_ext2, out_ext3;

	in_ext0.flags = XCL_MEM_DDR_BANK0; out_ext0.flags = XCL_MEM_DDR_BANK0;
	in_ext1.flags = XCL_MEM_DDR_BANK1; out_ext1.flags = XCL_MEM_DDR_BANK1;
	in_ext2.flags = XCL_MEM_DDR_BANK2; out_ext2.flags = XCL_MEM_DDR_BANK2;
	in_ext3.flags = XCL_MEM_DDR_BANK3; out_ext3.flags = XCL_MEM_DDR_BANK3;

	in_ext0.obj = input0.data(); out_ext0.obj = scattered_result0.data();
	in_ext1.obj = input1.data(); out_ext1.obj = scattered_result1.data();
	in_ext2.obj = input2.data(); out_ext2.obj = scattered_result2.data();
	in_ext3.obj = input3.data(); out_ext3.obj = scattered_result3.data();

	in_ext0.param = 0; out_ext0.param = 0;
	in_ext1.param = 0; out_ext1.param = 0;
	in_ext2.param = 0; out_ext2.param = 0;
	in_ext3.param = 0; out_ext3.param = 0;

	int size_bytes = MEM_SIZE * sizeof(int);
	//Allocate Buffer in Bank0 of Global Memory for Input Image using Xilinx Extension
    cl::Buffer d_input0(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			size_bytes, &in_ext0);
	cl::Buffer d_input1(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			size_bytes, &in_ext1);
	cl::Buffer d_input2(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			size_bytes, &in_ext2);
	cl::Buffer d_input3(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			size_bytes, &in_ext3);

	//Allocate Buffer in Bank1 of Global Memory for Input Image using Xilinx Extension
    cl::Buffer d_result0(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			size_bytes, &out_ext0);
	cl::Buffer d_result1(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			size_bytes, &out_ext1);
	cl::Buffer d_result2(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			size_bytes, &out_ext2);
	cl::Buffer d_result3(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			size_bytes, &out_ext3);

	/*
	cl_mem devMemResult0 = clCreateBuffer(world.context, 
			CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, sizeof(int) * 16, NULL, &err);
	cl_mem devMemResult1 = clCreateBuffer(world.context, 
			CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, sizeof(int) * 16, NULL, &err);
	cl_mem devMemResult2 = clCreateBuffer(world.context, 
			CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, sizeof(int) * 16, NULL, &err);
	cl_mem devMemResult3 = clCreateBuffer(world.context, 
			CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, sizeof(int) * 16, NULL, &err);
    */

    //cl_mem devMemInput0 = xcl_malloc(world, CL_MEM_READ_ONLY, MEM_SIZE * sizeof(int));
    //cl_mem devMemInput1 = xcl_malloc(world, CL_MEM_READ_ONLY, MEM_SIZE * sizeof(int));
    //cl_mem devMemInput2 = xcl_malloc(world, CL_MEM_READ_ONLY, MEM_SIZE * sizeof(int));
    //cl_mem devMemInput3 = xcl_malloc(world, CL_MEM_READ_ONLY, MEM_SIZE * sizeof(int));
	//cl_mem devMemResult0 = xcl_malloc(world, CL_MEM_WRITE_ONLY, 16*sizeof(int));
	//cl_mem devMemResult1 = xcl_malloc(world, CL_MEM_WRITE_ONLY, 16*sizeof(int));
	//cl_mem devMemResult2 = xcl_malloc(world, CL_MEM_WRITE_ONLY, 16*sizeof(int));
	//cl_mem devMemResult3 = xcl_malloc(world, CL_MEM_WRITE_ONLY, 16*sizeof(int));
	std::vector<cl::Memory> inBufVec, outBufVec;
    inBufVec.push_back(d_input0);
    inBufVec.push_back(d_input1);
    inBufVec.push_back(d_input2);
    inBufVec.push_back(d_input3);
    outBufVec.push_back(d_result0);
    outBufVec.push_back(d_result1);
    outBufVec.push_back(d_result2);
    outBufVec.push_back(d_result3);

	q.enqueueMigrateMemObjects(inBufVec, 0 /* 0 means from host*/); 
	q.finish();

	//xcl_memcpy_to_device(world, devMemInput0, input0, MEM_SIZE * sizeof(int));
	//xcl_memcpy_to_device(world, devMemInput1, input1, MEM_SIZE * sizeof(int));
	//xcl_memcpy_to_device(world, devMemInput2, input2, MEM_SIZE * sizeof(int));
	//xcl_memcpy_to_device(world, devMemInput3, input3, MEM_SIZE * sizeof(int));

	/*
	int nargs = 0;
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput0);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput1);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput2);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemInput3);

	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &burst_num);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &burst_len);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(int), &stride);

	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult0);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult1);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult2);
	xcl_set_kernel_arg(krnl_mem_test, nargs++, sizeof(cl_mem), &devMemResult3);

	unsigned long duration_ns = xcl_run_kernel3d(world, krnl_mem_test, 1, 1, 1);
	xcl_memcpy_from_device(world, scattered_result0, devMemResult0, 16 * sizeof(int));
	xcl_memcpy_from_device(world, scattered_result1, devMemResult1, 16 * sizeof(int));
	xcl_memcpy_from_device(world, scattered_result2, devMemResult2, 16 * sizeof(int));
	xcl_memcpy_from_device(world, scattered_result3, devMemResult3, 16 * sizeof(int));
	*/
	auto begin = std::chrono::high_resolution_clock::now();
	auto krnl_mem_test= cl::KernelFunctor<
		cl::Buffer&, 
		cl::Buffer&,
		cl::Buffer&,
		cl::Buffer&,
		cl::Buffer&,
		cl::Buffer&,
		cl::Buffer&,
		cl::Buffer&, 
		int, int, int>(kernel);
    
    //Launch the Kernel
    krnl_mem_test(cl::EnqueueArgs(q,cl::NDRange(1,1,1), cl::NDRange(1,1,1)), 
			d_input0, d_input1, d_input2, d_input3, 
			d_result0, d_result1, d_result2, d_result3, 
			burst_num, burst_len, stride
			);
	q.finish();

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

	//Copy Result from Device Global Memory to Host Local Memory
    q.enqueueMigrateMemObjects(outBufVec, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();

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

	auto bandwidth = 4 * burst_num * burst_len * sizeof(int)/duration;
	std::cout << "memory access bandwidth: " << bandwidth << " MB/s." << std::endl;

	return 0;
}
