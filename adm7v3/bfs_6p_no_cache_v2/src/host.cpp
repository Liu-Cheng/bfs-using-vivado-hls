/**********
Author: Cheng Liu
Email: st.liucheng@gmail.com
Software: SDx 2016.4
Date: July 6th 2017
**********/

#include "xcl.h"
#include "graph.h"

#include <cstdio>
#include <vector>
#include <ctime>

static const char *error_message =
    "Error: Result mismatch:\n"
    "i = %d CPU result = %d Device result = %d\n";

#define OCL_CHECK(call)                                                        \
	do {                                                                       \
		cl_int err = call;                                                     \
		if (err != CL_SUCCESS) {                                               \
			printf("Error calling " #call ", error: %s\n", oclErrorCode(err)); \
			exit(EXIT_FAILURE);                                                \
		}                                                                      \
	} while (0);

Graph* createGraph(const std::string &gName){
	Graph* gptr;
	//std::string home = "/data/graph-data/";
	std::string home = "/home/liucheng/gitrepo/graph-data/";
	std::string gpath;
	if(gName == "dblp"){
		gpath = home + "dblp.ungraph.txt";
	}
	else if(gName == "youtube"){
		gpath = home + "youtube.ungraph.txt";
	}
	else if(gName == "lj"){
		gpath = home + "lj.ungraph.txt";
	}
	else if(gName == "pokec"){
		gpath = home + "pokec-relationships.txt";
	}
	else if(gName == "wiki-talk"){
		gpath = home + "wiki-Talk.txt";
	}
	else if(gName == "lj1"){
		gpath = home + "LiveJournal1.txt";
	}
	else if(gName == "orkut"){
		gpath = home + "orkut.ungraph.txt";
	}
	else if(gName == "rmat-21-32"){
		gpath = home + "rmat-21-32.txt";
	}
	else if(gName == "rmat-19-32"){
		gpath = home + "rmat-19-32.txt";
	}
	else if(gName == "rmat-21-128"){
		gpath = home + "rmat-21-128.txt";
	}
	else if(gName == "twitter"){
		gpath = home + "twitter_rv.txt";
	}
	else if(gName == "friendster"){
		gpath = home + "friendster.ungraph.txt";
	}
	else if(gName == "example"){
		gpath = home + "rmat-1k-10k.txt";
	}
	else{
		std::cout << "Unknown graph name." << std::endl;
		exit(EXIT_FAILURE);
	}

	gptr = new Graph(gpath);
	return gptr;
}



void swBfsInit(int vertexNum, char* depth, const int &vertexIdx){
	for(int i = 0; i < vertexNum; i++){
		depth[i] = -1;
	}
	depth[vertexIdx] = 0;
}

void bfsIt(CSR* csr, char* depth, char level, int *eoBfs){
	int counter = 0;
	for(int i = 0; i < csr->vertexNum; i++){
		if(depth[i] == level){
			counter++;
			for(int cidx = csr->rpao[i]; cidx < csr->rpao[i+1]; cidx++){
				int ongb = csr->ciao[cidx];
				if(depth[ongb] == -1){
					depth[ongb] = level + 1;
				}
			}
		}
	}
	if(counter == 0) *eoBfs = 1;
	else *eoBfs = 0;
}

void swBfs(CSR* csr, char* depth, const int &vertexIdx){
	std::vector<int> frontier;
	char level = 0;
	while(true){
		for(int i = 0; i < csr->vertexNum; i++){
			if(depth[i] == level){
				frontier.push_back(i);
				for(int cidx = csr->rpao[i]; cidx < csr->rpao[i+1]; cidx++){
					int ongb = csr->ciao[cidx];
					if(depth[ongb] == -1){
						depth[ongb] = level + 1;
					}
				}
			}
		}

		if(frontier.empty()){
			break;
		}

		level++;
		frontier.clear();
	}
}

void hwBfsInit(int vertexNum, char* depth, int startVertexIdx){
	for(int i = 0; i < vertexNum; i++){
		if(i == startVertexIdx){
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
			printf(error_message, i, swDepth[i], hwDepth[i]);	
			match = false;
			break;
		} 
	}

	if (match) {
		printf("TEST PASSED.\n");
		return EXIT_SUCCESS;
	} else {
		printf("TEST FAILED.\n");
		return EXIT_FAILURE;
	}
}

int getTotalHubVertexNum(CSR* csr, int degreeThreshold){
	int outHubVertexNum = 0;
	int inHubVertexNum = 0;
	for(size_t i = 1; i < csr->rpao.size(); i++){
		if(csr->rpao[i] - csr->rpao[i - 1] > degreeThreshold){
			outHubVertexNum++;
		}
	}

	for(size_t i = 1; i < csr->rpai.size(); i++){
		if(csr->rpai[i] - csr->rpai[i - 1] > degreeThreshold){
			inHubVertexNum++;
		}
	}

	std::cout << "HubVertexNum(out): " << outHubVertexNum << std::endl;
	std::cout << "HubVertexNum(in): " << inHubVertexNum << std::endl;

	return outHubVertexNum;
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

int main(int argc, char **argv) {
	std::clock_t begin;
	std::clock_t end;
	double elapsedTime;

	int startVertexIdx = 365723;
	std::string gName = "youtube";
	if(gName == "youtube") startVertexIdx = 320872;
	if(gName == "lj1") startVertexIdx = 3928512;
	if(gName == "pokec") startVertexIdx = 182045;
	if(gName == "rmat-19-32") startVertexIdx = 104802;
	if(gName == "rmat-21-32") startVertexIdx = 365723;
	Graph* gptr = createGraph(gName);
	CSR* csr = new CSR(*gptr);
	free(gptr);
	std::cout << "Graph is loaded." << std::endl;
	int vertexNum = align(csr->vertexNum, 8, 512); 
	std::cout << "verterNum " << csr->vertexNum << " is aligned to " << vertexNum << std::endl;

	char *hwDepth = (char*)malloc(vertexNum * sizeof(char));
	char *swDepth = (char*)malloc(vertexNum * sizeof(char));
	int *rpaoExtend = (int*)malloc(2*vertexNum * sizeof(int));
	for(int i = 0; i < vertexNum; i++){
		if(i < csr->vertexNum){
			rpaoExtend[2 * i] = csr->rpao[i];
			rpaoExtend[2 * i + 1] = csr->rpao[i + 1] - csr->rpao[i];
		}
		else{
			rpaoExtend[2 * i] = 0;
			rpaoExtend[2 * i + 1] = 0;
		}
	}

	int frontierSize = 1;	
	int ciaoSize = csr->ciao.size();
	//int rpaoSize = csr->rpao.size();

	std::cout << "soft bfs starts." << std::endl;
	swBfsInit(vertexNum, swDepth, startVertexIdx);
	begin = clock();
	swBfs(csr, swDepth, startVertexIdx);
	end = clock();
	elapsedTime = (end - begin)*1.0/CLOCKS_PER_SEC;
	std::cout << "Software bfs takes " << elapsedTime << " seconds." << std::endl;

	hwBfsInit(vertexNum, hwDepth, startVertexIdx);

	xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "bfs");
	cl_kernel krnl_bfs = xcl_get_kernel(program, "bfs");

    // Transfer data from system memory over PCIe to the FPGA on-board DDR memory.
    cl_mem devMemRpaoExtend = xcl_malloc(world, CL_MEM_READ_ONLY, 2 * vertexNum * sizeof(int));
	cl_mem devMemCiao = xcl_malloc(world, CL_MEM_READ_ONLY, ciaoSize * sizeof(int));
    cl_mem devMemDepth = xcl_malloc(world, CL_MEM_READ_WRITE, vertexNum * sizeof(char));
    cl_mem devMemFrontierSize = xcl_malloc(world, CL_MEM_WRITE_ONLY, sizeof(int));
    xcl_memcpy_to_device(world, devMemRpaoExtend, rpaoExtend, 2 * vertexNum * sizeof(int));
    xcl_memcpy_to_device(world, devMemCiao, csr->ciao.data(), ciaoSize * sizeof(int));
	xcl_memcpy_to_device(world, devMemDepth, hwDepth, vertexNum * sizeof(char));

	int nargs = 0;
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemDepth);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemDepth);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemDepth);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemRpaoExtend);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemCiao);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(int), &vertexNum);

	char level = 0;
	begin = clock();
	while(frontierSize != 0){
		xcl_set_kernel_arg(krnl_bfs, nargs, sizeof(char), &level);
		xcl_run_kernel3d(world, krnl_bfs, 1, 1, 1);
		xcl_memcpy_from_device(world, &frontierSize, devMemFrontierSize, sizeof(int));
		level++;
	}
	clFinish(world.command_queue);
	xcl_memcpy_from_device(world, hwDepth, devMemDepth, vertexNum * sizeof(char));
	end = clock();
	elapsedTime = (end - begin)*1.0/CLOCKS_PER_SEC;
	std::cout << "hardware bfs takes " << elapsedTime << " seconds." << std::endl;
	std::cout << "level = " << (int)level << std::endl;
	
	clReleaseMemObject(devMemRpaoExtend);
	clReleaseMemObject(devMemCiao);
	clReleaseMemObject(devMemDepth);
	clReleaseMemObject(devMemFrontierSize);
	clReleaseKernel(krnl_bfs);
	clReleaseProgram(program);
	xcl_release_world(world);

	verify(swDepth, hwDepth, csr->vertexNum);
	free(csr);
	free(swDepth);
	free(hwDepth);

	return 0;
}
