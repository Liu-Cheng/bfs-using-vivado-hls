#include "xcl2.hpp"
#include <vector>
#include <chrono>

int main(int argc, char* argv[])
{
    std::string bitmapFilename = "mem_test";
    int inc = 10;
    int size = 16 * 1024 * 1024;
    int size_bytes = size * sizeof(int); 
    std::vector<int,aligned_allocator<int>> input(size);
    std::vector<int,aligned_allocator<int>> output(size);

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

    // For Allocating Buffer to specific Global Memory Bank, user has to use cl_mem_ext_ptr_t
    // and provide the Banks
    cl_mem_ext_ptr_t inExt, outExt;  // Declaring two extensions for both buffers
    inExt.flags  = XCL_MEM_DDR_BANK0; // Specify Bank0 Memory for input memory
    outExt.flags = XCL_MEM_DDR_BANK1; // Specify Bank1 Memory for output Memory
    // Setting input and output objects
    inExt.obj = input.data(); 
    outExt.obj = output.data();
    inExt.param = 0 ; outExt.param = 0; 

    //Allocate Buffer in Bank0 of Global Memory for Input Image using Xilinx Extension
    cl::Buffer buffer_inImage(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            size_bytes, &inExt);
    //Allocate Buffer in Bank1 of Global Memory for Input Image using Xilinx Extension
    cl::Buffer buffer_outImage(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            size_bytes, &outExt);

    std::vector<cl::Memory> inBufVec, outBufVec;
    inBufVec.push_back(buffer_inImage);
    outBufVec.push_back(buffer_outImage);

    //Copy input Image to device global memory
    q.enqueueMigrateMemObjects(inBufVec, 0 /* 0 means from host*/); 
	q.finish();
    
	auto begin = std::chrono::high_resolution_clock::now();
    auto krnl_mem_test= cl::KernelFunctor<cl::Buffer&, cl::Buffer& ,int,int>(kernel);
    
    //Launch the Kernel
    krnl_mem_test(cl::EnqueueArgs(q,cl::NDRange(1,1,1), cl::NDRange(1,1,1)), 
            buffer_inImage, buffer_outImage, inc, size);
	q.finish();
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
	auto bandwidth = size * sizeof(int) * 2.0/duration;
	std::cout << "memory access bandwidth: " << bandwidth << " MB/s." << std::endl;


    //Copy Result from Device Global Memory to Host Local Memory
    q.enqueueMigrateMemObjects(outBufVec, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();

    //Compare Golden Image with Output image
    bool match = 0;
	for(int i = 0; i < size; i++){
		if(output[i] != inc){
			match = 1;
			break;
		}
	}
    std::cout << "TEST " << (match ? "FAILED" : "PASSED") << std::endl; 
    return (match ? EXIT_FAILURE :  EXIT_SUCCESS);

}
