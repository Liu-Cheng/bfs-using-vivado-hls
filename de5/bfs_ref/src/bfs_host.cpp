// ----------------------------------------------------------------------
// Copyright (c) 2016, The Regents of the University of California All
// rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
// 
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
// 
//     * Neither the name of The Regents of the University of California
//       nor the names of its contributors may be used to endorse or
//       promote products derived from this software without specific
//       prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL REGENTS OF THE
// UNIVERSITY OF CALIFORNIA BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
// ----------------------------------------------------------------------
/*
 * Filename: my_bfs.cpp
 * Version: 1.0
 * Description: Breadth-first search OpenCL benchmark.
 * Author: Quentin Gautier
 *
 * Note: This program calls the methods defined in bfs_fpga.cl, which is a
 * derivative work released under the LGPL license provided in the LICENSE file.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>

#include <iostream>
#include <fstream>

#include <chrono>
#include <algorithm>

#include "knobs.h"
#include "graph.h"
#include "opencl_utils.h"


using namespace std;
using namespace spector;

//---------------------------------------------
// Constants
//---------------------------------------------

#define AOCL_ALIGNMENT 64


//---------------------------------------------
// Type definitions
//---------------------------------------------

/// Represents one node in the graph
struct Node
{
	int starting;     //Index where the edges of the node start
	int edge_num;  //The degree of the node
};


/// Scoped array aligned in memory
template<class T>
struct AlignedArray
{
	AlignedArray(size_t numElts){ data = (T*) memalign( AOCL_ALIGNMENT, sizeof(T) * numElts ); }
	~AlignedArray(){ free(data); }

	T& operator[](size_t idx){ return data[idx]; }
	const T& operator[](size_t idx) const { return data[idx]; }

	T* data;
};


//---------------------------------------------
// Functions
//---------------------------------------------

//---------------------------------------------
void bfs_cpu(
		int     node_num,
		Node*   h_graph_nodes,
		int     edge_num,
		int*    h_graph_edges,
		mask_t* h_graph_frontier_mask,
		mask_t* h_graph_updating_mask,
		mask_t* h_graph_visited_mask,
		int*    h_cost_ref)
//---------------------------------------------
{
	mask_t not_over = 1;

	int it_num = 0;
	while(not_over)
	{
		not_over = 0;

		// Kernel 1
		for(int tid = 0; tid < node_num; tid++)
		{
			if (h_graph_frontier_mask[tid] == 1)
			{ 
				h_graph_frontier_mask[tid] = 0;

				int start = h_graph_nodes[tid].starting;
				int end   = h_graph_nodes[tid].edge_num + h_graph_nodes[tid].starting;

				for(int i = start; i < end; i++)
				{
					int id = h_graph_edges[i];

					if(!h_graph_visited_mask[id])
					{
						h_cost_ref[id] = h_cost_ref[tid] + 1;
						h_graph_updating_mask[id] = 1;
					}
				}
			}		
		}

		// Kernel 2
  		for(int tid = 0; tid < node_num; tid++)
		{
			if (h_graph_updating_mask[tid] == 1)
			{
				h_graph_frontier_mask[tid]    = 1;
				h_graph_visited_mask[tid] = 1;

				not_over = 1;

				h_graph_updating_mask[tid] = 0;
			}
		}
		cout << "software bfs iteration: " << it_num << endl;
		it_num++;
	}
}


//---------------------------------------------
void printUsage(const char* argv0)
//---------------------------------------------
{
	cout << "Usage: " << argv0 << " <filename> [platform] [num_runs]" << endl;
}

// Read graph from edge based files
Graph* createGraph(const std::string &gName){
	Graph* gptr;
	if(gName == "dblp"){
		gptr = new Graph("/home/liucheng/graph-data/dblp.ungraph.txt");
	}
	else if(gName == "youtube"){
		gptr = new Graph("/home/liucheng/graph-data/youtube.ungraph.txt");
	}
	else if(gName == "lj"){
		gptr = new Graph("/home/liucheng/graph-data/lj.ungraph.txt");
	}
	else if(gName == "pokec"){
		gptr = new Graph("/home/liucheng/graph-data/pokec-relationships.txt");
	}
	else if(gName == "wiki-talk"){
		gptr = new Graph("/home/liucheng/graph-data/wiki-Talk.txt");
	}
	else if(gName == "lj1"){
		gptr = new Graph("/home/liucheng/graph-data/LiveJournal1.txt");
	}
	else if(gName == "orkut"){
		gptr = new Graph("/home/liucheng/graph-data/orkut.ungraph.txt");
	}
	else if(gName == "rmat-21-32"){
		gptr = new Graph("/home/liucheng/graph-data/rmat-21-32.txt");
	}
	else if(gName == "rmat-19-32"){
		gptr = new Graph("/home/liucheng/graph-data/rmat-19-32.txt");
	}
	else if(gName == "rmat-21-128"){
		gptr = new Graph("/home/liucheng/graph-data/rmat-21-128.txt");
	}
	else if(gName == "twitter"){
		gptr = new Graph("/home/liucheng/graph-data/twitter_rv.txt");
	}
	else if(gName == "friendster"){
		gptr = new Graph("/home/liucheng/graph-data/friendster.ungraph.txt");
	}
	else if(gName == "example"){
		gptr = new Graph("/home/liucheng/graph-data/rmat-1k-10k.txt");
	}
	else{
		std::cout << "Unknown graph name." << std::endl;
		exit(EXIT_FAILURE);
	}

	return gptr;
}

int align(int num, int dataWidth, int alignedWidth){
	if(dataWidth > alignedWidth){
		std::cout << "Aligning to smaller data width is not supported." << std::endl;
		return -1;
	}
	else{
		int wordNum = alignedWidth / dataWidth;
		int alignedNum = ((num - 1)/wordNum + 1) * wordNum;
		return alignedNum;
	}
}


//---------------------------------------------
int main(int argc, char ** argv)
//---------------------------------------------
{
	// *********************
	//  Initialize OpenCL
	// *********************

	ClContext clContext;

	// Default device is CPU
	cl_device_type device_type = CL_DEVICE_TYPE_CPU;
	device_type = CL_DEVICE_TYPE_ACCELERATOR;	

	vector<string> kernel_names;
	kernel_names.push_back("kernel1");
	kernel_names.push_back("kernel2");

	const char* cl_filename   = "bfs_fpga.cl";
	const char* aocx_filename = "bfs_fpga.aocx";

	if(!init_opencl(&clContext, kernel_names, device_type, cl_filename, aocx_filename)){ exit(EXIT_FAILURE); }

	int num_runs = 1;
	if(argc >= 4)
	{
		num_runs = atoi(argv[3]);
	}
	cout << "Running the program " << num_runs << " time" << (num_runs!=1?"s.":".") << endl;

	int source = 365723;
	std::string gName = "youtube";
	if(gName == "youtube")    source = 320872;
	if(gName == "lj1")        source = 3928512;
	if(gName == "pokec")      source = 182045;
	if(gName == "rmat-19-32") source = 104802;
	if(gName == "rmat-21-32") source = 365723;

	Graph* gptr = createGraph(gName);
	CSR* csr = new CSR(*gptr);
	free(gptr);

	std::cout << "Graph is loaded." << std::endl;
	int node_num = align(csr->vertexNum, 8, 8*1024); 
	int edge_num = csr->ciao.size();
	std::cout << "node_num " << csr->vertexNum << " is aligned to " << node_num << std::endl;
	std::cout << "edge_num " << edge_num << std::endl;

	// convert csr data to the user defined format
	AlignedArray<Node>    h_graph_nodes         (node_num);
	AlignedArray<mask_t>  h_graph_frontier_mask (node_num);
	AlignedArray<mask_t>  h_graph_updating_mask (node_num);
	AlignedArray<mask_t>  h_graph_visited_mask  (node_num);
	AlignedArray<int>     h_graph_edges         (edge_num);
	AlignedArray<int>     h_cost                (node_num);
	AlignedArray<int>     h_cost_ref            (node_num);

	// Initialize the memory
	for(unsigned int i = 0; i < node_num; i++){
		h_graph_nodes[i].starting    = csr->rpao[i]; 
		h_graph_nodes[i].edge_num    = csr->rpao[i+1] - csr->rpao[i];
		h_graph_frontier_mask[i]     = 0;
		h_graph_updating_mask[i]     = 0;
		h_graph_visited_mask[i]      = 0;
	}
	for(unsigned int i = 0; i < edge_num; i++){
		h_graph_edges[i] = csr->ciao[i];
	}
	h_graph_frontier_mask[source]    = 1;
	h_graph_visited_mask[source]     = 1;

	for(unsigned int i = 0; i < node_num; i++){
		h_cost[i]     = -1;
		h_cost_ref[i] = -1;
	}
	h_cost[source]     = 0;
	h_cost_ref[source] = 0;

	cl_context context             = clContext.context;
	cl_command_queue command_queue = clContext.queues[0];
	cl_kernel* kernel              = clContext.kernels.data();
	cl_device_id device_id         = clContext.device;

	// Initialize OpenCL buffers
	int err;
	cl_mem d_graph_nodes = clCreateBuffer(context, CL_MEM_READ_ONLY, 
			sizeof(Node) * node_num, NULL, &err);
	ExitError(checkErr(err, "Failed to allocate memory!"));

	cl_mem d_graph_edges = clCreateBuffer(context, CL_MEM_READ_ONLY, 
			sizeof(int) * edge_num, NULL, &err);
	ExitError(checkErr(err, "Failed to allocate memory!"));

	cl_mem d_graph_frontier_mask = clCreateBuffer(context, CL_MEM_READ_WRITE, 
			sizeof(mask_t) * node_num, NULL, &err);
	ExitError(checkErr(err, "Failed to allocate memory!"));

	cl_mem d_graph_updating_mask = clCreateBuffer(context, CL_MEM_READ_WRITE, 
			sizeof(mask_t) * node_num, NULL, &err);
	ExitError(checkErr(err, "Failed to allocate memory!"));

	cl_mem d_graph_visited_mask = clCreateBuffer(context, CL_MEM_READ_WRITE, 
			sizeof(mask_t) * node_num, NULL,  &err);
	ExitError(checkErr(err, "Failed to allocate memory!"));

	// Allocate device memory for result
	cl_mem d_cost = clCreateBuffer(context, CL_MEM_READ_WRITE, 
			sizeof(int) * node_num, NULL, &err);
	ExitError(checkErr(err, "Failed to allocate memory!"));

	// Make a bool to check if the execution is over
	cl_mem d_over = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 
			sizeof(int), NULL, &err);
	ExitError(checkErr(err, "Failed to allocate memory!"));
	

	// Number of work-items
	size_t localWorkSize_1[1] = {KNOB_NUM_WORK_ITEMS_1}; //{no_of_nodes < 256? no_of_nodes: 256};
	size_t workSize_1[1]      = {(node_num/localWorkSize_1[0])*localWorkSize_1[0] + ((node_num%localWorkSize_1[0])==0?0:localWorkSize_1[0])}; // one dimensional Range

	size_t localWorkSize_2[1] = {KNOB_NUM_WORK_ITEMS_2}; //{no_of_nodes < 256? no_of_nodes: 256};
	size_t workSize_2[1]      = {(node_num/localWorkSize_2[0])*localWorkSize_2[0] + ((node_num%localWorkSize_2[0])==0?0:localWorkSize_2[0])}; // one dimensional Range


	cout << "localWorkSize_1 = " << localWorkSize_1[0] << "  workSize_1 = " << workSize_1[0] << endl;
	cout << "localWorkSize_2 = " << localWorkSize_2[0] << "  workSize_2 = " << workSize_2[0] << endl;


	int k = 0;
	
	// Timers
	//
	double totalTime = 0;
	double dryRunTime = 0;

	double kernel_time[clContext.events.size()];
	for(unsigned int i = 0; i < clContext.events.size(); i++){ kernel_time[i] = 0.0; }
	cl_ulong k_time_start, k_time_end;
	double k_temp, k_total_time = 0.0;

	// Run the algorithm multiple times
	//
	for(int irun = 0; irun < num_runs+1; irun++)
	{
		// *** irun == 0: dry run ***
		

		k = 0;
		int stop;

		// Copy data to device memory
		//
		clEnqueueWriteBuffer(command_queue, d_graph_nodes, CL_TRUE, 0, sizeof(Node) * node_num, 
				h_graph_nodes.data, 0, NULL, NULL);
		ExitError(checkErr(err, "Failed to copy data to device!"));
		clEnqueueWriteBuffer(command_queue, d_graph_edges, CL_TRUE, 0, sizeof(int) * edge_num, 
				h_graph_edges.data, 0, NULL, NULL);
		ExitError(checkErr(err, "Failed to copy data to device!"));
		clEnqueueWriteBuffer(command_queue, d_graph_frontier_mask, CL_TRUE, 0, sizeof(mask_t) * node_num, 
				h_graph_frontier_mask.data, 0, NULL, NULL);
		ExitError(checkErr(err, "Failed to copy data to device!"));
		clEnqueueWriteBuffer(command_queue, d_graph_updating_mask, CL_TRUE, 0, 
				sizeof(mask_t) * node_num, h_graph_updating_mask.data, 0, NULL, NULL);
		ExitError(checkErr(err, "Failed to copy data to device!"));
		clEnqueueWriteBuffer(command_queue, d_graph_visited_mask, CL_TRUE, 0, 
				sizeof(mask_t) * node_num, h_graph_visited_mask.data, 0, NULL, NULL);
		ExitError(checkErr(err, "Failed to copy data to device!"));
		clEnqueueWriteBuffer(command_queue, d_cost, CL_TRUE, 0, sizeof(int) * node_num, 
				h_cost.data, 0, NULL, NULL);
		ExitError(checkErr(err, "Failed to copy data to device!"));



		// Set Arguments for kernels 1 and 2
		//
		clSetKernelArg(kernel[0], 0, sizeof(cl_mem), (void*)&d_graph_nodes);
		clSetKernelArg(kernel[0], 1, sizeof(cl_mem), (void*)&d_graph_edges);
		clSetKernelArg(kernel[0], 2, sizeof(cl_mem), (void*)&d_graph_frontier_mask);
		clSetKernelArg(kernel[0], 3, sizeof(cl_mem), (void*)&d_graph_updating_mask);
		clSetKernelArg(kernel[0], 4, sizeof(cl_mem), (void*)&d_graph_visited_mask);
		clSetKernelArg(kernel[0], 5, sizeof(cl_mem), (void*)&d_cost);
		clSetKernelArg(kernel[0], 6, sizeof(unsigned int), (void*)&node_num);

		clSetKernelArg(kernel[1], 0, sizeof(cl_mem), (void*)&d_graph_frontier_mask);
		clSetKernelArg(kernel[1], 1, sizeof(cl_mem), (void*)&d_graph_updating_mask);
		clSetKernelArg(kernel[1], 2, sizeof(cl_mem), (void*)&d_graph_visited_mask);
		clSetKernelArg(kernel[1], 3, sizeof(cl_mem), (void*)&d_over);
		clSetKernelArg(kernel[1], 4, sizeof(unsigned int), (void*)&node_num);



		auto startTime = chrono::high_resolution_clock::now();

		do
		{
			stop = 0;

			// Copy stop to device
			clEnqueueWriteBuffer(command_queue, d_over, CL_TRUE, 0, sizeof(int), (void*)&stop, 0, NULL, NULL);
			ExitError(checkErr(err, "Failed to copy data to device!"));

			// Run kernel[0] and kernel[1]
			cl_int err = clEnqueueNDRangeKernel(
					command_queue, kernel[0], 1, NULL,
					workSize_1, localWorkSize_1, 0, NULL, &clContext.events[0]);
			clFinish(command_queue);
			
			ExitError(checkErr(err, "Failed to execute kernel1!"));

			
			err = clEnqueueNDRangeKernel(command_queue, kernel[1], 1, NULL,
					workSize_2, localWorkSize_2, 0, NULL, &clContext.events[1]);
			clFinish(command_queue);
			
			ExitError(checkErr(err, "Failed to execute kernel2!"));


			// Get kernel execution times
			if(irun != 0)
			{
				for(unsigned int i = 0; i < clContext.events.size(); i++)
				{
					clGetEventProfilingInfo(clContext.events[i], CL_PROFILING_COMMAND_START, 
							sizeof(k_time_start), &k_time_start, NULL);
					clGetEventProfilingInfo(clContext.events[i], CL_PROFILING_COMMAND_END, 
							sizeof(k_time_end), &k_time_end, NULL);
					k_temp = k_time_end - k_time_start;
					kernel_time[i] += (k_temp / 1000000.0);
					//cout << (k_total_time / 1000000.0) << endl;
				}
			}


			// Copy stop from device
			clEnqueueReadBuffer(command_queue, d_over, CL_TRUE, 0, sizeof(int), (void*)&stop, 0, NULL, NULL);
			ExitError(checkErr(err, "Failed to read data from the device!"));
			
			cout << "fpga bfs iteration: " << k << std::endl;
			k++;

		}while(stop == 1);

		auto endTime = chrono::high_resolution_clock::now();

		
		if(irun != 0) // not dry run
		{
			totalTime += chrono::duration <double, milli> (endTime - startTime).count();
		}
		else
		{
			dryRunTime = chrono::duration <double, milli> (endTime - startTime).count();
		}

		cout << flush;
	}
	cout << endl;

	totalTime /= num_runs;
	
	cout << "Kernel executed " << k << " times" << endl;
	cout << "Dry run time: " << dryRunTime << " ms" << endl;
	//cout << "Total time: " << totalTime << " ms" << endl;
	
	for(unsigned int i = 0; i < clContext.events.size(); i++)
	{
		kernel_time[i] /= num_runs;
		k_total_time += kernel_time[i];

		cout << "Kernel " << i+1 << ": " << kernel_time[i] << " ms" << endl;
	}
	cout << "Total time: " << k_total_time << " ms" << endl;





	// Copy result from device to host
	//
	clEnqueueReadBuffer(command_queue, d_cost, CL_TRUE, 0, sizeof(int)*node_num, 
			(void*)h_cost.data, 0, NULL, NULL);
	ExitError(checkErr(err, "Failed to read data from the device!"));



	// Verification step
	//
	for(int i = 0; i < node_num; i++)
	{
		h_graph_frontier_mask[i] = 0;
		h_graph_updating_mask[i] = 0;
		h_graph_visited_mask[i]  = 0;
	}
	h_graph_frontier_mask[source] = 1;
	h_graph_visited_mask[source] = 1;

	bfs_cpu(node_num, h_graph_nodes.data, edge_num, 
			h_graph_edges.data, h_graph_frontier_mask.data, 
			h_graph_updating_mask.data, 
			h_graph_visited_mask.data, h_cost_ref.data);

	bool passed = std::equal(h_cost.data, h_cost.data+node_num, h_cost_ref.data);

	cout << "Verification: " << (passed? "Passed": "Failed") << endl;




	// Store the result into a file
	//
	ofstream output_file("bfs_result.txt");
	if(!output_file.is_open()){ cerr << "Unable to write to \"bfs_result.txt\"" << endl; exit(EXIT_FAILURE); }
	for(unsigned int i = 0; i < node_num; i++)
	{
		output_file << i << ") cost:" << h_cost[i] << "\n";
	}
	cout << "Result stored in bfs_result.txt" << endl;


	// Cleanup memory
	//
	for (unsigned int ki = 0; ki < clContext.kernels.size(); ki++)
   	{
		clReleaseKernel(kernel[ki]);
	}
	clReleaseProgram(clContext.program);
	clReleaseCommandQueue(command_queue);
	clReleaseContext(context);
	clReleaseMemObject(d_graph_nodes);
	clReleaseMemObject(d_graph_edges);
	clReleaseMemObject(d_graph_frontier_mask);
	clReleaseMemObject(d_graph_updating_mask);
	clReleaseMemObject(d_graph_visited_mask);
	clReleaseMemObject(d_cost);
	clReleaseMemObject(d_over);


	return 0;
}
