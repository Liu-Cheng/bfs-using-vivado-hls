/**
 * File              : src/host.cpp
 * Author            : Cheng Liu <st.liucheng@gmail.com>
 * Date              : 12.11.2017
 * Last Modified Date: 12.11.2017
 * Last Modified By  : Cheng Liu <st.liucheng@gmail.com>
 */

#include <iostream>
#include <cstring>

// OpenCL utility layer include
#include "xcl.h"

// CGRA IO parameter
#define INPUT_DATA_SIZE 256    // range: (256, 16384)
#define OUTPUT_DATA_SIZE 256   // range: (256, 16384)
#define INPUT_CTRL_SIZE 1024  // range: (256, 16384)
#define OUTPUT_CTRL_SIZE 1024 // range: (256, 16384)
#define CTRL_WORD_WIDTH 16      // 14 bit offset + 2 bit header

int main(int argc, char** argv){

    //Allocate Memory in Host Memory
    size_t input_size_bytes = sizeof(int) * INPUT_DATA_SIZE;
	size_t output_size_bytes = sizeof(int) * OUTPUT_DATA_SIZE;

    int *source_input = (int *) malloc(input_size_bytes);
    int *output_results  = (int *) malloc(output_size_bytes);
	char16_t *input_ctrl_bytes = (char16_t *)malloc(INPUT_CTRL_SIZE * CTRL_WORD_WIDTH/8);
	char16_t *output_ctrl_bytes = (char16_t *)malloc(OUTPUT_CTRL_SIZE * CTRL_WORD_WIDTH/8);

	int computing_start = 1;

    // Create the test data and Software Result 
    for(int i = 0 ; i < INPUT_DATA_SIZE; i++){
        source_input[i] = i;
    }
    for(int i=0; i < OUTPUT_DATA_SIZE; i++){
        output_results[i] = 0;
    }

	for(int i = 0; i < INPUT_CTRL_SIZE; i++){
		input_ctrl_bytes[i] = 0;
	}

	for(int i = 0; i < OUTPUT_CTRL_SIZE; i++){
		output_ctrl_bytes[i] = 0;
	}

//OPENCL HOST CODE AREA START
    //Create Program and Kernel
    xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "vadd");
    cl_kernel krnl_vadd = xcl_get_kernel(program, "krnl_vadd_rtl");

    //Allocate Buffer in Global Memory
    cl_mem buffer_input = xcl_malloc(world, CL_MEM_READ_ONLY, input_size_bytes);
    cl_mem buffer_output = xcl_malloc(world, CL_MEM_WRITE_ONLY, output_size_bytes);
    cl_mem buffer_input_ctrl = xcl_malloc(world, CL_MEM_READ_ONLY, input_ctrl_bytes);
    cl_mem buffer_output_ctrl = xcl_malloc(world, CL_MEM_READ_ONLY, output_ctrl_bytes);

    //Copy input data to device global memory
    xcl_memcpy_to_device(world, buffer_input, source_input, input_size_bytes);
    xcl_memcpy_to_device(world, buffer_input_ctrl, input_ctrl_bytes, input_ctrl_bytes);
    xcl_memcpy_to_device(world, buffer_output_ctrl, output_ctrl_bytes, output_ctrl_bytes);

    //Set the Kernel Arguments
    xcl_set_kernel_arg(krnl_vadd, 0, sizeof(cl_mem), &buffer_input);
    xcl_set_kernel_arg(krnl_vadd, 1, sizeof(cl_mem), &buffer_output);
    xcl_set_kernel_arg(krnl_vadd, 2, sizeof(cl_mem), &buffer_input_ctrl);
    xcl_set_kernel_arg(krnl_vadd, 3, sizeof(cl_mem), &buffer_output_ctrl);
    xcl_set_kernel_arg(krnl_vadd, 4, sizeof(int), &start);

    //Launch the Kernel
    xcl_run_kernel3d(world,krnl_vadd,1,1,1);

    //Copy Result from Device Global Memory to Host Local Memory
    xcl_memcpy_from_device(world, output_ctrl_bytes, buffer_output_ctrl ,output_size_bytes);

    //Release Device Memories and Kernels
    clReleaseMemObject(buffer_input);
    clReleaseMemObject(buffer_output);
    clReleaseMemObject(buffer_input_ctrl);
    clReleaseMemObject(buffer_output_ctrl);
    clReleaseKernel(krnl_vadd);
    clReleaseProgram(program);
    xcl_release_world(world);
//OPENCL HOST CODE AREA END
    
    // Compare the results of the Device to the simulation
    int match = 0;
    for (int i = 0 ; i < size*2 ; i++){
        if (source_hw_results[i] != source_sw_results[i]){
            std::cout << "Error: Result mismatch" << std::endl;
            std::cout << "i = " << i << " Software result = " << source_sw_results[i]
                << " Device result = " << source_hw_results[i] << std::endl;
            match = 1;
            break;
        }
    }

    // Release Memory from Host Memory
    free(source_input1);
    free(source_input2);
    free(source_hw_results);
    free(source_sw_results);

    if (match){
        std::cout << "TEST FAILED." << std::endl; 
        return EXIT_FAILURE;
    }
    std::cout << "All Device results match CPU results! " << std::endl;
    std::cout << "TEST PASSED." << std::endl; 
    return EXIT_SUCCESS; 
}
