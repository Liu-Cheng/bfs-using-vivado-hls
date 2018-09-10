//-----------------------------------------------------------------------------
// Filename: bfs_host.cpp
// Version: 1.0
// Description: Breadth-first search OpenCL benchmark.
//
// Author:      Cheng Liu
// Email:       liucheng@ict.ac.cn, st.liucheng@gmail.com
// Affiliation: Institute of Computing Technology, Chinese Academy of Sciences
//
// Acknowledgement:
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <malloc.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>

#include "graph.h"
#include "opencl_utils.h"

#define HERE do {std::cout << "File: " << __FILE__ << " Line: " << __LINE__ << std::endl;} while(0)

#define ERROR(FMT, ARG...) do {fprintf(stderr,"File=%s, Line=%d  \
        " FMT " \n",__FILE__, __LINE__, ##ARG); exit(-1);} while(0)

#define PRINT(FMT, ARG...) do {fprintf(stdout,"File=%s, Line=%d  \
        " FMT " \n",__FILE__, __LINE__, ##ARG);} while(0)

#define AOCL_ALIGNMENT 64
#define BMAP_DEPTH (64 * 1024) 

template<class T>
struct AlignedArray{
	AlignedArray(size_t numElts){ data = (T*) memalign( AOCL_ALIGNMENT, sizeof(T) * numElts ); }
	~AlignedArray(){ free(data); }

	T& operator[](size_t idx){ return data[idx]; }
	const T& operator[](size_t idx) const { return data[idx]; }

	T* data;
};

Graph* createGraph(const std::string &gName){
	std::string dir = "/home/liucheng/graph-data/";
	std::string fName;

	if     (gName == "dblp")        fName = "dblp.ungraph.txt";
	else if(gName == "youtube")     fName = "youtube.ungraph.txt";
	else if(gName == "lj")          fName = "lj.ungraph.txt";
	else if(gName == "pokec")       fName = "pokec-relationships.txt";
	else if(gName == "wiki-talk")   fName = "wiki-Talk.txt";
	else if(gName == "lj1")         fName = "LiveJournal1.txt";
	else if(gName == "orkut")       fName = "orkut.ungraph.txt";
	else if(gName == "rmat-21-32")  fName = "rmat-21-32.txt";
	else if(gName == "rmat-21-64")  fName = "rmat-19-32.txt";
	else if(gName == "rmat-21-128") fName = "rmat-21-128.txt";
	else if(gName == "twitter")     fName = "twitter_rv.txt";
	else if(gName == "friendster")  fName = "friendster.ungraph.txt";
	else ERROR(" Unknown graph name %s .", gName.c_str());

	std::string fpath = dir + fName;
	return new Graph(fpath.c_str());
}

void swBfsInit(int vertexNum, char* depth, const int &vertexIdx){
	for(int i = 0; i < vertexNum; i++){
		depth[i] = -1;
	}
	depth[vertexIdx] = 0;
}

void swBfs(CSR* csr, char* depth, const int &vertexIdx){
	std::vector<int> frontier;
	char level = 0;
	while(true){
		std::cout << "software bfs: level = " << (int)level << std::endl;
		for(int i = 0; i < csr->vertexNum; i++){
			if(depth[i] == level){
				frontier.push_back(i);
				int start = csr->rpao[2 * i];
				int num   = csr->rpao[2 * i + 1];
				for(int cidx = 0; cidx < num; cidx++){
					int ongb = csr->ciao[start + cidx];
					if(ongb != -1){
						if(depth[ongb] == -1){
							depth[ongb] = level + 1;
						}
					}
				}
			}
		}

		if(frontier.empty()){
			break;
		}
		std::cout << "software bfs: frontier " << frontier.size() << std::endl;

		level++;
		frontier.clear();
	}
}

void hwBfsInit(int vertexNum, char* depth, int rootVidx){
	for(int i = 0; i < vertexNum; i++){
		if(i == rootVidx){
			depth[i] = 0;
		}
		else{
			depth[i] = -1;
		}
	}
}

int verify(char* swDepth, char* hwDepth, const int &num){
	bool match = true;
	for (int i = 0; i < num; i++) {
		if (swDepth[i] != hwDepth[i]) {
			PRINT("swDepth[%d] = %d, hwDepth[%d] = %d\n", i, swDepth[i], i, hwDepth[i]);	
			match = false;
			break;
		} 
	}

	if (match){
		printf("TEST PASSED.\n");
		return EXIT_SUCCESS;
	} 
	else{
		printf("TEST FAILED.\n");
		return -1;
	}
}

int getStartVertexIdx(const std::string &gName){
	if(gName == "youtube")    return 320872;
	if(gName == "lj1")        return 3928512;
	if(gName == "pokec")      return 182045;
	if(gName == "rmat-19-32") return 104802;
	if(gName == "rmat-21-32") return 365723;
	return -1;
}

// Sum the array
int getSum(int *ptr, int num){	
	if(ptr == nullptr) return -1;
	int sum = 0;
	for(int i = 0; i < num; i++){
		sum += ptr[i];
	}
	return sum;
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

int main(int argc, char ** argv){

	spector::ClContext clContext;
	cl_device_type deviceType = CL_DEVICE_TYPE_ACCELERATOR;	
	std::vector<std::string> kernelNames;
	kernelNames.push_back("read_rpa");
	kernelNames.push_back("read_cia");
	kernelNames.push_back("traverse_cia");
	kernelNames.push_back("write_frontier0");
	kernelNames.push_back("write_frontier1");
	kernelNames.push_back("write_frontier2");
	kernelNames.push_back("write_frontier3");
	kernelNames.push_back("write_frontier4");
	kernelNames.push_back("write_frontier5");
	kernelNames.push_back("write_frontier6");
	kernelNames.push_back("write_frontier7");
	const char* clFileName   = "bfs_fpga.cl";
	const char* aocxFileName = "bfs_fpga.aocx";
	if(!init_opencl(&clContext, kernelNames, deviceType, clFileName, aocxFileName)){exit(-1);}

	int         pad   = 8;
	std::string gName = "youtube";
	Graph* gptr = createGraph(gName);
	CSR* csr = new CSR(*gptr, pad);
	free(gptr);

	int vertexNum = csr->vertexNum; 
	int rpaoSize = (int)(csr->rpao.size());
	int ciaoSize = (int)(csr->ciao.size());
	int segSize  = (vertexNum + pad - 1) / pad;
	int rootVidx = getStartVertexIdx(gName);
	std::cout << "Graph is loaded." << std::endl;
	std::cout << "rpaoSize: " << rpaoSize << std::endl;
	std::cout << "ciaoSize: " << ciaoSize << std::endl;

	char* hwDepth = (char*)malloc(vertexNum * sizeof(char));
	char* swDepth = (char*)malloc(vertexNum * sizeof(char));
	AlignedArray<int>  rpaInfo(2 * vertexNum);
	AlignedArray<int>  cia(ciaoSize);
	for(int i = 0; i < ciaoSize; i++){
		cia[i] = csr->ciao[i];
	}
	rpaInfo[0] = csr->rpao[2 * rootVidx];
	rpaInfo[1] = csr->rpao[2 * rootVidx + 1];

	AlignedArray<int> nextFrontier0(segSize);
	AlignedArray<int> nextFrontier1(segSize);
	AlignedArray<int> nextFrontier2(segSize);
	AlignedArray<int> nextFrontier3(segSize);
	AlignedArray<int> nextFrontier4(segSize);
	AlignedArray<int> nextFrontier5(segSize);
	AlignedArray<int> nextFrontier6(segSize);
	AlignedArray<int> nextFrontier7(segSize);
	AlignedArray<int> nextFrontierSize(pad);

	//----------------------
	// software bfs
	//----------------------
	std::cout << "soft bfs starts." << std::endl;
	swBfsInit(vertexNum, swDepth, rootVidx);
	std::clock_t begin = clock();
	swBfs(csr, swDepth, rootVidx);
	std::clock_t end = clock();
	double elapsedTime = (end - begin)*1.0/CLOCKS_PER_SEC;
	std::cout << "Software bfs takes " << elapsedTime << " seconds." << std::endl;

	//--------------------------------
	// hardware accelerated bfs
	//--------------------------------
	hwBfsInit(vertexNum, hwDepth, rootVidx);
	char level       = 0;
	int frontierSize = 1;

	// Note that some of the queues may be reused in different execution steps
	cl_context context             = clContext.context;
	cl_command_queue readRpaQueue  = clContext.queues[0];
	cl_command_queue readCiaQueue  = clContext.queues[1];
	cl_command_queue TravCiaQueue  = clContext.queues[2];
	cl_command_queue wrFrt0Queue   = clContext.queues[3];
	cl_command_queue wrFrt1Queue   = clContext.queues[4];
	cl_command_queue wrFrt2Queue   = clContext.queues[5];
	cl_command_queue wrFrt3Queue   = clContext.queues[6];
	cl_command_queue wrFrt4Queue   = clContext.queues[7];
	cl_command_queue wrFrt5Queue   = clContext.queues[8];
	cl_command_queue wrFrt6Queue   = clContext.queues[9];
	cl_command_queue wrFrt7Queue   = clContext.queues[10];
	cl_kernel* kernel              = clContext.kernels.data();
	cl_device_id deviceId          = clContext.device;
	
	cl_int err;
	cl_mem devRpa   = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int) * 2 * vertexNum, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devCia   = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int) * ciaoSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devNextFrontier0 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * segSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devNextFrontier1 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * segSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devNextFrontier2 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * segSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devNextFrontier3 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * segSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devNextFrontier4 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * segSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devNextFrontier5 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * segSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devNextFrontier6 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * segSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devNextFrontier7 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * segSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devNextFrontierSize = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * pad,  NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devBitmap0          = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * BMAP_DEPTH, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devBitmap1          = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * BMAP_DEPTH, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devBitmap2          = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * BMAP_DEPTH, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devBitmap3          = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * BMAP_DEPTH, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devBitmap4          = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * BMAP_DEPTH, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devBitmap5          = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * BMAP_DEPTH, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devBitmap6          = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * BMAP_DEPTH, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	cl_mem devBitmap7          = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * BMAP_DEPTH, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

	err = clEnqueueWriteBuffer(readRpaQueue, devRpa, CL_TRUE, 0, sizeof(int) * frontierSize * 2, rpaInfo.data, 0, NULL, NULL);
	spector::checkErr(err, "Failed to write memory!");
	err = clEnqueueWriteBuffer(readCiaQueue, devCia, CL_TRUE, 0, sizeof(int) * ciaoSize, cia.data, 0, NULL, NULL);
	spector::checkErr(err, "Failed to write memory!");
	clFinish(readRpaQueue);
	clFinish(readCiaQueue);

	//clSetKernelArg(kernel[0], 0, sizeof(cl_mem), (void*)&devRpa);
	//clSetKernelArg(kernel[0], 1, sizeof(int),    (void*)&frontierSize);

	clSetKernelArg(kernel[1], 0, sizeof(cl_mem), (void*)&devCia);
	//clSetKernelArg(kernel[1], 1, sizeof(int),    (void*)&frontierSize);

	clSetKernelArg(kernel[2], 0, sizeof(cl_mem), (void*)&devNextFrontierSize);
	clSetKernelArg(kernel[2], 1, sizeof(int),    (void*)&rootVidx);
	//clSetKernelArg(kernel[2], 2, sizeof(char),   (void*)&level);

	clSetKernelArg(kernel[3],  0, sizeof(cl_mem), (void*)&devNextFrontier0);
	clSetKernelArg(kernel[4],  0, sizeof(cl_mem), (void*)&devNextFrontier1);
	clSetKernelArg(kernel[5],  0, sizeof(cl_mem), (void*)&devNextFrontier2);
	clSetKernelArg(kernel[6],  0, sizeof(cl_mem), (void*)&devNextFrontier3);
	clSetKernelArg(kernel[7],  0, sizeof(cl_mem), (void*)&devNextFrontier4);
	clSetKernelArg(kernel[8],  0, sizeof(cl_mem), (void*)&devNextFrontier5);
	clSetKernelArg(kernel[9],  0, sizeof(cl_mem), (void*)&devNextFrontier6);
	clSetKernelArg(kernel[10], 0, sizeof(cl_mem), (void*)&devNextFrontier7);

	begin = clock();
	while(frontierSize > 0){
		clSetKernelArg(kernel[0], 0, sizeof(cl_mem), (void*)&devRpa);
		clSetKernelArg(kernel[0], 1, sizeof(int),    (void*)&frontierSize);
		clSetKernelArg(kernel[1], 1, sizeof(int),    (void*)&frontierSize);
		clSetKernelArg(kernel[2], 2, sizeof(char),   (void*)&level);

		err = clEnqueueTask(readRpaQueue, kernel[0],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(readCiaQueue, kernel[1],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(TravCiaQueue, kernel[2],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");

		err = clEnqueueTask(wrFrt0Queue, kernel[3],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(wrFrt1Queue, kernel[4],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(wrFrt2Queue, kernel[5],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(wrFrt3Queue, kernel[6],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(wrFrt4Queue, kernel[7],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(wrFrt5Queue, kernel[8],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(wrFrt6Queue, kernel[9],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(wrFrt7Queue, kernel[10], 0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");

		clFinish(readRpaQueue);
		clFinish(readCiaQueue);
		clFinish(TravCiaQueue);
		clFinish(wrFrt0Queue);
		clFinish(wrFrt1Queue);
		clFinish(wrFrt2Queue);
		clFinish(wrFrt3Queue);
		clFinish(wrFrt4Queue);
		clFinish(wrFrt5Queue);
		clFinish(wrFrt6Queue);
		clFinish(wrFrt7Queue);

		clEnqueueReadBuffer(readRpaQueue, devNextFrontierSize, CL_TRUE, 0, pad * sizeof(int), (void*)(nextFrontierSize.data), 0, NULL, NULL);
		clFinish(readRpaQueue);

		int sum = 0;
		for(int i = 0; i < pad; i++){
			sum += nextFrontierSize[i];
		}
		frontierSize = sum;

		if(nextFrontierSize[0] > 0)
			clEnqueueReadBuffer(wrFrt0Queue, devNextFrontier0, CL_TRUE, 0, sizeof(int) * nextFrontierSize[0], (void*)(nextFrontier0.data), 0, NULL, NULL);
		if(nextFrontierSize[1] > 0)
			clEnqueueReadBuffer(wrFrt1Queue, devNextFrontier1, CL_TRUE, 0, sizeof(int) * nextFrontierSize[1], (void*)(nextFrontier1.data), 0, NULL, NULL);
		if(nextFrontierSize[2] > 0)
			clEnqueueReadBuffer(wrFrt2Queue, devNextFrontier2, CL_TRUE, 0, sizeof(int) * nextFrontierSize[2], (void*)(nextFrontier2.data), 0, NULL, NULL);
		if(nextFrontierSize[3] > 0)
			clEnqueueReadBuffer(wrFrt3Queue, devNextFrontier3, CL_TRUE, 0, sizeof(int) * nextFrontierSize[3], (void*)(nextFrontier3.data), 0, NULL, NULL);
		if(nextFrontierSize[4] > 0)
			clEnqueueReadBuffer(wrFrt4Queue, devNextFrontier4, CL_TRUE, 0, sizeof(int) * nextFrontierSize[4], (void*)(nextFrontier4.data), 0, NULL, NULL);
		if(nextFrontierSize[5] > 0)
			clEnqueueReadBuffer(wrFrt5Queue, devNextFrontier5, CL_TRUE, 0, sizeof(int) * nextFrontierSize[5], (void*)(nextFrontier5.data), 0, NULL, NULL);
		if(nextFrontierSize[6] > 0)
			clEnqueueReadBuffer(wrFrt6Queue, devNextFrontier6, CL_TRUE, 0, sizeof(int) * nextFrontierSize[6], (void*)(nextFrontier6.data), 0, NULL, NULL);
		if(nextFrontierSize[7] > 0)
			clEnqueueReadBuffer(wrFrt7Queue, devNextFrontier7, CL_TRUE, 0, sizeof(int) * nextFrontierSize[7], (void*)(nextFrontier7.data), 0, NULL, NULL);
		if(nextFrontierSize[0] > 0) clFinish(wrFrt0Queue);
		if(nextFrontierSize[1] > 0) clFinish(wrFrt1Queue);
		if(nextFrontierSize[2] > 0) clFinish(wrFrt2Queue);
		if(nextFrontierSize[3] > 0) clFinish(wrFrt3Queue);
		if(nextFrontierSize[4] > 0) clFinish(wrFrt4Queue);
		if(nextFrontierSize[5] > 0) clFinish(wrFrt5Queue);
		if(nextFrontierSize[6] > 0) clFinish(wrFrt6Queue);
		if(nextFrontierSize[7] > 0) clFinish(wrFrt7Queue);

		int id = 0;
		for(int j = 0; j < nextFrontierSize[0]; j++){
			int vidx = nextFrontier0[j];
			hwDepth[vidx] = level + 1;
			rpaInfo[2 * id] = csr->rpao[2 * vidx];
			rpaInfo[2 * id + 1] = csr->rpao[2 * vidx + 1];
			id++;
		}
		for(int j = 0; j < nextFrontierSize[1]; j++){
			int vidx = nextFrontier1[j];
			hwDepth[vidx] = level + 1;
			rpaInfo[2 * id] = csr->rpao[2 * vidx];
			rpaInfo[2 * id + 1] = csr->rpao[2 * vidx + 1];
			id++;
		}
		for(int j = 0; j < nextFrontierSize[2]; j++){
			int vidx = nextFrontier2[j];
			hwDepth[vidx] = level + 1;
			rpaInfo[2 * id] = csr->rpao[2 * vidx];
			rpaInfo[2 * id + 1] = csr->rpao[2 * vidx + 1];
			id++;
		}
		for(int j = 0; j < nextFrontierSize[3]; j++){
			int vidx = nextFrontier3[j];
			hwDepth[vidx] = level + 1;
			rpaInfo[2 * id] = csr->rpao[2 * vidx];
			rpaInfo[2 * id + 1] = csr->rpao[2 * vidx + 1];
			id++;
		}
		for(int j = 0; j < nextFrontierSize[4]; j++){
			int vidx = nextFrontier4[j];
			hwDepth[vidx] = level + 1;
			rpaInfo[2 * id] = csr->rpao[2 * vidx];
			rpaInfo[2 * id + 1] = csr->rpao[2 * vidx + 1];
			id++;
		}
		for(int j = 0; j < nextFrontierSize[5]; j++){
			int vidx = nextFrontier5[j];
			hwDepth[vidx] = level + 1;
			rpaInfo[2 * id] = csr->rpao[2 * vidx];
			rpaInfo[2 * id + 1] = csr->rpao[2 * vidx + 1];
			id++;
		}
		for(int j = 0; j < nextFrontierSize[6]; j++){
			int vidx = nextFrontier6[j];
			hwDepth[vidx] = level + 1;
			rpaInfo[2 * id] = csr->rpao[2 * vidx];
			rpaInfo[2 * id + 1] = csr->rpao[2 * vidx + 1];
			id++;
		}
		for(int j = 0; j < nextFrontierSize[7]; j++){
			int vidx = nextFrontier7[j];
			hwDepth[vidx] = level + 1;
			rpaInfo[2 * id] = csr->rpao[2 * vidx];
			rpaInfo[2 * id + 1] = csr->rpao[2 * vidx + 1];
			id++;
		}

		if(frontierSize > 0){
			clEnqueueWriteBuffer(readRpaQueue, devRpa, CL_TRUE, 0, sizeof(int) * frontierSize * 2, rpaInfo.data, 0, NULL, NULL);
			clFinish(readRpaQueue);
		}
		std::cout << "swBfs iteration: " << (int)(level) << " frontier size: " << frontierSize << std::endl;
		level++;
	}
	end = clock();
	elapsedTime =(end - begin)*1.0/CLOCKS_PER_SEC;
	std::cout << "Hardware bfs time: " << elapsedTime << std::endl;

	verify(swDepth, hwDepth, vertexNum);

	// Cleanup memory
	for (int i = 0; i < clContext.kernels.size(); i++){
		clReleaseKernel(kernel[i]);
	}

	clReleaseProgram(clContext.program);
	clReleaseCommandQueue(readRpaQueue);
	clReleaseCommandQueue(readCiaQueue);
	clReleaseCommandQueue(TravCiaQueue);
	clReleaseCommandQueue(wrFrt0Queue);
	clReleaseCommandQueue(wrFrt1Queue);
	clReleaseCommandQueue(wrFrt2Queue);
	clReleaseCommandQueue(wrFrt3Queue);
	clReleaseCommandQueue(wrFrt4Queue);
	clReleaseCommandQueue(wrFrt5Queue);
	clReleaseCommandQueue(wrFrt6Queue);
	clReleaseCommandQueue(wrFrt7Queue);
	clReleaseContext(context);
	clReleaseMemObject(devRpa);
	clReleaseMemObject(devCia);
	clReleaseMemObject(devNextFrontier0);
	clReleaseMemObject(devNextFrontier1);
	clReleaseMemObject(devNextFrontier2);
	clReleaseMemObject(devNextFrontier3);
	clReleaseMemObject(devNextFrontier4);
	clReleaseMemObject(devNextFrontier5);
	clReleaseMemObject(devNextFrontier6);
	clReleaseMemObject(devNextFrontier7);
	clReleaseMemObject(devNextFrontierSize);

	return 0;
}
