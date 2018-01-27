/**********
Author: Cheng Liu
Email: st.liucheng@gmail.com
Software: SDx 2016.4
Date: July 6th 2017
**********/
#include "xcl2.hpp"
#include "graph.h"
#include <chrono>
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
	std::string home = "/data/graph-data/";
	//std::string home = "/home/liucheng/gitrepo/graph-data/";
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
	int vertexNum = align(csr->vertexNum, 8, 256*8); 
	std::cout << "Aligned vertex num: " << vertexNum << std::endl;

	// Allocate memory
	char *hwDepth_host = (char*)malloc(vertexNum * sizeof(char));
	char *swDepth = (char*)malloc(vertexNum * sizeof(char));

 //   std::vector<char,aligned_allocator<char>> swDepth(vertexNum);
	// software bfs
	{
		std::cout << "soft bfs starts." << std::endl;
		swBfsInit(vertexNum, swDepth, startVertexIdx);
		auto begin = std::chrono::high_resolution_clock::now();
		swBfs(csr, swDepth, startVertexIdx);
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
		std::cout << "Software bfs takes " << duration << " ms." << std::endl;
	}

	hwBfsInit(vertexNum, hwDepth_host, startVertexIdx);
	std::cout << "Hardware bfs initialization is done." << std::endl;

    	std::vector<char,aligned_allocator<char>> hwDepth(vertexNum);
	for(int i = 0; i < vertexNum; i++)
		hwDepth[i] = hwDepth_host[i];

/*
    std::vector<char,aligned_allocator<char>> hwDepth_b0(vertexNum/4);
    std::vector<char,aligned_allocator<char>> hwDepth_b1(vertexNum/4);
    std::vector<char,aligned_allocator<char>> hwDepth_b2(vertexNum/4);
    std::vector<char,aligned_allocator<char>> hwDepth_b3(vertexNum/4);

	// Initialize the depth partitions.
	{
		int id0 = 0; int id1 = 0; int id2 = 0; int id3 = 0;
		for(int i = 0; i < vertexNum; i++){
			int bank_idx = (i >> 6) & 0x3;
			if(bank_idx == 0){
				hwDepth_b0[id0++] = hwDepth[i];
			}
			else if(bank_idx == 1){
				hwDepth_b1[id1++] = hwDepth[i];
			}
			else if(bank_idx == 2){
				hwDepth_b2[id2++] = hwDepth[i];
			}
			else{
				hwDepth_b3[id3++] = hwDepth[i];
			}
		}
		std::cout << id0 << " " << id1 << " " << id2 << " " << id3 << std::endl;
	}
*/
    std::vector<int,aligned_allocator<int>> rpaoExtend(2 * vertexNum);
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

	int ciaoSize = csr->ciao.size();
    std::vector<int,aligned_allocator<int>> ciaoExtend(ciaoSize);
	for(int i = 0; i < ciaoSize; i++){
		ciaoExtend[i] = csr->ciao[i];
	}

    std::vector<int, aligned_allocator<int>> frontierSize(1);
	frontierSize[0] = 1;	

	std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];
    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>(); 
    std::string binaryFile = xcl::find_binary_file(device_name, "bfs");
    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    devices.resize(1);
    cl::Program program(context, devices, bins);
    cl::Kernel kernel(program, "bfs");

	// For Allocating Buffer to specific Global Memory Bank, 
	// user has to use cl_mem_ext_ptr_t and provide the Banks
	// depth is equally splitted and put into four banks.
	// rpao and ciao array have only one global memory port in the design,
	// so they are put in a single memory bank.
	// rpao->bank0, ciao->bank1, the rest will be put in bank3
   
	// cl_mem_ext_ptr_t depthExt0, depthExt1, depthExt2, depthExt3;
    cl_mem_ext_ptr_t depthExt;
    cl_mem_ext_ptr_t ciaoExt0,ciaoExt1,ciaoExt2,ciaoExt3;
    cl_mem_ext_ptr_t rpaoExt0,rpaoExt1,rpaoExt2,rpaoExt3;

    cl_mem_ext_ptr_t  fsExt;
   
    ciaoExt0.flags = XCL_MEM_DDR_BANK0; 
    ciaoExt1.flags = XCL_MEM_DDR_BANK1; 
    ciaoExt2.flags = XCL_MEM_DDR_BANK2; 
    ciaoExt3.flags = XCL_MEM_DDR_BANK3; 
	
    	rpaoExt0.flags = XCL_MEM_DDR_BANK0;
	rpaoExt1.flags = XCL_MEM_DDR_BANK1;
	rpaoExt2.flags = XCL_MEM_DDR_BANK2;
	rpaoExt3.flags = XCL_MEM_DDR_BANK3;

	depthExt.flags = XCL_MEM_DDR_BANK1;
	fsExt.flags = XCL_MEM_DDR_BANK2;

    ciaoExt0.obj = ciaoExtend.data(); 
    ciaoExt1.obj = ciaoExtend.data(); 
    ciaoExt2.obj = ciaoExtend.data(); 
    ciaoExt3.obj = ciaoExtend.data(); 
    rpaoExt0.obj = rpaoExtend.data();
    rpaoExt1.obj = rpaoExtend.data();
    rpaoExt2.obj = rpaoExtend.data();
    rpaoExt3.obj = rpaoExtend.data();
	depthExt.obj = hwDepth.data();
	fsExt.obj = frontierSize.data();

    depthExt.param = 0; 
	rpaoExt0.param = 0;
	rpaoExt1.param = 0;
	rpaoExt2.param = 0;
	rpaoExt3.param = 0;
	ciaoExt0.param = 0;
	ciaoExt1.param = 0;
	ciaoExt2.param = 0;
	ciaoExt3.param = 0;
	fsExt.param = 0;

    //Allocate Buffer in Bank0 of Global Memory for Input Image using Xilinx Extension
    cl::Buffer dev_depth(context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            (vertexNum) * sizeof(char), &depthExt);
       cl::Buffer dev_rpao0(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            2 * vertexNum * sizeof(int), &rpaoExt0);
	cl::Buffer dev_rpao1(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            2 * vertexNum * sizeof(int), &rpaoExt1);
	cl::Buffer dev_rpao2(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            2 * vertexNum * sizeof(int), &rpaoExt2);
	cl::Buffer dev_rpao3(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            2 * vertexNum * sizeof(int), &rpaoExt3);
	cl::Buffer dev_ciao0(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            ciaoSize * sizeof(int), &ciaoExt0);
	cl::Buffer dev_ciao1(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            ciaoSize * sizeof(int), &ciaoExt1);
	cl::Buffer dev_ciao2(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            ciaoSize * sizeof(int), &ciaoExt2);
	cl::Buffer dev_ciao3(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            ciaoSize * sizeof(int), &ciaoExt3);
	cl::Buffer dev_fs(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            sizeof(int), &fsExt);

	std::cout << "device memory is created." << std::endl;

    std::vector<cl::Memory> inBufVec, outBufVec, depthBufVec;
    inBufVec.push_back(dev_depth);
    inBufVec.push_back(dev_rpao0);
    inBufVec.push_back(dev_rpao1);
    inBufVec.push_back(dev_rpao2);
    inBufVec.push_back(dev_rpao3);
    inBufVec.push_back(dev_ciao0);
    inBufVec.push_back(dev_ciao1);
    inBufVec.push_back(dev_ciao2);
    inBufVec.push_back(dev_ciao3);
    outBufVec.push_back(dev_fs);
	depthBufVec.push_back(dev_depth);

	//Copy input data to device global memory
    q.enqueueMigrateMemObjects(inBufVec, 0 /* 0 means from host*/); 
	q.finish();
	std::cout << "Move data to device." << std::endl;
    
	// Launch the kernel
    auto krnl_bfs = cl::KernelFunctor<
		cl::Buffer&, 

		cl::Buffer&, 

		cl::Buffer&, 

		cl::Buffer&, 
		cl::Buffer&, 
		cl::Buffer&, 
		cl::Buffer&, 
		cl::Buffer&, 
		cl::Buffer&, 
		cl::Buffer&, 
		cl::Buffer&, 
		cl::Buffer&, 
		int, 
		char>(kernel);

	char level = 0;
	auto begin = std::chrono::high_resolution_clock::now();
	while(frontierSize[0] != 0){
		krnl_bfs(cl::EnqueueArgs(q,cl::NDRange(1,1,1), cl::NDRange(1,1,1)), 
				dev_depth, 

				dev_depth, 

				dev_depth, 

				dev_rpao0,
				dev_rpao1,
				dev_rpao2,
				dev_rpao3,
				dev_ciao0,
				dev_ciao1,
				dev_ciao2,
				dev_ciao3,
				dev_fs,

				vertexNum, 
				level);

		q.enqueueMigrateMemObjects(outBufVec, CL_MIGRATE_MEM_OBJECT_HOST);
		q.finish();
		level++;
	}
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
	std::cout << "hardware bfs takes " << duration << " ms." << std::endl;
	std::cout << "level = " << (int)level << std::endl;	


	q.enqueueMigrateMemObjects(depthBufVec, CL_MIGRATE_MEM_OBJECT_HOST);
	q.finish();

	for(int i = 0; i < vertexNum; i++)
		hwDepth_host[i] = hwDepth[i];


	verify(swDepth, hwDepth_host, csr->vertexNum);
	free(csr);
	free(swDepth);
	free(hwDepth_host);

	return 0;
}
