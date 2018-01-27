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
#include <algorithm>
#include <iterator>

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
	else if(gName == "lj-d"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/LiveJournal1.txt");
	}
	else if(gName == "orkut"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/orkut.ungraph.txt");
	}
	else if(gName == "rmat"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-2m-256m.txt");
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

void bfsInit(int vertexNum, char* depth, const int &vertexIdx){
	for(int i = 0; i < vertexNum; i++){
		depth[i] = -1;
	}
	depth[vertexIdx] = 0;
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



int main(int argc, char **argv) {
	//int startVertexIdx = 365723;
	//int startVertexIdx = 23;
	int bufferLimit[2] = {64*1024, 0};
	int hubThreshold[2] = {0, 0};
	double proportion[2] = {0, 0.5};
	std::string gName = "youtube";
	Graph* gptr = createGraph(gName);
	std::cout << "Graph is created." << std::endl;
	int* degree_distribution = (int*)malloc(gptr->vertexNum * sizeof(int));
	for(int i = 0; i < gptr->vertexNum; i++){
		degree_distribution[i] = 0;
	}

	for(auto vptr : gptr->vertices){
		degree_distribution[vptr->outDeg]++;
	}

	long long edgeNum = 0;
	for(int i = 0; i < gptr->vertexNum; i++){
		edgeNum += i * degree_distribution[i];
	}
	std::cout << "edgeNum = " << edgeNum << std::endl;

	// Analyze given buffer limit.
	long long sum = 0;
	int topIdx = 0;
	for(int i = gptr->vertexNum - 1; i >= 0; i--){
		if(degree_distribution[i] > 0){
			topIdx += degree_distribution[i];
			sum += degree_distribution[i] * i;
		}
		if(topIdx >= bufferLimit[0]){ 
			hubThreshold[0] = i;
			proportion[0] = sum * 1.0/edgeNum;
			break;
		}
	}

	sum = (long long)(edgeNum * proportion[1]);
	topIdx = 0;
	for(int i = gptr->vertexNum - 1; i >= 0; i--){
		if(degree_distribution[i] > 0){
			sum -= i * degree_distribution[i];
			topIdx += degree_distribution[i];
		}

		if(sum < 0){
			hubThreshold[1] = i;
			bufferLimit[1] = topIdx;
			break;
		}
	}

	std::cout << "bufferLimit: " << bufferLimit[0] << " " << bufferLimit[1] << std::endl;
	std::cout << "hubThreshold: " << hubThreshold[0] << " " << hubThreshold[1] << std::endl;
	std::cout << "proportion: " << proportion[0] << " " << proportion[1] << std::endl;

	std::cout << "degree distribution: " << std::endl;
	for(int i = 0; i < gptr->vertexNum; i++){
		if(degree_distribution[i] > 0){
			std::cout << i*degree_distribution[i] << ", ";
		}
	}
	std::cout << std::endl;

	std::cout << "vertex distribution: " << std::endl;
	for(int i = 0; i < gptr->vertexNum; i++){
		if(degree_distribution[i] > 0){
			std::cout << degree_distribution[i] << ", ";
		}
	}
	std::cout << std::endl;

	std::cout << "degree sequence: " << std::endl;
	for(int i = 0; i < gptr->vertexNum; i++){
		if(degree_distribution[i] > 0){
			std::cout << i << ", ";
		}
	}
	std::cout << std::endl;


	free(gptr);
	free(degree_distribution);

	return 0;
}
