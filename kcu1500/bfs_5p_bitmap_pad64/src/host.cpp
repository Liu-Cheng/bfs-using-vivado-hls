/**
 * File              : host.cpp
 * Author            : Cheng Liu <st.liucheng@gmail.com>
 * Date              : 14.11.2017
 * Last Modified Date: 20.08.2018
 * Last Modified By  : Cheng Liu <st.liucheng@gmail.com>
 */

#include "xcl.h"
#include "graph.h"

#include <cstdio>
#include <vector>
#include <ctime>


#define HERE do {std::cout << "File: " << __FILE__ << " Line: " << __LINE__ << std::endl;} while(0)

static const char *error_message =
    "Error: Result mismatch:\n"
    "i = %d CPU result = %d Device result = %d\n";

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
	else if(gName == "rmat-19-32")  fName = "rmat-19-32.txt";
	else if(gName == "rmat-21-128") fName = "rmat-21-128.txt";
	else if(gName == "twitter")     fName = "twitter_rv.txt";
	else if(gName == "friendster")  fName = "friendster.ungraph.txt";
	else {
		std::cout << "Unknown graph name." << std::endl;
		exit(EXIT_FAILURE);
	}

	std::string fpath = dir + fName;
	return new Graph(fpath.c_str());
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
		std::cout << "software bfs: frontier.size() " << frontier.size() << std::endl;

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

int getStartVertexIdx(const std::string &gName)
{
	if(gName == "youtube")    return 320872;
	if(gName == "lj1")        return 3928512;
	if(gName == "pokec")      return 182045;
	if(gName == "rmat-19-32") return 104802;
	if(gName == "rmat-21-32") return 365723;
	return -1;
}

// Sum the array
int getSum(int *ptr, int num)
{	
	if(ptr == nullptr) return EXIT_FAILURE;
	int sum = 0;
	for(int i = 0; i < num; i++){
		sum += ptr[i];
	}
	return sum;
}

int main(int argc, char **argv) {
	std::clock_t begin;
	std::clock_t end;
	double elapsedTime;

	int pad = 64;
	std::string gName = "rmat-21-32";

	// Load padded graph data to memory
	Graph* gptr = createGraph(gName);
	CSR* csr = new CSR(*gptr, pad);
	int vertexNum = csr->vertexNum; 
	int rpaoSize = (int)(csr->rpao.size());
	int ciaoSize = (int)(csr->ciao.size());
	int segSize  = (vertexNum + pad - 1) / pad;
	char level   = 0;
	int startVertexIdx     = getStartVertexIdx(gName);
	char *hwDepth          = (char*)malloc(vertexNum * sizeof(char));
	char *swDepth          = (char*)malloc(vertexNum * sizeof(char));
	int  *frontier         = (int *)malloc(vertexNum * sizeof(int));
	int  *frontierSize     = (int *)malloc(pad * sizeof(int));
	int  *nextFrontier     = (int *)malloc(vertexNum * sizeof(int));
	int  *nextFrontierSize = (int *)malloc(pad * sizeof(int));
	for(int i = 0; i < pad; i++){
		frontierSize[i] = 0;
	}
	int bankIdx = startVertexIdx & 0xF;
	std::cout << "root_vidx: " << startVertexIdx << std::endl;
	std::cout << "bankIdx: " << bankIdx << std::endl;
	HERE;
	frontier[bankIdx * segSize] = startVertexIdx;
	frontierSize[bankIdx]  = 1;
	int totalFrontierSize  = 1;
	free(gptr);
	std::cout << "Graph is loaded." << std::endl;

	// software bfs
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
	cl_mem devMemRpao  = xcl_malloc(world, CL_MEM_READ_ONLY,  rpaoSize  * sizeof(int));
	cl_mem devMemCiao  = xcl_malloc(world, CL_MEM_READ_ONLY,  ciaoSize  * sizeof(int));
	cl_mem devMemFrontier = xcl_malloc(world, CL_MEM_READ_WRITE, vertexNum * sizeof(int));
	cl_mem devMemFrontierSize = xcl_malloc(world, CL_MEM_READ_WRITE, pad * sizeof(int));
	cl_mem devMemNextFrontier = xcl_malloc(world, CL_MEM_READ_WRITE, vertexNum * sizeof(int));
	cl_mem devMemNextFrontierSize = xcl_malloc(world, CL_MEM_READ_WRITE, pad * sizeof(int));
	xcl_memcpy_to_device(world, devMemRpao, csr->rpao.data(), rpaoSize * sizeof(int));
	xcl_memcpy_to_device(world, devMemCiao, csr->ciao.data(), ciaoSize * sizeof(int));
	xcl_memcpy_to_device(world, devMemFrontier, frontier, vertexNum * sizeof(int));
	xcl_memcpy_to_device(world, devMemFrontierSize, frontierSize, pad * sizeof(int));
	
	begin = clock();
	bool direction = true;
	while(totalFrontierSize > 0){
		if(direction){
			int nargs = 0;
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemRpao);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemCiao);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(int),    &startVertexIdx);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(int),    &segSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(int),    &pad);
			xcl_set_kernel_arg(krnl_bfs, nargs,   sizeof(char),   &level);
			xcl_run_kernel3d(world, krnl_bfs, 1, 1, 1);
			xcl_memcpy_from_device(world, nextFrontierSize, devMemNextFrontierSize, pad * sizeof(int));	

			/*
			xcl_memcpy_from_device(world, nextFrontier, devMemNextFrontier, vertexNum * sizeof(int));

			for(int i = 0; i < pad; i++){
				if(nextFrontierSize[i] > 0){
					for(int j = 0; j < nextFrontierSize[i]; j++){
						int vtmp = nextFrontier[i * segSize + j];
						hwDepth[vtmp] = level + 1;
					}
				}
			}
			*/

			totalFrontierSize = getSum(nextFrontierSize, pad);
			direction = false;
		}
		else{
			int nargs = 0;
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemNextFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemRpao);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemCiao);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontier);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(int),    &startVertexIdx);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(int),    &segSize);
			xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(int),    &pad);
			xcl_set_kernel_arg(krnl_bfs, nargs,   sizeof(char),   &level);
			xcl_run_kernel3d(world, krnl_bfs, 1, 1, 1);
			xcl_memcpy_from_device(world, frontierSize, devMemFrontierSize, pad * sizeof(int));

			/*
			xcl_memcpy_from_device(world, frontier, devMemFrontier, vertexNum * sizeof(int));
			
			for(int i = 0; i < pad; i++){
				if(frontierSize[i] > 0){
					for(int j = 0; j < frontierSize[i]; j++){
						int vtmp = frontier[i * segSize + j];
						hwDepth[vtmp] = level + 1;
					}
				}
				
			}
			*/

			totalFrontierSize = getSum(frontierSize, pad);
			direction = true;

		}

		level++;
	}
	clFinish(world.command_queue);
	end = clock();
	elapsedTime = (end - begin)*1.0/CLOCKS_PER_SEC;
	std::cout << "hardware bfs takes " << elapsedTime << " seconds." << std::endl;
	std::cout << "level = " << (int)level << std::endl;
	
	clReleaseMemObject(devMemRpao);
	clReleaseMemObject(devMemCiao);
	clReleaseMemObject(devMemNextFrontier);
	clReleaseMemObject(devMemNextFrontierSize);
	clReleaseMemObject(devMemFrontier);
	clReleaseMemObject(devMemFrontierSize);
	clReleaseKernel(krnl_bfs);
	clReleaseProgram(program);
	xcl_release_world(world);

	//verify(swDepth, hwDepth, csr->vertexNum);
	free(csr);
	free(frontier);
	free(nextFrontier);
	free(frontierSize);
	free(nextFrontierSize);
	free(swDepth);
	free(hwDepth);

	return 0;
}
