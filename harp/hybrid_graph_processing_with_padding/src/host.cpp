#include <stdlib.h>
#include <omp.h>
#include <malloc.h>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <cstdio>
#include <math.h>
#include <ctime>
#include "config.h"
#include "graph.h"
#include "safequeue.h"
#include "CL/opencl.h"
#include "AOCLUtils/aocl_utils.h"
#include <tr1/unordered_map>
#include <cmath>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <sys/time.h>

using namespace std::tr1;
using namespace aocl_utils;
using namespace std;
static cl_command_queue queueReadActiveVertices;
static cl_command_queue queueReadNgbInfo;
static cl_command_queue queueProcessEdge;

static cl_kernel readActiveVertices;
static cl_kernel readNgbInfo;
static cl_kernel processEdge;

static cl_program program;

static cl_int status;
static PROP_TYPE* vertexProp;
static PROP_TYPE* tmpVertexProp;
static int* rpa;
static int* blkRpa;
static int* outDeg;
static int* outdeg_padding;
static int* cia;
static int* cia_padding;
static PROP_TYPE* edgeProp;
static int* blkCia;
static PROP_TYPE* blkEdgeProp;
static int* activeVertices;
static int* activeVertexNum;
static int* blkActiveVertices;
static int* blkActiveVertexNum;
static int* itNum;
static int* blkEdgeNum;
static int* blkVertexNum;
static int* eop; // end of processing
static int* srcRange;
static int* sinkRange;

int processing_edges = 0;

static cl_platform_id platform;
static cl_device_id device;
static cl_context context;

static int fpga_partition_x = 0;
static int fpga_partition_y = 0;
static int max_partition_degree = 0; 
static CSR_BLOCK* fpga_partition;

double fpga_runtime =0;
#define AOCL_ALIGNMENT 64
#define THREAD_NUM 1
#define MAX_ITER 1
//#define DEBUG 
#define MODE "harp"
#define PR
#ifdef PR 
#define PROP_TYPE float
#define kDamp 0.85f
#define epsilon  0.001f
#endif
#ifdef CC 
#define PROP_TYPE int
#endif

#define RAND_RANGE(N) ((float)rand() / ((float)RAND_MAX + 1) * (N))

static void freeResources(){
	if(readActiveVertices) clReleaseKernel(readActiveVertices);  
	if(readNgbInfo)        clReleaseKernel(readNgbInfo);  
	if(processEdge)        clReleaseKernel(processEdge);  
	if(program)            clReleaseProgram(program);

	if(queueReadActiveVertices) clReleaseCommandQueue(queueReadActiveVertices);
	if(queueReadNgbInfo)        clReleaseCommandQueue(queueReadNgbInfo);
	if(queueProcessEdge)        clReleaseCommandQueue(queueProcessEdge);

	// We set all the objects to be shared by CPU and FPGA, though
	// some of them are only used by CPU process.
	if(vertexProp)         clSVMFreeAltera(context, vertexProp);
	if(tmpVertexProp)      clSVMFreeAltera(context, tmpVertexProp);
	if(rpa)                clSVMFreeAltera(context, rpa);
	if(blkRpa)             clSVMFreeAltera(context, blkRpa);
	if(outDeg)             clSVMFreeAltera(context, outDeg);
	if(cia)                clSVMFreeAltera(context, cia);
	if(edgeProp)           clSVMFreeAltera(context, edgeProp);
	if(blkCia)             clSVMFreeAltera(context, blkCia);
	if(blkEdgeProp)        clSVMFreeAltera(context, blkEdgeProp);
	if(activeVertices)     clSVMFreeAltera(context, activeVertices);
	if(blkActiveVertices)  clSVMFreeAltera(context, blkActiveVertices);
	if(activeVertexNum)    clSVMFreeAltera(context, activeVertexNum);
	if(blkActiveVertexNum) clSVMFreeAltera(context, blkActiveVertexNum);
	if(itNum)              clSVMFreeAltera(context, itNum);
	if(blkVertexNum)       clSVMFreeAltera(context, blkVertexNum);
	if(blkEdgeNum)         clSVMFreeAltera(context, blkEdgeNum);
	if(eop)                clSVMFreeAltera(context, eop);

	if(context)            clReleaseContext(context);
}

void cleanup(){}

void dumpError(const char *str) {
	printf("Error: %s\n", str);
	freeResources();
}

void checkStatus(const char *str) {
	if(status != 0 || status != CL_SUCCESS){
		dumpError(str);
		printf("Error code: %d\n", status);
	}
}

void kernelVarMap(
		const int &vertexNum, 
		const int &edgeNum
		)
{
	status = clEnqueueSVMMap(queueReadActiveVertices, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)blkActiveVertices, sizeof(int) * vertexNum, 0, NULL, NULL); 
	checkStatus("enqueue activeVertices.");
	status = clEnqueueSVMMap(queueReadActiveVertices, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)blkActiveVertexNum, sizeof(int), 0, NULL, NULL); 
	checkStatus("enqueue activeVertexNum.");

	status = clEnqueueSVMMap(queueReadNgbInfo, CL_TRUE, CL_MAP_READ, 
			(void *)blkRpa, sizeof(int) * (vertexNum + 1), 0, NULL, NULL); 
	checkStatus("enqueue blkRpa.");
	status = clEnqueueSVMMap(queueReadNgbInfo, CL_TRUE, CL_MAP_READ, 
			(void *)blkCia, sizeof(int) * edgeNum, 0, NULL, NULL); 
	checkStatus("enqueue blkCia");
	status = clEnqueueSVMMap(queueReadNgbInfo, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)blkEdgeProp, sizeof(PROP_TYPE) * edgeNum, 0, NULL, NULL); 
	checkStatus("enqueue blkEdgeProp");
	status = clEnqueueSVMMap(queueReadNgbInfo, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)outDeg, sizeof(int) * vertexNum, 0, NULL, NULL); 
	checkStatus("enqueue outDeg");
	status = clEnqueueSVMMap(queueReadNgbInfo, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)blkActiveVertexNum, sizeof(int), 0, NULL, NULL); 
	checkStatus("enqueue blkActiveVertexNum");
	status = clEnqueueSVMMap(queueReadNgbInfo, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)blkVertexNum, sizeof(int), 0, NULL, NULL); 
	checkStatus("enqueue blkVertexNum");
	status = clEnqueueSVMMap(queueReadNgbInfo, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)blkEdgeNum, sizeof(int), 0, NULL, NULL); 
	checkStatus("enqueue blkEdgeNum.");
	status = clEnqueueSVMMap(queueReadNgbInfo, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)srcRange, sizeof(int) * 2, 0, NULL, NULL); 
	checkStatus("enqueue srcRange.");
	status = clEnqueueSVMMap(queueReadNgbInfo, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)itNum, sizeof(int), 0, NULL, NULL); 
	checkStatus("enqueue itNum.");

	// traverse neighbor
	status = clEnqueueSVMMap(queueProcessEdge, CL_TRUE, CL_MAP_READ, 
			(void *)vertexProp, sizeof(int) * vertexNum, 0, NULL, NULL); 
	checkStatus("enqueue vertexProp.");
	status = clEnqueueSVMMap(queueProcessEdge, CL_TRUE, CL_MAP_READ, 
			(void *)tmpVertexProp, sizeof(int) * vertexNum, 0, NULL, NULL); 
	checkStatus("enqueue tmpVertexProp.");
	status = clEnqueueSVMMap(queueProcessEdge, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)eop, sizeof(int), 0, NULL, NULL); 
	checkStatus("enqueue activeVertexNum.");
	status = clEnqueueSVMMap(queueProcessEdge, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)itNum, sizeof(int), 0, NULL, NULL); 
	checkStatus("enqueue semaphore.");
	status = clEnqueueSVMMap(queueProcessEdge, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)srcRange, sizeof(int) * 2, 0, NULL, NULL); 
	checkStatus("enqueue srcRange");
	status = clEnqueueSVMMap(queueProcessEdge, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 
			(void *)sinkRange, sizeof(int) * 2, 0, NULL, NULL); 
	checkStatus("enqueue sinkRange.");
	std::cout << "All the shared memory objects are mapped successfully." << std::endl;
}
void kernelVarUnmap(){
	status = clEnqueueSVMUnmap(queueReadActiveVertices, 
			(void *)blkActiveVertices, 0, NULL, NULL); 
	checkStatus("unmap activeVertices.");
	status = clEnqueueSVMUnmap(queueReadActiveVertices, 
			(void *)blkActiveVertexNum, 0, NULL, NULL); 
	checkStatus("Unmap activeVertexNum.");

	status = clEnqueueSVMUnmap(queueReadNgbInfo, 
			(void *)blkRpa, 0, NULL, NULL); 
	checkStatus("Unmap blkRpa.");
	status = clEnqueueSVMUnmap(queueReadNgbInfo, 
			(void *)blkCia, 0, NULL, NULL); 
	checkStatus("Unmap blkCia.");
	status = clEnqueueSVMUnmap(queueReadNgbInfo, 
			(void *)blkEdgeProp, 0, NULL, NULL); 
	checkStatus("Unmap activeVertexNum.");
	status = clEnqueueSVMUnmap(queueReadNgbInfo, 
			(void *)outDeg, 0, NULL, NULL); 
	checkStatus("Unmap edgeProp.");
	status = clEnqueueSVMUnmap(queueReadNgbInfo, 
			(void *)blkActiveVertexNum, 0, NULL, NULL); 
	checkStatus("Unmap cia.");
	status = clEnqueueSVMUnmap(queueReadNgbInfo, 
			(void *)blkVertexNum, 0, NULL, NULL); 
	checkStatus("Unmap blkVertexNum.");
	status = clEnqueueSVMUnmap(queueReadNgbInfo, 
			(void *)blkEdgeNum, 0, NULL, NULL); 
	checkStatus("Unmap blkEdgeNum.");
	status = clEnqueueSVMUnmap(queueReadNgbInfo, 
			(void *)srcRange, 0, NULL, NULL); 
	checkStatus("Unmap srcRange.");
	status = clEnqueueSVMUnmap(queueReadNgbInfo, 
			(void *)itNum, 0, NULL, NULL); 
	checkStatus("Unmap itNum.");

	status = clEnqueueSVMUnmap(queueProcessEdge, 
			(void *)vertexProp, 0, NULL, NULL); 
	checkStatus("Unmap vertexProp.");
	status = clEnqueueSVMUnmap(queueProcessEdge, 
			(void *)tmpVertexProp, 0, NULL, NULL); 
	checkStatus("Unmap tmpVertexProp.");
	status = clEnqueueSVMUnmap(queueProcessEdge, 
			(void *)eop, 0, NULL, NULL); 
	checkStatus("Unmap eop.");
	status = clEnqueueSVMUnmap(queueProcessEdge, 
			(void *)itNum, 0, NULL, NULL); 
	checkStatus("Unmap itNum.");
	status = clEnqueueSVMUnmap(queueProcessEdge, 
			(void *)srcRange, 0, NULL, NULL); 
	checkStatus("Unmap srcRange.");
	status = clEnqueueSVMUnmap(queueProcessEdge, 
			(void *)sinkRange, 0, NULL, NULL); 
	checkStatus("Unmap sinkRange.");
}

void createKernels(
		const int &vertexNum, 
		const int &edgeNum
		)
{
	std::cout << "Creating graph processing kernels." << std::endl;
	readActiveVertices = clCreateKernel(program, "readActiveVertices", &status);
	checkStatus("Failed clCreateKernel read active vertices.");
	readNgbInfo = clCreateKernel(program, "readNgbInfo", &status);
	checkStatus("Failed clCreateKernel status readNgbInfo.");
	processEdge = clCreateKernel(program, "processEdge", &status);
	checkStatus("Failed clCreateKernel processEdge.");

	clSetKernelArgSVMPointerAltera(readActiveVertices, 0, (void*)blkActiveVertices);	
	clSetKernelArgSVMPointerAltera(readActiveVertices, 1, (void*)blkActiveVertexNum);

	clSetKernelArgSVMPointerAltera(readNgbInfo, 0, (void*)blkRpa);
	clSetKernelArgSVMPointerAltera(readNgbInfo, 1, (void*)cia_padding);
	clSetKernelArgSVMPointerAltera(readNgbInfo, 2, (void*)blkEdgeProp);
	clSetKernelArgSVMPointerAltera(readNgbInfo, 3, (void*)outDeg);	
	clSetKernelArgSVMPointerAltera(readNgbInfo, 4, (void*)blkActiveVertexNum);
	clSetKernelArgSVMPointerAltera(readNgbInfo, 5, (void*)blkVertexNum);
	clSetKernelArgSVMPointerAltera(readNgbInfo, 6, (void*)blkEdgeNum);
	clSetKernelArgSVMPointerAltera(readNgbInfo, 7, (void*)srcRange);
	clSetKernelArgSVMPointerAltera(readNgbInfo, 8, (void*)itNum);

	clSetKernelArgSVMPointerAltera(processEdge, 0, (void*)vertexProp);
	clSetKernelArgSVMPointerAltera(processEdge, 1, (void*)tmpVertexProp);
	clSetKernelArgSVMPointerAltera(processEdge, 2, (void*)eop);
	clSetKernelArgSVMPointerAltera(processEdge, 3, (void*)itNum);
	clSetKernelArgSVMPointerAltera(processEdge, 4, (void*)srcRange);
	clSetKernelArgSVMPointerAltera(processEdge, 5, (void*)sinkRange);
}

void setKernelEnv(){
	queueReadActiveVertices = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
	checkStatus("Failed clCreateCommandQueue of queueReadActiveVertices.");
	queueReadNgbInfo = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
	checkStatus("Failed clCreateCommandQueue of queueReadNgbInfo.");
	queueProcessEdge = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
	checkStatus("Failed clCreateCommandQueue of queueProcessEdge.");

	size_t binSize = 0;
	unsigned char* binaryFile = loadBinaryFile("./graph_fpga.aocx", &binSize);
	if(!binaryFile) dumpError("Failed loadBinaryFile.");

	program = clCreateProgramWithBinary(
			context, 1, &device, &binSize, (const unsigned char**)&binaryFile, 
			&status, &status);
	if(status != CL_SUCCESS) delete [] binaryFile;
	checkStatus("Failed clCreateProgramWithBinary of program.");

	status = clBuildProgram(program, 0, NULL, "", NULL, NULL);
	checkStatus("Failed clBuildProgram.");

	std::cout << "set kernel env." << std::endl;
}
// access FPGA using the main thread 
void setHardwareEnv(){
	cl_uint numPlatforms;
	cl_uint numDevices;
	status = clGetPlatformIDs(1, &platform, &numPlatforms);
	checkStatus("Failed clGetPlatformIDs.");
	printf("Found %d platforms!\n", numPlatforms);

	status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, &numDevices);
	checkStatus("Failed clGetDeviceIDs.");
	printf("Found %d devices!\n", numDevices);

	context = clCreateContext(0, 1, &device, NULL, NULL, &status);
	checkStatus("Failed clCreateContext.");
}


Graph* createGraph(const std::string &gName, const std::string &mode){
	Graph* gptr;
	std::string dir;
	if(mode == "harp") dir = "./";
	else if(mode == "sim") dir = "/data/DATA/liucheng/graph-data/";
	else {
		std::cout << "unknown execution environment." << std::endl;
		exit(0);
	}

	if(gName == "dblp"){
		gptr = new Graph(dir + "dblp.ungraph.txt");
	}
	else if(gName == "youtube"){
		gptr = new Graph(dir + "youtube.ungraph.txt");
	}
	else if(gName == "lj"){
		gptr = new Graph(dir + "lj.ungraph.txt");
	}
	else if(gName == "pokec"){
		gptr = new Graph(dir + "pokec-relationships.txt");
	}
	else if(gName == "wiki-talk"){
		gptr = new Graph(dir + "wiki-Talk.txt");
	}
	else if(gName == "lj1"){
		gptr = new Graph(dir + "LiveJournal1.txt");
	}
	else if(gName == "orkut"){
		gptr = new Graph(dir + "orkut.ungraph.txt");
	}
	else if(gName == "rmat-21-32"){
		gptr = new Graph(dir + "rmat-21-32.txt");
	}
	else if(gName == "rmat-19-32"){
		gptr = new Graph(dir + "rmat-19-32.txt");
	}
	else if(gName == "rmat-21-128"){
		gptr = new Graph(dir + "rmat-21-128.txt");
	}
	else if(gName == "twitter"){
		gptr = new Graph(dir + "twitter_rv.txt");
	}
	else if(gName == "friendster"){
		gptr = new Graph(dir + "friendster.ungraph.txt");
	}
	else if(gName == "example"){
		gptr = new Graph(dir + "rmat-1k-10k.txt");
	}
	else if(gName == "rmat-12-4"){
		gptr = new Graph(dir + "rmat-12-4.txt");
	}
	else{
		std::cout << "Unknown graph name." << std::endl;
		exit(EXIT_FAILURE);
	}

	return gptr;
}

#ifdef PR
typedef float ScoreT;
void swProcessing(
		CSR* csr, PROP_TYPE* swProp,
		const int &source){
	int nodeNum = csr->vertexNum;
	//int MAX_ITER = 10;
	const ScoreT init_score = 1.0f / nodeNum;
	const ScoreT base_score = (1.0f - kDamp) / nodeNum;
	std::vector<float> scores(nodeNum, init_score);
	std::vector<float> tmp_scores(nodeNum, 0);
	std::vector<float> outgoing_contrib(nodeNum);

	for (int iter=0; iter < MAX_ITER; iter++){
			double error = 0;
			for (int n=0; n < nodeNum; n++)
			{	
				tmp_scores[n] = 0;
				if(outDeg[n]!=0)
					outgoing_contrib[n] = scores[n] / outDeg[n];
				else 
					outgoing_contrib[n] = 0;
			}
			for (int u=0; u < nodeNum; u++) {
				int start = rpa[u];
				int num = outDeg[u];
				//printf(" node %d start+num %d\n", u, start + num);
				for(int j = 0; j < num; j++){
					tmp_scores[csr->ciao[start + j]] += outgoing_contrib[u];
					//if (start + j > csr->edgeNum) printf(">= \n");
				}	
			}
			for (int u=0; u < nodeNum; u++) {
				ScoreT old_score = scores[u];
				scores[u] = base_score + kDamp * tmp_scores[u];
				error += fabs(scores[u] - old_score);
			}
			printf(" %2d    %lf\n", iter, error);
			//if (error < epsilon) break;
		}
	}
#endif
#ifdef PR
void ptProcessing(
		CSR* csr,
		std::vector<CSR_BLOCK*> &blkVec, 
		PROP_TYPE* ptProp, 
		const int &blkNum,
		const int &vertexNum,
		const int &source
		)
{
	const ScoreT base_score = (1.0f - kDamp) / vertexNum;
	itNum[0] = 0;
	while(itNum[0] < MAX_ITER){
		//std::cout << "Processing with partition, iteration: " << itNum[0] << std::endl;
		double begin = getCurrentTimestamp();
		struct timeval t1, t2;
		gettimeofday(&t1, NULL);
		for (int u=0; u < vertexNum; u++) {
			int start = rpa[u];
			int num = rpa[u+1] - rpa[u];
			for(int j = 0; j < num; j++){
				if((csr->rpao[u+1] - csr->rpao[u]) != 0)
					tmpVertexProp[cia[start + j]] += vertexProp[u] / (csr->rpao[u+1] - csr->rpao[u]);
			}	
		}
		gettimeofday(&t2, NULL);
		double end = getCurrentTimestamp();
		printf("[INFO]sw pt CSR takes %lf\n", end - begin);
		printf("[INFO]sw pt CSR takes %lf ms\n", ((t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0));
		begin = getCurrentTimestamp();
		for(int row = 0; row < blkNum; row++){
				CSR_BLOCK* blkPtr = blkVec[row];
				int srcStart = blkPtr->srcStart;
				int srcEnd = blkPtr->srcEnd;
				double begin = getCurrentTimestamp();
				for(int v = srcStart; v < srcEnd; v++){
					int ciaIdxStart = blkPtr->rpa[v - srcStart];
					int ciaIdxEnd = blkPtr->rpa[v + 1 - srcStart];
					for(int i = ciaIdxStart; i < ciaIdxEnd; i++){
						int ngbIdx = blkPtr->cia[i]; 
						if((csr->rpao[v+1] - csr->rpao[v]) != 0)
							tmpVertexProp[ngbIdx] += vertexProp[v] / (csr->rpao[v+1] - csr->rpao[v]);
					}
				}
				double end = getCurrentTimestamp();
				//if(row == 1) printf("cpu run time for one partition %lf\n", end - begin);
		} 
		end = getCurrentTimestamp();
		printf("[INFO] the offloaded partition will costs CPU %lf ms\n", (end - begin) * 1000);

		// Decide active vertices and apply
		double error = 0;
		for(int i = 0; i < vertexNum; i++){
			PROP_TYPE tProp = tmpVertexProp[i];
			PROP_TYPE old_score = vertexProp[i];
			vertexProp[i] = base_score + kDamp * tProp;
			error += fabs(vertexProp[i] - old_score);
			tmpVertexProp[i] = 0;
		}
		printf(" %2d    %lf\n", itNum[0], error);
		activeVertexNum[0] = vertexNum;
		itNum[0]++;
	}
}
#endif

class ThreadPool{
	public:
		CSR* csr;
		bool isComplete;
		bool start = false;
		int blkNum;
		std::vector<CSR_BLOCK*>* blkVec;
		//SafeQueue<Task*> taskQueue;
		std::thread fpgaThread;
		std::thread cpuThread[THREAD_NUM];
		int totalTaskNum;
		volatile int cpuTaskCounter;
		volatile int fpgaTaskCounter;
		volatile bool cpuThreadUp = false;

		std::condition_variable cond;
		std::mutex condMutex;
		std::mutex startMutex;
		std::mutex printMutex;
		std::mutex counterMutex;
		std::condition_variable doneCond;
		std::mutex doneCondMutex;

		explicit ThreadPool(
				bool _isComplete
				): isComplete(_isComplete)
		{  
			//fpgaThread = std::thread(&ThreadPool::fpgaThreadFunc, this);
			for(int i = 0; i < THREAD_NUM; i++){
				cpuThread[i]  = std::thread(&ThreadPool::cpuThreadFunc, this);
        		cpu_set_t cpuset;
        		CPU_ZERO(&cpuset);
        		CPU_SET(5, &cpuset);
        		int rc = pthread_setaffinity_np(cpuThread[i].native_handle(),sizeof(cpu_set_t), &cpuset);
        		if (rc) {
        		    cout << "Error:unable to create thread," << rc << endl;
        		    exit(-1);
        		}
			}

      		std::cout << "Main Thread : on CPU " << sched_getcpu() << "\n";
			std::cout << "ThreadPool is created." << std::endl;
		};
		// Create the task container with the pointer of the 2D grid graph partitions
		void init(
				CSR* csr,
				std::vector<CSR_BLOCK*> &blkVec,
				const int &vertexNum,
				const int &edgeNum,
				int &blkNum
				)
		{	
			this->csr = csr;
			this->blkVec = &blkVec;
			this->blkNum = blkNum;
			totalTaskNum = 2;
			cpuTaskCounter = 0;
			fpgaTaskCounter = 0;
			std::cout << "Threadpool is initialized." << std::endl;
		};
		void init_fpga(
				const std::vector<CSR_BLOCK*> &blkVec,
				const int &vertexNum,
				const int &edgeNum,
				const int &blkNum
				)
		{	
			setKernelEnv();
			createKernels(vertexNum, edgeNum);
			kernelVarMap(vertexNum, edgeNum);
		};

		void cpu_run(){
			cond.notify_all();
			//fpgaThreadFunc();
			while((cpuTaskCounter + fpgaTaskCounter) != totalTaskNum);
			//std::cout << "FPGA taskCounter: " << fpgaTaskCounter << std::endl;
			//std::cout << "cpu taskCounter: " << cpuTaskCounter << std::endl;
			fpgaTaskCounter = 0;
			std::lock_guard<std::mutex> counterLock(counterMutex);
			cpuTaskCounter = 0;
		};

		void hybrid_run(){
			isComplete = false;
			//printf("hostisComplete %x\n", isComplete);
			cond.notify_all();

			fpgaThreadFunc();

			//usleep(400);
			while((cpuTaskCounter ) != totalTaskNum - 1);
			std::lock_guard<std::mutex> counterLock(counterMutex);
			cpuTaskCounter = 0;
			fpgaTaskCounter = 0;
		};

		void shutdown(){
			isComplete = true;
			cond.notify_all();
			for(int i = 0; i < THREAD_NUM; i++){
				cpuThread[i].join();
			}
			//std::cout << "End of the paralell processing." << std::endl;
		};

		void cpuThreadFunc(){
     		std::this_thread::sleep_for(std::chrono::milliseconds(800));
      		std::cout << "Thread #" << "newly created" << ": on CPU " << sched_getcpu() << "\n";					
      		//warm - up
   //    		for (int u=0; u < csr->vertexNum; u++) {
			// 			int start = rpa[u];
			// 			int num = rpa[u+1] - rpa[u];;
			// 			for(int j = 0; j < num; j++){
			// 				if((csr->rpao[u+1] - csr->rpao[u]) != 0)
			// 					tmpVertexProp[cia[start + j]] += vertexProp[u] / (csr->rpao[u+1] - csr->rpao[u]);
			// 			}
			// }
			// for(int i = 0; i <  csr->vertexNum; i++){
			// 	tmpVertexProp[i] = 0;
			// }
			cpuThreadUp = true;

			while(true){
				{
					std::unique_lock<std::mutex> condLock(condMutex);
					cond.wait(condLock);

					if(isComplete) break;

					struct timeval t1, t2;
					gettimeofday(&t1, NULL);
					double begin = getCurrentTimestamp();
        #pragma omp parallel
        {
        #pragma omp for
					for (int u=0; u < csr->vertexNum; u++) {
						int start = rpa[u];
						int num = rpa[u+1] - rpa[u];;
       // #pragma omp for
            for(int j = 0; j < num; j++){
							if((csr->rpao[u+1] - csr->rpao[u]) != 0)
								tmpVertexProp[cia[start + j]] += vertexProp[u] / (csr->rpao[u+1] - csr->rpao[u]);
						}
					}
        }
					gettimeofday(&t2, NULL);
					double end = getCurrentTimestamp();
					std::lock_guard<std::mutex> counterLock(counterMutex);
					cpuTaskCounter ++;
					printf("[INFO]hybridProcessing pt CSR takes %lf ms\n", ((t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0));
					printf("[INFO]hybridProcessing CPU runtime %lf\n", end - begin);
				}
			}
		};
		void paddingBlk(){
		// pre-processing to parallel : padding
			int row = fpga_partition_x;
			CSR_BLOCK* blkPtr = (*blkVec)[row];
			int srcStart = blkPtr->srcStart;
			int srcEnd = blkPtr->srcEnd;
			blkVertexNum[0] = blkPtr->vertexNum; 
			blkEdgeNum[0]   = blkPtr->edgeNum;
			srcRange[0]     = srcStart;
			srcRange[1]     = srcEnd;
			sinkRange[0]    = blkPtr->sinkStart;
			sinkRange[1]    = blkPtr->sinkEnd;
			//std::cout << "fpga edgeNum: " << blkEdgeNum[0] << std::endl;
 			//std::cout << "vetexnum  " << blkVertexNum[0] <<std::endl;
 			int idx = 0;
 			for(int i = 0; i < activeVertexNum[0]; i++){
				int v = activeVertices[i];
				if(v >= srcStart && v < srcEnd){
					blkActiveVertices[idx++] = v;
				}
			}
			blkActiveVertexNum[0] = idx;
			for(int i = 0; i <= blkVertexNum[0]; i++){
				blkRpa[i] = blkPtr->rpa[i];
			}
			for(int i = 0; i < blkEdgeNum[0]; i++){
				blkCia[i]   = blkPtr->cia[i];
			}
			#if 1 // csr block partition padding
			int pad = -1;
			int edgesIdx = 0;
			int outEdgesOld = 0;
			int padding_num = 16;
			for(int i = 0; i < blkActiveVertexNum[0]; i++){
				int v = blkActiveVertices[i];
				int start = blkRpa[v - srcStart];
				int num = blkRpa[v - srcStart + 1] - blkRpa[v - srcStart];
		
				std::vector<std::vector<int>> bankVec(padding_num);

				for(int j = 0; j < num; j++){
					int ngbIdx = blkCia[start + j];
					bankVec[ngbIdx % padding_num].push_back(ngbIdx);
				}
				outEdgesOld = edgesIdx;
				blkRpa[i] = edgesIdx;
		
				bool empty_flag = false;
				for(int i = 0; i < padding_num; i ++) empty_flag |= (!bankVec[i].empty());
		
				while (empty_flag){
					for(int i = 0; i < padding_num; i ++){
						if(!bankVec[i].empty()){
							cia_padding[edgesIdx ++] = bankVec[i].back();
							bankVec[i].pop_back();
						}
						else
							cia_padding[edgesIdx ++] = pad;
					}
					empty_flag = false;
					for(int i = 0; i < padding_num; i ++) empty_flag |= (!bankVec[i].empty());
				}
				outdeg_padding[i] = edgesIdx - outEdgesOld;
			}
			#endif
		};

		void fpgaThreadFunc(){
		//FPGA thread CPU version
		#if 0
			for(int row = 0; row < blkNum; row++){
				if (row == fpga_partition_x) continue;
				CSR_BLOCK* blkPtr = (*blkVec)[row];
				int srcStart = blkPtr->srcStart;
				int srcEnd = blkPtr->srcEnd;

				for(int v = srcStart; v < srcEnd; v++){
					int ciaIdxStart = blkPtr->rpa[v - srcStart];
					int ciaIdxEnd = blkPtr->rpa[v + 1 - srcStart];
					for(int i = ciaIdxStart; i < ciaIdxEnd; i++){
						int ngbIdx = blkPtr->cia[i]; 
						//if(v == 134929) printf("ngbIdx %d\n", ngbIdx);
					if((csr->rpao[v+1] - csr->rpao[v]) != 0)
							tmpVertexProp[ngbIdx] += vertexProp[v] / (csr->rpao[v+1] - csr->rpao[v]);
					}
				}
			} 
			fpgaTaskCounter ++;
		#else
			cl_event eventReadActiveVertices;
			cl_event eventReadNgbInfo;
			cl_event eventProcessEdge;
			#if 0
				int row = fpga_partition_x;
				CSR_BLOCK* blkPtr = (*blkVec)[row];
				int srcStart = blkPtr->srcStart;
				int srcEnd = blkPtr->srcEnd;

				for(int i = 0; i < blkActiveVertexNum[0]; i++){
					int v = blkActiveVertices[i];
  		           for(int k = blkRpa[v - srcStart]; k < blkRpa[v - srcStart + 1]; k++){
  		             int ngbIdx = cia_padding[k];
  		             	if(ngbIdx != -1){
  		             		if(outDeg[v] != 0)
  		               			tmpVertexProp[ngbIdx] += vertexProp[v] / outDeg[v]; //compute(uProp, 1, tmpVertexProp[     ngbIdx]);
  		           		}
  		       		}
  		        }
			#else
				double begin = getCurrentTimestamp();
				status = clEnqueueTask(queueReadActiveVertices, 
						readActiveVertices, 0, NULL, &eventReadActiveVertices);
				checkStatus("Failed to launch readActiveVertices.");
				status = clEnqueueTask(queueReadNgbInfo, readNgbInfo, 0, NULL, &eventReadNgbInfo);
				checkStatus("Failed to launch readNgbInfo.");
				status = clEnqueueTask(queueProcessEdge, processEdge, 0, NULL, &eventProcessEdge);
				checkStatus("Failed to launch processEdge.");

				clFinish(queueReadActiveVertices);
				clFinish(queueReadNgbInfo);
				clFinish(queueProcessEdge);

				double end = getCurrentTimestamp();
			  	fpga_runtime = (end - begin);
			 	printf("fpga run time for one partition %lf\n", fpga_runtime);
			#endif
			
				for(int row = 0; row < blkNum; row++){
					if (row == fpga_partition_x) continue;
					CSR_BLOCK* blkPtr = (*blkVec)[row];
					int srcStart = blkPtr->srcStart;
					int srcEnd = blkPtr->srcEnd;
	
					for(int v = srcStart; v < srcEnd; v++){
						int ciaIdxStart = blkPtr->rpa[v - srcStart];
						int ciaIdxEnd = blkPtr->rpa[v + 1 - srcStart];
						for(int i = ciaIdxStart; i < ciaIdxEnd; i++){
							int ngbIdx = blkPtr->cia[i]; 
						if((csr->rpao[v+1] - csr->rpao[v]) != 0)
								tmpVertexProp[ngbIdx] += vertexProp[v] / (csr->rpao[v+1] - csr->rpao[v]);
						}
					}
				} 
				fpgaTaskCounter ++;
		#endif
		};
};

double hybridProcessing(
		CSR* csr,
		std::vector<CSR_BLOCK*> &blkVec, 
		PROP_TYPE* hybridProp, 
		int &blkNum,
		const int &vertexNum,
		const int &edgeNum,
		const int platSeclct // 1 is CPU, 2 is CPU and FPGA coprocess
		)
{	
	itNum[0] = 0;
	ThreadPool* tp = new ThreadPool(false);
	tp->init(csr, blkVec, vertexNum, edgeNum, blkNum); 

	if(platSeclct == 2) tp->init_fpga( blkVec, vertexNum, edgeNum, blkNum);
	tp->paddingBlk();

	while(!tp->cpuThreadUp);
	printf("CPU thread up\n");
	//printf("ck 1\n");
	const double begin = getCurrentTimestamp();
	const ScoreT base_score = (1.0f - kDamp) / vertexNum;
	while(itNum[0] < MAX_ITER){	
	#ifdef DEBUG
		std::cout << "Hybrid processing with partition, iteration: " << itNum[0];
		std::cout << " active verterx number: " << activeVertexNum[0] << std::endl;
	#endif
		
		if( platSeclct == 1)	
			tp->cpu_run();
		else
			tp->hybrid_run();

		double error = 0;
		for(int i = 0; i < vertexNum; i++){
			PROP_TYPE tProp = tmpVertexProp[i];
			PROP_TYPE old_score = vertexProp[i];
			vertexProp[i] = base_score + kDamp * tProp;
			error += fabs(vertexProp[i] - old_score);
			tmpVertexProp[i] = 0;
		}
		printf(" %2d    %lf\n", itNum[0], error);
    //activeVertexNum[0] = vertexNum;
		itNum[0]++;
	}
	
	const double end = getCurrentTimestamp();
	tp->shutdown();
	for(int i = 0; i < vertexNum; i++){
		hybridProp[i] = vertexProp[i];
	}
	return (end - begin);//*1.0/CLOCKS_PER_SEC;
}


// Init the global variables and they remain the 
// same during the processing.
void globalVarInit(
		CSR* csr, 
		const int &vertexNum, 
		const int &edgeNum, 
		const int &source
		)
{
	vertexProp         = (PROP_TYPE*) clSVMAllocAltera(context, 0, sizeof(PROP_TYPE) * vertexNum, 1024); 
	tmpVertexProp      = (PROP_TYPE*) clSVMAllocAltera(context, 0, sizeof(PROP_TYPE) * vertexNum, 1024);
	rpa                = (int*) clSVMAllocAltera(context, 0, sizeof(int) * (vertexNum + 1), 1024);
	blkRpa             = (int*) clSVMAllocAltera(context, 0, sizeof(int) * (vertexNum + 1), 1024);
	outDeg             = (int*) clSVMAllocAltera(context, 0, sizeof(int) * vertexNum, 1024);
	cia           	   = (int*) clSVMAllocAltera(context, 0, sizeof(int) * edgeNum * 2, 1024);
	cia_padding    	   = (int*) clSVMAllocAltera(context, 0, sizeof(int) * edgeNum * 5, 1024);
	edgeProp           = (PROP_TYPE*) clSVMAllocAltera(context, 0, sizeof(PROP_TYPE) * edgeNum, 1024);
	blkCia             = (int*) clSVMAllocAltera(context, 0, sizeof(int) * edgeNum, 1024);	
	outdeg_padding     = (int*) clSVMAllocAltera(context, 0, sizeof(int) * vertexNum, 1024);
	blkEdgeProp        = (PROP_TYPE*) clSVMAllocAltera(context, 0, sizeof(PROP_TYPE) * edgeNum, 1024);
	activeVertices     = (int*) clSVMAllocAltera(context, 0, sizeof(int) * vertexNum, 1024);
	activeVertexNum    = (int*) clSVMAllocAltera(context, 0, sizeof(int), 1024);
	blkActiveVertices  = (int*) clSVMAllocAltera(context, 0, sizeof(int) * vertexNum, 1024);
	blkActiveVertexNum = (int*) clSVMAllocAltera(context, 0, sizeof(int), 1024);
	itNum     		   = (int*) clSVMAllocAltera(context, 0, sizeof(int), 1024); 
	blkEdgeNum     	   = (int*) clSVMAllocAltera(context, 0, sizeof(int), 1024); 
	blkVertexNum 	   = (int*) clSVMAllocAltera(context, 0, sizeof(int), 1024); 
	eop  		       = (int*) clSVMAllocAltera(context, 0, sizeof(int), 1024); 
	srcRange 	   	   = (int*) clSVMAllocAltera(context, 0, sizeof(int) * 2, 1024);  
	sinkRange 	   	   = (int*) clSVMAllocAltera(context, 0, sizeof(int) * 2, 1024);  

	if(!vertexProp || !tmpVertexProp || !rpa || !blkRpa 
	   || !outDeg || !cia || !edgeProp || !blkCia 
	   || !blkEdgeProp || !activeVertices|| !activeVertexNum 
	   || !blkActiveVertices || !blkActiveVertexNum 
	   || !itNum || !blkEdgeNum || !blkVertexNum || !eop 
	   || !srcRange || !sinkRange
	  ) 
	{
		dumpError("Failed to allocate buffers.");
	}

	for(int i = 0; i < vertexNum; i++){
		if(i < csr->vertexNum){ // 'vertexNum' may be aligned.	
			rpa[i] = csr->rpao[i];
			outDeg[i] = csr->rpao[i + 1] - csr->rpao[i];
		}
		else{
			rpa[i] = 0;
			outDeg[i] = 0;
		}
	}
	rpa[vertexNum] = csr->rpao[vertexNum]; 

	for(int i = 0; i < edgeNum; i++){
		cia[i] = csr->ciao[i];
		edgeProp[i] = rand()%100;
	}
}

void padding (CSR* csr, const int &nodeNum, const int &edgeNum, int pading_num){
// pre-processing to parallel : padding
	int pad = -1;
	int edgesIdx = 0;
	int outEdgesOld = 0;
	for(int i = 0; i < nodeNum; i++){
		int start = rpa[i];
		int num = outDeg[i];

		std::vector<std::vector<int>> bankVec(pading_num);

		for(int j = 0; j < num; j++){
			int ngbIdx = csr->ciao[start + j];
			bankVec[ngbIdx % pading_num].push_back(ngbIdx);
		}
		outEdgesOld = edgesIdx;
		rpa[i] = edgesIdx;

		bool empty_flag = false;
		for(int i = 0; i < pading_num; i ++) empty_flag |= (!bankVec[i].empty());

		while (empty_flag){
			for(int i = 0; i < pading_num; i ++){
				if(!bankVec[i].empty()){
					cia_padding[edgesIdx ++] = bankVec[i].back();
					bankVec[i].pop_back();
				}
				else
					cia_padding[edgesIdx ++] = pad;
			}

			empty_flag = false;
			for(int i = 0; i < pading_num; i ++) empty_flag |= (!bankVec[i].empty());
		}
		outdeg_padding[i] = edgesIdx - outEdgesOld;
	}
}
// Init the variables for a new processing.
void processInit(
		const int &vertexNum,
		const int &edgeNum,
		const int &source
		)
{
	eop[0] = 0;

#ifdef PR
	for(int i = 0; i < vertexNum; i++){
		vertexProp[i] = 1.0f / vertexNum;
		tmpVertexProp[i] = 0;
		activeVertices[i] = i;
	}
	activeVertexNum[0] = vertexNum;
	//activeVertexNum[0] = 0;
#endif

#ifdef BFS
	for(int i = 0; i < vertexNum; i++){
		vertexProp[i]    = MAX_PROP;
		tmpVertexProp[i] = MAX_PROP;
	}
	vertexProp[source] = 0;
	tmpVertexProp[source] = 0;
	activeVertexNum[0] = 1;
	activeVertices[0] = source;
#endif

}

int verify(
		PROP_TYPE* refProp, 
		PROP_TYPE* resultProp, 
		const int &num)
{	
	return 1;
}

void csrReorganize(
		CSR* csr,
		std::vector<CSR_BLOCK*> &blkVec,
		const int &blkNum){
	// remove the FPGA partition from CSR
	CSR_BLOCK* blkPtr = blkVec[0];
	int k = 0;
	rpa[0] = 0;
	for(int i = 0; i < csr->vertexNum; i++){
	   	for(int j = csr->rpao[i]; j < csr->rpao[i + 1]; j ++ ){
	   		if((csr->ciao[j] < blkPtr -> sinkStart) ||  (csr->ciao[j] > blkPtr -> sinkEnd))
	   			cia[k ++] = csr->ciao[j];
	   	}
		rpa[i + 1] = k;
	}
}
void csrPartition(
		CSR* csr,
		std::vector<CSR_BLOCK*> &blkVec,
		const int &blkNum
		)
{
	std::cout << "The graph is divided into " << blkNum * blkNum << " partitions\n";
	for(int cordx = 0; cordx < blkNum; cordx++){
		for(int cordy = 0; cordy < blkNum; cordy++){
			CSR_BLOCK* csrBlkPtr = new CSR_BLOCK(cordx, cordy, csr);
			// find the partition with max degree
			if(csrBlkPtr->edgeNum > max_partition_degree){
				max_partition_degree = csrBlkPtr->edgeNum;
				fpga_partition_x = cordx;
				fpga_partition_y = cordy;
				fpga_partition = csrBlkPtr;
				printf("fpga_partition_x %d, fpga_partition_y %d, max_partition_degree %d \n",fpga_partition_x, fpga_partition_y,max_partition_degree);
			}
			//printf("cordx %d, cordy %d, degree %d \n",cordx, cordy,csrBlkPtr->edgeNum);
		}
	}

	for(int cordx = 0; cordx < blkNum; cordx++){
		CSR_BLOCK* csrBlkPtr = new CSR_BLOCK(cordx, fpga_partition_y, csr);
		printf("degree of %d row is %d\n", cordx, csrBlkPtr->edgeNum);
		blkVec.push_back(csrBlkPtr);
	}
}

int main(int argc, char **argv) {
	double begin;
	double end;
	double elapsedTime;

	int startVertexIdx;
	std::string gName = "youtube";
	std::string mode = "harp"; // or harp

	if(gName == "youtube")    startVertexIdx = 320872;
	if(gName == "lj1")        startVertexIdx = 3928512;
	if(gName == "pokec")      startVertexIdx = 182045;
	if(gName == "rmat-19-32") startVertexIdx = 104802;
	if(gName == "rmat-21-32") startVertexIdx = 365723;
	Graph* gptr = createGraph(gName, mode);
	CSR* csr = new CSR(*gptr);
	int vertexNum = csr->vertexNum;
	int edgeNum   = csr->edgeNum;
	free(gptr);

	// Results of software based processing, partitioned software processing 
	// and hybrid processing.
	PROP_TYPE *swProp      = (PROP_TYPE*)malloc(vertexNum * sizeof(PROP_TYPE));
	PROP_TYPE *ptProp      = (PROP_TYPE*)malloc(vertexNum * sizeof(PROP_TYPE));
	PROP_TYPE *hybridProp  = (PROP_TYPE*)malloc(vertexNum * sizeof(PROP_TYPE));

	// Partition the CSR
	int blkNum = (vertexNum + BLK_SIZE - 1)/BLK_SIZE;
	std::vector<CSR_BLOCK*> blkVec;
	

	// Global variables initialization
	setHardwareEnv();
	globalVarInit(csr, vertexNum, edgeNum, startVertexIdx);

	// soft processing on CPU
	std::cout << "soft PageRank starts." << std::endl;
	processInit(vertexNum, edgeNum, startVertexIdx);
	begin = getCurrentTimestamp();
	swProcessing(csr, swProp, startVertexIdx);
	end = getCurrentTimestamp();
	elapsedTime = (end - begin);
	std::cout << "[INFO] Software PR takes " << elapsedTime << " seconds." << std::endl;
	
	// software processing with only 2 partitions
	csrPartition(csr, blkVec, blkNum);
	csrReorganize(csr, blkVec, blkNum);
	std::cout << "soft PageRank starts." << std::endl;
	processInit(vertexNum, edgeNum, startVertexIdx);
	begin = getCurrentTimestamp();
	ptProcessing(csr, blkVec, ptProp, blkNum, vertexNum, startVertexIdx);
	end = getCurrentTimestamp();
	elapsedTime = (end - begin);
	std::cout << "[INFO] Software PR takes " << elapsedTime << " seconds." << std::endl;


	std::cout << "Hybrid multi-thread processing." << std::endl;
	processInit(vertexNum, edgeNum, startVertexIdx);
	//padding(csr, vertexNum, edgeNum, 16);	
	elapsedTime = hybridProcessing(csr, blkVec, hybridProp, blkNum, vertexNum, edgeNum, 2);
	std::cout << "[INFO] Hybird BFS with partition takes " << elapsedTime << " seconds." << std::endl;
	std::cout << "Verify hybrid BFS with partition: " << std::endl;									
	verify(swProp, hybridProp, vertexNum);
	freeResources();
/*
	std::cout << "soft PageRank with partition starts." << std::endl;
	processInit(vertexNum, edgeNum, startVertexIdx);
	begin = getCurrentTimestamp();
	
	end = getCurrentTimestamp();
	elapsedTime = (end - begin);
	std::cout << "[INFO] Software PageRank with partition takes " << elapsedTime << " seconds." << std::endl;
	std::cout << "Verify PR with partition: " << std::endl;									
	verify(swProp, ptProp, vertexNum);

	std::cout << "CPU multi-thread processing." << std::endl;
	processInit(vertexNum, edgeNum, startVertexIdx);
	elapsedTime = hybridProcessing(blkVec, hybridProp, blkNum, vertexNum, edgeNum, 1);
	std::cout << "[INFO] CPU PR with partition and multi-thread takes " << elapsedTime << " seconds." << std::endl;
	std::cout << "Verify CPU PR with partition: " << std::endl;							
	verify(swProp, hybridProp, vertexNum);


	std::cout << "Hybrid multi-thread processing." << std::endl;
	processInit(vertexNum, edgeNum, startVertexIdx);
	//padding(csr, vertexNum, edgeNum, 16);	
	elapsedTime = hybridProcessing(blkVec, hybridProp, blkNum, vertexNum, edgeNum, 2);
	std::cout << "[INFO] Hybird BFS with partition takes " << elapsedTime << " seconds." << std::endl;
	std::cout << "Verify hybrid BFS with partition: " << std::endl;									
	verify(swProp, hybridProp, vertexNum);
	freeResources();
*/
	return 0;
}
