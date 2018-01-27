/*
 * Host code for testing the krnlMmult design
 */
#define DEBUG // needs to go before hkuCommon.h

#include <iostream>
#include <cmath> // for log2
#include "cmdlineparser.h"
#include "oclHelper.h"
#include "xcl.h"
#include "clblast_half.h"
#include "hkuCommon.h"
#include "hkuCl.h"


int main(int argc, char **argv)
{
    int ret = 0;
    int M, K, N;
    //Command Line Parser
    sda::utils::CmdLineParser parser;

    parser.addSwitch("--rowsA", "-m", "Number of rows in matrix A",             "16");
    parser.addSwitch("--colsA", "-k", "Number of columns/rows in matrix A/B",   "16");
    parser.addSwitch("--colsB", "-n", "Number of columns in matrix B",          "16");
    parser.parse(argc, argv);
    M = parser.value_to_int("rowsA");
    K = parser.value_to_int("colsA");
    N = parser.value_to_int("colsB");

    cl_int err;

    std::cout << "Try to xcl_world_single" << std::endl;
    xcl_world world = xcl_world_single();
    //clReleaseCommandQueue(world.command_queue);
    //world.command_queue = clCreateCommandQueue(world.context, world.device_id, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);

    std::cout << "Try to xcl_import_binary" << std::endl;
    cl_program program = xcl_import_binary(world, "mmult");
    
    std::cout << "Try to xcl_get_kernel" << std::endl;
    cl_kernel krnlMmult = xcl_get_kernel(program, "mmult");

    std::cout << "Finished xcl_get_kernel" << std::endl;

    size_t upK = RndWrdHalf(K);
    size_t upN = RndWrdHalf(N);
    size_t lastRowA = ((upK-1)/TILE_W + 1) * TILE_W;
    size_t lastRowB = ((upN-1)/TILE_W + 1) * TILE_W;
    size_t bufA_size = ((M-1)*upK + lastRowA) * sizeof(cl_half);
    size_t bufB_size = ((K-1)*upN + lastRowB) * sizeof(cl_half);
    size_t bufC_size = ((M-1)*upN + lastRowB) * sizeof(cl_half);
    std::cout << "Host bufA size is " << bufA_size << std::endl;
    std::cout << "Host bufB size is " << bufB_size << std::endl;
    std::cout << "Host bufC size is " << bufC_size << std::endl;

    cl_half* A = (cl_half*)malloc(bufA_size);
    cl_half* B = (cl_half*)malloc(bufB_size);
    cl_half* C = (cl_half*)malloc(bufC_size);

    std::cout << "Filling host matrix A and B" << std::endl;
    hkuGenHCntMat(M, upK, A);
    hkuGenHIMat(K, upN, B);

    /* ---- begin test 1, A x I == A ---- */
    #define WRITE_TO_DEVICE 1
    #define READ_FROM_DEVICE 1
    cl_mem bufA, bufB, bufC;
    cl_event write_events[2], kernel_events[1], read_events[1];

    #if WRITE_TO_DEVICE == 2
    bufA = clCreateBuffer(world.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, bufA_size, A, &err);
    bufB = clCreateBuffer(world.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, bufB_size, B, &err);
    bufC = clCreateBuffer(world.context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, bufC_size, C, &err);
    #else
    bufA = xcl_malloc(world, CL_MEM_READ_ONLY, bufA_size);
    bufB = xcl_malloc(world, CL_MEM_READ_ONLY, bufB_size);
    bufC = xcl_malloc(world, CL_MEM_WRITE_ONLY, bufC_size);
    #endif

    std::cout << "Migrating bufA and bufB" << std::endl;
    //#if WRITE_TO_DEVICE == 0
    //xcl_memcpy_to_device(world, bufA, A, bufA_size);
    //xcl_memcpy_to_device(world, bufB, B, bufB_size);
    #if WRITE_TO_DEVICE == 1
    OCL_CHECK( clEnqueueWriteBuffer(world.command_queue, bufA, CL_TRUE, 0, bufA_size, A, 0, NULL, &write_events[0]));
    OCL_CHECK( clEnqueueWriteBuffer(world.command_queue, bufB, CL_TRUE, 0, bufB_size, B, 0, NULL, &write_events[1]));
    #elif WRITE_TO_DEVICE == 2
    OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufA, 0 /* flags, 0 means from host*/, 0, NULL, &write_events[0]));
    OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufB, 0 /* flags, 0 means from host*/, 0, NULL, &write_events[1]));
    #endif
    // if host->global write and kernel->global write deadlocks, need wait
    //clFinish(world.command_queue); 

    std::cout << "Launching kernel 1" << std::endl;
    err = hkuHmmult(M, N, K, bufA, bufB, bufC, world.command_queue, krnlMmult, 2, write_events, &kernel_events[0]);

    //#if READ_FROM_DEVICE == 0
    //xcl_memcpy_from_device(world, C, bufC, bufC_size);
    #if READ_FROM_DEVICE == 1
    OCL_CHECK( clEnqueueReadBuffer(world.command_queue, bufC, CL_TRUE, 0, bufC_size, C, 1, &kernel_events[0], &read_events[0]));
    #elif READ_FROM_DEVICE == 2
    cl_event map_events[1];
    OCL_CHECK( clEnqueueMigrateMemObjects(world.command_queue, 1, &bufC, CL_MIGRATE_MEM_OBJECT_HOST, 1, &kernel_events[0], &read_events[0]));
    clEnqueueMapBuffer(world.command_queue, bufC, CL_FALSE, CL_MAP_READ, 0, bufC_size, 1, &read_events[0], &map_events[0], &err);
    #endif

    clFlush(world.command_queue);
    clFinish(world.command_queue);
    /* ---- end test 1, A x I == A ---- */
    #if READ_FROM_DEVICE == 2
    OCL_CHECK( clWaitForEvents(1, &map_events[0]));
    #endif

    unsigned long nsduration;

    double sizeAmb = (double)bufA_size / (1024*1024);
    std::cout << "Write matrix A to global: size = " << sizeAmb << " MBytes" << std::endl;
    nsduration = xcl_get_event_duration(write_events[0]);
    double secWriteA = (double)nsduration / (double)1000000000;
    std::cout << "Write matrix A to global: time = " << secWriteA << " (sec)" << std::endl;
    std::cout << "Write matrix A to global: throughput = " << sizeAmb / secWriteA << " (MB/sec)" << std::endl;

    double sizeBmb = (double)bufB_size / (1024*1024);
    std::cout << "Write matrix B to global: size = " << sizeBmb << " MBytes" << std::endl;
    nsduration = xcl_get_event_duration(write_events[1]);
    double secWriteB = (double)nsduration / (double)1000000000;
    std::cout << "Write matrix B to global: time = " << secWriteB << " (sec)" << std::endl;
    std::cout << "Write matrix B to global: throughput = " << sizeBmb / secWriteB << " (MB/sec)" << std::endl;

    double multOps = (double)M * (double)N * (double)K;
    double addOps = (double)M * (double)N * (double)(K-1);
    std::cout << "kernel 1st execution: mult ops = " << multOps << std::endl;
    std::cout << "kernel 1st execution: add ops = " << addOps << std::endl;

    nsduration = xcl_get_event_duration(kernel_events[0]);
    double secKrnl = (double)nsduration / (double)1000000000;
    std::cout << "kernel 1st execution: time = " << secKrnl << " (sec)" << std::endl;
    std::cout << "kernel 1st execution: compute throughput = " << (multOps + addOps)/(secKrnl*1000*1000) << " (MFLOPS)" << std::endl;

    double sizeCmb = (double)bufC_size / (1024*1024);
    std::cout << "Read matrix C from global: size = " << sizeCmb << " MBytes" << std::endl;
    nsduration = xcl_get_event_duration(read_events[0]);
    double secReadC = (double)nsduration / (double)1000000000;
    std::cout << "Read matrix C from global: time = " << secReadC << " (sec)" << std::endl;
    std::cout << "Read matrix C from global: throughput = " << sizeCmb / secReadC << " (MB/sec)" << std::endl;

    #if READ_FROM_DEVICE == 2
    nsduration = xcl_get_event_duration(map_events[0]);
    double secMapC = ((double)nsduration) / ((double) 1000000000);
    std::cout << "Map matrix C from global: time = " << secMapC << " (sec)" << std::endl;
    #endif

    OCL_CHECK( clReleaseMemObject(bufA));
    OCL_CHECK( clReleaseMemObject(bufB));
    OCL_CHECK( clReleaseMemObject(bufC));

    OCL_CHECK( clReleaseEvent(write_events[0]));
    OCL_CHECK( clReleaseEvent(write_events[1]));
    OCL_CHECK( clReleaseEvent(kernel_events[0]));
    OCL_CHECK( clReleaseEvent(read_events[0]));
    #if READ_FROM_DEVICE == 2
    OCL_CHECK( clReleaseEvent(map_events[0]));
    #endif

    OCL_CHECK( clReleaseKernel(krnlMmult));
    OCL_CHECK( clReleaseProgram(program));
    xcl_release_world(world);
//OPENCL HOST CODE AREA END

    /*
    cl_half* C_sw = (cl_half*)malloc(M * upN * sizeof(cl_half));
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
    free(C_sw);
    */
    free(C);
    free(B);
    free(A);
    return ret;
}
