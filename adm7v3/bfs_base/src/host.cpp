/**
 * File              : src/host.cpp
 * Author            : Cheng Liu <st.liucheng@gmail.com>
 * Date              : 04.12.2017
 * Last Modified Date: 18.12.2017
 * Last Modified By  : Cheng Liu <st.liucheng@gmail.com>
 */

#include "xcl.h"
#include "graph.h"
#include "CL/cl.h"

#include <cstdio>
#include <vector>
#include <string>
#include <ctime>

size_t offset[1] = {0};
size_t global[1] = {1};
size_t local[1] = {1};

static const char *error_message =
    "Error: Result mismatch:\n"
    "i = %d CPU result = %d Device result = %d\n";	

#define OCL_CHECK(call)                                                           \
		do {                                                                      \
			cl_int err = call;                                                    \
			if (err != CL_SUCCESS) {                                              \
				printf(__FILE__ ":%d: [ERROR] " #call " returned \n", __LINE__); \
				exit(EXIT_FAILURE);                                               \
			}                                                                     \
		} while (0);


Graph* createGraph(const std::string &gName){
	Graph* gptr;
	if(gName == "dblp"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/dblp.ungraph.txt");
	}
	else if(gName == "youtube"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/youtube.ungraph.txt");
	}
	else if(gName == "lj"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/lj.ungraph.txt");
	}
	else if(gName == "pokec"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/pokec-relationships.txt");
	}
	else if(gName == "wiki-talk"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/wiki-Talk.txt");
	}
	else if(gName == "lj1"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/LiveJournal1.txt");
	}
	else if(gName == "orkut"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/orkut.ungraph.txt");
	}
	else if(gName == "rmat-21-32"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-21-32.txt");
	}
	else if(gName == "rmat-19-32"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-19-32.txt");
	}
	else if(gName == "rmat-21-128"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-21-128.txt");
	}
	else if(gName == "twitter"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/twitter_rv.txt");
	}
	else if(gName == "friendster"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/friendster.ungraph.txt");
	}
	else if(gName == "example"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-1k-10k.txt");
	}
	else{
		std::cout << "Unknown graph name." << std::endl;
		exit(EXIT_FAILURE);
	}

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
	//printf("swBfs frontier size: ");
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

		//printf("%zu ", frontier.size());

		level++;
		frontier.clear();
	}
	printf("\n");
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


void csr2udf(
		CSR* csr, 
		int* g_graph_node_start, 
		int* g_graph_edge_num, 
		int* g_graph_edges,
		char* g_graph_frontier_mask, 
		char* g_graph_visited_mask, 
		char* g_graph_updating_mask, 
		char* g_graph_cost,
		const int vertexNum,	
		const int startVertexIdx){
	for(int i = 0; i < csr->vertexNum; i++){
		g_graph_node_start[i] = csr->rpao[i];
		g_graph_edge_num[i] = csr->rpao[i+1] - csr->rpao[i];
	}
	
	for(size_t i = 0; i < csr->ciao.size(); i++){
		g_graph_edges[i] = csr->ciao[i];
	}

	// Note that the aligned part should also be initialized
	for(int i = 0; i < vertexNum; i++){
		g_graph_frontier_mask[i] = 0;
		g_graph_visited_mask[i] = 0;
		g_graph_updating_mask[i] = 0;
		g_graph_cost[i] = -1;
	}
	g_graph_frontier_mask[startVertexIdx] = 1;
	g_graph_visited_mask[startVertexIdx] = 1;
	g_graph_cost[startVertexIdx] = 0;
}


int main(int argc, char **argv) {
	std::clock_t begin;
	std::clock_t end;
	double elapsedTime;

	int startVertexIdx = 365723;
	std::string gName = "pokec";
	if(gName == "youtube") startVertexIdx = 320872;
	if(gName == "lj1") startVertexIdx = 3928512;
	if(gName == "pokec") startVertexIdx = 182045;
	if(gName == "rmat-19-32") startVertexIdx = 104802;
	if(gName == "rmat-21-32") startVertexIdx = 365723;

	Graph* gptr = createGraph(gName);
	CSR* csr = new CSR(*gptr);
	free(gptr);
	std::cout << "Graph is loaded." << std::endl;
	int vertexNum = align(csr->vertexNum, 8, 8*4096); 
	int edgeNum = csr->ciao.size();
	std::cout << "verterNum " << csr->vertexNum << " is aligned to " << vertexNum << std::endl;
	std::cout << "edgeNum " << edgeNum << std::endl;

	// convert csr data to the user defined format
	int* g_graph_node_start = (int*)malloc(vertexNum * sizeof(int));
	int* g_graph_edge_num = (int*)malloc(vertexNum * sizeof(int));
	int* g_graph_edges = (int*)malloc(edgeNum * sizeof(int));
	char* g_graph_frontier_mask = (char*)malloc(vertexNum * sizeof(char));
	char* g_graph_visited_mask = (char*)malloc(vertexNum * sizeof(char));
	char* g_graph_updating_mask = (char*)malloc(vertexNum * sizeof(char));
	char *g_graph_cost = (char*)malloc(vertexNum * sizeof(char));
	char *swDepth = (char*)malloc(vertexNum * sizeof(char));
	int frontierSize = 1;	

	csr2udf(csr, g_graph_node_start, g_graph_edge_num, g_graph_edges,
			g_graph_frontier_mask, g_graph_visited_mask, g_graph_updating_mask, 
			g_graph_cost, vertexNum, startVertexIdx);

	
	// software bfs
	std::cout << "soft bfs starts." << std::endl;
	swBfsInit(vertexNum, swDepth, startVertexIdx);
	begin = clock();
	swBfs(csr, swDepth, startVertexIdx);
	end = clock();
	elapsedTime = (end - begin)*1.0/CLOCKS_PER_SEC;
	std::cout << "Software bfs takes " << elapsedTime << " seconds." << std::endl;

	// load kernels
	xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "bfs");
	cl_kernel krnl_k0 = xcl_get_kernel(program, "bfs_k0");
	cl_kernel krnl_k1 = xcl_get_kernel(program, "bfs_k1");

    // Transfer data from system memory over PCIe to the FPGA on-board DDR memory.
    cl_mem devMemNodeStart = xcl_malloc(world, CL_MEM_READ_ONLY, vertexNum * sizeof(int));
	cl_mem devMemEdgeNum = xcl_malloc(world, CL_MEM_READ_ONLY, vertexNum * sizeof(int));
    cl_mem devMemEdges = xcl_malloc(world, CL_MEM_READ_ONLY, edgeNum * sizeof(int));
    cl_mem devMemFrontierMask = xcl_malloc(world, CL_MEM_READ_WRITE, vertexNum * sizeof(char));
    cl_mem devMemVisitedMask = xcl_malloc(world, CL_MEM_READ_WRITE, vertexNum * sizeof(char));
    cl_mem devMemUpdatingMask = xcl_malloc(world, CL_MEM_READ_WRITE, vertexNum * sizeof(char));
    cl_mem devMemCost = xcl_malloc(world, CL_MEM_READ_WRITE, vertexNum * sizeof(char));
    cl_mem devMemFrontierSize = xcl_malloc(world, CL_MEM_WRITE_ONLY, sizeof(int));

    xcl_memcpy_to_device(world, devMemNodeStart, g_graph_node_start, vertexNum * sizeof(int));
    xcl_memcpy_to_device(world, devMemEdgeNum, g_graph_edge_num, vertexNum * sizeof(int));
	xcl_memcpy_to_device(world, devMemEdges, g_graph_edges, edgeNum * sizeof(int));
	xcl_memcpy_to_device(world, devMemFrontierMask, g_graph_frontier_mask, vertexNum * sizeof(char));
	xcl_memcpy_to_device(world, devMemVisitedMask, g_graph_visited_mask, vertexNum * sizeof(char));
	xcl_memcpy_to_device(world, devMemUpdatingMask, g_graph_updating_mask, vertexNum * sizeof(char));
	xcl_memcpy_to_device(world, devMemCost, g_graph_cost, vertexNum * sizeof(char));

	int nargs0 = 0;
	int nargs1 = 0;
	xcl_set_kernel_arg(krnl_k0, nargs0++, sizeof(cl_mem), &devMemNodeStart);
	xcl_set_kernel_arg(krnl_k0, nargs0++, sizeof(cl_mem), &devMemEdgeNum);
	xcl_set_kernel_arg(krnl_k0, nargs0++, sizeof(cl_mem), &devMemEdges);
	xcl_set_kernel_arg(krnl_k0, nargs0++, sizeof(cl_mem), &devMemFrontierMask);
	xcl_set_kernel_arg(krnl_k0, nargs0++, sizeof(cl_mem), &devMemUpdatingMask);
	xcl_set_kernel_arg(krnl_k0, nargs0++, sizeof(cl_mem), &devMemVisitedMask);
	xcl_set_kernel_arg(krnl_k0, nargs0++, sizeof(cl_mem), &devMemCost);
	xcl_set_kernel_arg(krnl_k0, nargs0++, sizeof(int),    &vertexNum);

	xcl_set_kernel_arg(krnl_k1, nargs1++, sizeof(cl_mem), &devMemFrontierMask);
	xcl_set_kernel_arg(krnl_k1, nargs1++, sizeof(cl_mem), &devMemUpdatingMask);
	xcl_set_kernel_arg(krnl_k1, nargs1++, sizeof(cl_mem), &devMemVisitedMask);
	xcl_set_kernel_arg(krnl_k1, nargs1++, sizeof(cl_mem), &devMemFrontierSize);
	xcl_set_kernel_arg(krnl_k1, nargs1++, sizeof(int), &vertexNum);

	cl_int err;
	cl_event k0_event;
	cl_command_queue seq_queue = clCreateCommandQueue(
			world.context, world.device_id, CL_QUEUE_PROFILING_ENABLE, &err);

	char level = 0;
	begin = clock();
	//printf("hwBfs frontier size: ");
	while(frontierSize != 0){
		//printf("%d ", frontierSize);
		xcl_set_kernel_arg(krnl_k0, nargs0, sizeof(char), &level);	
		clEnqueueNDRangeKernel(
					seq_queue, 
					krnl_k0, 1, 
					offset, global, 
					local, 0,
					nullptr, 
					&k0_event);

		OCL_CHECK(clEnqueueNDRangeKernel(seq_queue, 
					krnl_k1, 1, 
					offset, global, 
					local, 1,
					&k0_event, 
					nullptr));

		level++;
		clFlush(seq_queue);
		clFinish(seq_queue);
		xcl_memcpy_from_device(world, &frontierSize, devMemFrontierSize, sizeof(int));
	}
	std::cout << std::endl;
	xcl_memcpy_from_device(world, g_graph_cost, devMemCost, vertexNum * sizeof(char));
	end = clock();
	elapsedTime = (end - begin)*1.0/CLOCKS_PER_SEC;

	std::cout << "hardware bfs takes " << elapsedTime << " seconds." << std::endl;
	std::cout << "level = " << (int)level << std::endl;
	
	clReleaseMemObject(devMemNodeStart);
	clReleaseMemObject(devMemEdgeNum);
	clReleaseMemObject(devMemEdges);
	clReleaseMemObject(devMemFrontierMask);
	clReleaseMemObject(devMemUpdatingMask);
	clReleaseMemObject(devMemVisitedMask);
	clReleaseMemObject(devMemCost);
	clReleaseMemObject(devMemFrontierSize);
	clReleaseKernel(krnl_k0);
	clReleaseKernel(krnl_k1);
	clReleaseProgram(program);
	xcl_release_world(world);

	verify(swDepth, g_graph_cost, csr->vertexNum);

	free(csr);
	free(swDepth);
	free(g_graph_node_start);
	free(g_graph_edge_num);
	free(g_graph_edges);
	free(g_graph_frontier_mask);
	free(g_graph_updating_mask);
	free(g_graph_visited_mask);
	free(g_graph_cost);

	return 0;
}
