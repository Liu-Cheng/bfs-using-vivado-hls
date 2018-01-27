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
	std::cout << "swBfs: frontier size: ";
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
		std::cout << frontier.size() << " ";

		level++;
		frontier.clear();
	}
	std::cout << std::endl;
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
	std::cout << gName << std::endl;
	Graph* gptr = createGraph(gName);
	CSR* csr = new CSR(*gptr);
	free(gptr);
	std::cout << "Graph is loaded." << std::endl;
	int vertexNum = align(csr->vertexNum, 8, 256*8); 
	std::cout << "Aligned vertex num: " << vertexNum << std::endl;

	// Allocate memory
	char *hwDepth = (char*)malloc(vertexNum * sizeof(char));
	char *swDepth = (char*)malloc(vertexNum * sizeof(char));

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

	hwBfsInit(vertexNum, hwDepth, startVertexIdx);
	std::cout << "Hardware bfs initialization is done." << std::endl;

    std::vector<char,aligned_allocator<char>> hwDepth_b0(vertexNum/2);
    std::vector<char,aligned_allocator<char>> hwDepth_b1(vertexNum/2);

	int rpao_b0_size = vertexNum/4;
	int rpao_b1_size = vertexNum/4;
	int rpao_b2_size = vertexNum/4;
	int rpao_b3_size = vertexNum/4;
	rpao_b0_size = align(rpao_b0_size, 32, 512); 
	rpao_b1_size = align(rpao_b1_size, 32, 512); 
	rpao_b2_size = align(rpao_b2_size, 32, 512); 
	rpao_b3_size = align(rpao_b3_size, 32, 512); 

	int ciao_b0_size = 0;
	int ciao_b1_size = 0;
	int ciao_b2_size = 0;
	int ciao_b3_size = 0;
	for(int i = 0; i < csr->vertexNum; i++){
		int channel = (i >> 6) & 0x3;
		if(channel == 0){
			ciao_b0_size += csr->rpao[i+1] - csr->rpao[i];
		}
		if(channel == 1){
			ciao_b1_size += csr->rpao[i+1] - csr->rpao[i];
		}
		if(channel == 2){
			ciao_b2_size += csr->rpao[i+1] - csr->rpao[i];
		}
		if(channel == 3){
			ciao_b3_size += csr->rpao[i+1] - csr->rpao[i];
		}
	}
	ciao_b0_size = align(ciao_b0_size, 32, 512); 
	ciao_b1_size = align(ciao_b1_size, 32, 512); 
	ciao_b2_size = align(ciao_b2_size, 32, 512); 
	ciao_b3_size = align(ciao_b3_size, 32, 512); 

	std::cout << "ciao allocates " << ciao_b0_size << " data to bank0" << std::endl;
	std::cout << "ciao allocates " << ciao_b1_size << " data to bank1" << std::endl;
	std::cout << "ciao allocates " << ciao_b2_size << " data to bank2" << std::endl;
	std::cout << "ciao allocates " << ciao_b3_size << " data to bank3" << std::endl;

    std::vector<int, aligned_allocator<int>> ciao_b0(ciao_b0_size, -1);
    std::vector<int, aligned_allocator<int>> ciao_b1(ciao_b1_size, -1);
    std::vector<int, aligned_allocator<int>> ciao_b2(ciao_b2_size, -1);
    std::vector<int, aligned_allocator<int>> ciao_b3(ciao_b3_size, -1);

	std::vector<int, aligned_allocator<int>> rpao_b0(2 * rpao_b0_size);
    std::vector<int, aligned_allocator<int>> rpao_b1(2 * rpao_b1_size);
    std::vector<int, aligned_allocator<int>> rpao_b2(2 * rpao_b2_size);
    std::vector<int, aligned_allocator<int>> rpao_b3(2 * rpao_b3_size);

	// Initialize the depth partitions.
	{
		int id0 = 0; int id1 = 0; 
		for(int i = 0; i < vertexNum; i++){
			int channel = (i >> 6) & 0x1;
			if(channel == 0){
				hwDepth_b0[id0++] = hwDepth[i];
			}
			else if(channel == 1){
				hwDepth_b1[id1++] = hwDepth[i];
			}
		}
	}

	// Initialize rpao partition
	{
		int id0 = 0; int id1 = 0; int id2 = 0; int id3 = 0;
		int ridx0 = 0; int ridx1 = 0; int ridx2 = 0; int ridx3 = 0;
		for(int i = 0; i < vertexNum; i++){
			if(i < csr->vertexNum){
				int channel = (i >> 6) & 0x3;
				if(channel == 0){
					rpao_b0[2 * id0] = ridx0;
					rpao_b0[2 * id0 + 1] = csr->rpao[i + 1] - csr->rpao[i];
					id0++;
					ridx0 += csr->rpao[i+1] - csr->rpao[i];
				}
				else if(channel == 1){
					rpao_b1[2 * id1] = ridx1;
					rpao_b1[2 * id1 + 1] =  csr->rpao[i + 1] - csr->rpao[i];
					id1++;
					ridx1 += csr->rpao[i+1] - csr->rpao[i];
				}
				else if(channel == 2){
					rpao_b2[2 * id2] = ridx2;
					rpao_b2[2 * id2 + 1] = csr->rpao[i + 1] - csr->rpao[i];
					id2++;
					ridx2 += csr->rpao[i+1] - csr->rpao[i];
				}
				else{
					rpao_b3[2 * id3] = ridx3;
					rpao_b3[2 * id3 + 1] = csr->rpao[i + 1] - csr->rpao[i];
					id3++;
					ridx3 += csr->rpao[i+1] - csr->rpao[i];
				}
			}
			else{
				rpao_b3[2 * id3] = 0;
				rpao_b3[2 * id3 + 1] = 0;
				id3++;
			}
		}
		std::cout << "rpao array is spreaded to the 4 memory banks." << std::endl;
	}

	//Initialize ciao partition
	{
		int id0 = 0; int id1 = 0; int id2 = 0; int id3 = 0;
		for(int i = 0; i < csr->vertexNum; i++){	
			int channel = (i >> 6) & 0x3;
			int begin = csr->rpao[i];
			int end = csr->rpao[i+1];
			if(channel == 0){
				for(int j = begin; j < end; j++){
					ciao_b0[id0++] = csr->ciao[j];
				}
			}
			else if(channel == 1){
				for(int j = begin; j < end; j++){
					ciao_b1[id1++] = csr->ciao[j];
				}
			}
			else if(channel == 2){
				for(int j = begin; j < end; j++){
					ciao_b2[id2++] = csr->ciao[j];
				}
			}
			else{
				for(int j = begin; j < end; j++){
					ciao_b3[id3++] = csr->ciao[j];
				}
			}
		}
		std::cout << "ciao array is spreaded to the 4 memory banks." << std::endl;
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

    cl_mem_ext_ptr_t depth_b0_ext;
    cl_mem_ext_ptr_t depth_b1_ext;

	cl_mem_ext_ptr_t rpao_b0_ext;
	cl_mem_ext_ptr_t rpao_b1_ext;
	cl_mem_ext_ptr_t rpao_b2_ext;
	cl_mem_ext_ptr_t rpao_b3_ext;

	cl_mem_ext_ptr_t ciao_b0_ext;
	cl_mem_ext_ptr_t ciao_b1_ext;
	cl_mem_ext_ptr_t ciao_b2_ext;
	cl_mem_ext_ptr_t ciao_b3_ext;

	cl_mem_ext_ptr_t fs_ext;

    depth_b0_ext.flags = XCL_MEM_DDR_BANK0; 
    depth_b1_ext.flags = XCL_MEM_DDR_BANK1; 

	rpao_b0_ext.flags = XCL_MEM_DDR_BANK0;
	rpao_b1_ext.flags = XCL_MEM_DDR_BANK1;
	rpao_b2_ext.flags = XCL_MEM_DDR_BANK2;
	rpao_b3_ext.flags = XCL_MEM_DDR_BANK3;

	ciao_b0_ext.flags = XCL_MEM_DDR_BANK0;
	ciao_b1_ext.flags = XCL_MEM_DDR_BANK1;
	ciao_b2_ext.flags = XCL_MEM_DDR_BANK2;
	ciao_b3_ext.flags = XCL_MEM_DDR_BANK3;

	fs_ext.flags = XCL_MEM_DDR_BANK0;

    depth_b0_ext.obj = hwDepth_b0.data(); 
    depth_b1_ext.obj = hwDepth_b1.data(); 

    rpao_b0_ext.obj = rpao_b0.data();
	rpao_b1_ext.obj = rpao_b1.data();
	rpao_b2_ext.obj = rpao_b2.data();
	rpao_b3_ext.obj = rpao_b3.data();
               
	ciao_b0_ext.obj = ciao_b0.data();
	ciao_b1_ext.obj = ciao_b1.data();
	ciao_b2_ext.obj = ciao_b2.data();
	ciao_b3_ext.obj = ciao_b3.data();

	fs_ext.obj = frontierSize.data();

    depth_b0_ext.param = 0; 
    depth_b1_ext.param = 0; 

	rpao_b0_ext.param = 0;
	rpao_b1_ext.param = 0;
	rpao_b2_ext.param = 0;
	rpao_b3_ext.param = 0;

	ciao_b0_ext.param = 0;
	ciao_b1_ext.param = 0;
	ciao_b2_ext.param = 0;
	ciao_b3_ext.param = 0;

	fs_ext.param = 0;

    //Allocate Buffer in Bank0 of Global Memory for Input Image using Xilinx Extension
    cl::Buffer dev_depth_b0(context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            (vertexNum/2) * sizeof(char), &depth_b0_ext);
	cl::Buffer dev_depth_b1(context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            (vertexNum/2) * sizeof(char), &depth_b1_ext);

    cl::Buffer dev_rpao_b0(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            2 * rpao_b0_size * sizeof(int), &rpao_b0_ext);
	cl::Buffer dev_rpao_b1(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            2 * rpao_b1_size * sizeof(int), &rpao_b1_ext);
	cl::Buffer dev_rpao_b2(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			2 * rpao_b2_size * sizeof(int), &rpao_b2_ext);
	cl::Buffer dev_rpao_b3(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			2 * rpao_b3_size * sizeof(int), &rpao_b3_ext);

	cl::Buffer dev_ciao_b0(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            ciao_b0_size * sizeof(int), &ciao_b0_ext);
	cl::Buffer dev_ciao_b1(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			ciao_b1_size * sizeof(int), &ciao_b1_ext);
	cl::Buffer dev_ciao_b2(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			ciao_b2_size * sizeof(int), &ciao_b2_ext);
	cl::Buffer dev_ciao_b3(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
			ciao_b3_size * sizeof(int), &ciao_b3_ext);

	cl::Buffer dev_fs(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            sizeof(int), &fs_ext);

    std::vector<cl::Memory> inBufVec, outBufVec, depthBufVec;
    inBufVec.push_back(dev_depth_b0);
    inBufVec.push_back(dev_depth_b1);

    inBufVec.push_back(dev_rpao_b0);
    inBufVec.push_back(dev_rpao_b1);
    inBufVec.push_back(dev_rpao_b2);
    inBufVec.push_back(dev_rpao_b3);

    inBufVec.push_back(dev_ciao_b0);
    inBufVec.push_back(dev_ciao_b1);
    inBufVec.push_back(dev_ciao_b2);
    inBufVec.push_back(dev_ciao_b3);

    outBufVec.push_back(dev_fs);

	depthBufVec.push_back(dev_depth_b0);
	depthBufVec.push_back(dev_depth_b1);

	//Copy input data to device global memory
    q.enqueueMigrateMemObjects(inBufVec, 0 /* 0 means from host*/); 
	q.finish();
	std::cout << "Moving data to device memory." << std::endl;
    
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

		cl::Buffer&, 

		cl::Buffer&, 
		cl::Buffer&, 

		int, 
		char>(kernel);

	char level = 0;
	auto begin = std::chrono::high_resolution_clock::now();
	while(frontierSize[0] != 0){
		krnl_bfs(cl::EnqueueArgs(q,cl::NDRange(1,1,1), cl::NDRange(1,1,1)), 
				dev_depth_b0, 
				dev_depth_b1,
				dev_depth_b0,
				dev_depth_b1,

				dev_rpao_b0, 
				dev_rpao_b1,
				dev_rpao_b2,
				dev_rpao_b3,

				dev_ciao_b0, 
				dev_ciao_b1,
				dev_ciao_b2,
				dev_ciao_b3,

				dev_fs,

				dev_depth_b0, 
				dev_depth_b1,

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

	{
		int id0 = 0; int id1 = 0; 
		for(int i = 0; i < vertexNum; i++){
			int bank_idx = (i >> 6) & 0x1;
			if(bank_idx == 0){
				hwDepth[i] = hwDepth_b0[id0++];
			}
			else if(bank_idx == 1){
				hwDepth[i] = hwDepth_b1[id1++];
			}
		}
	}

	verify(swDepth, hwDepth, csr->vertexNum);
	free(csr);
	free(swDepth);
	free(hwDepth);

	return 0;
}
