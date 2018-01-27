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
#include <map>

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

void updateMap(std::map<int, int> &burstAmount, const int &burstLen, const size_t &bytes){
	if(burstAmount.find(burstLen) == burstAmount.end()){
		burstAmount[burstLen] = bytes * burstLen;
	}
	else{
		burstAmount[burstLen] += bytes * burstLen;
	}
}


void updateMap(std::map<int, int> &burstMap, const int &burstLen){
	if(burstMap.find(burstLen) == burstMap.end()){
		burstMap[burstLen] = 1;
	}
	else{
		burstMap[burstLen]++;
	}
}

int main(int argc, char **argv) {
	int startVertexIdx = 365723;
	//int startVertexIdx = 23;
	std::string gName = "rmat-19-32";
	Graph* gptr = createGraph(gName);
	if(gName == "youtube") startVertexIdx = 320872;
	if(gName == "lj1") startVertexIdx = 3928512;
	if(gName == "pokec") startVertexIdx = 182045;
	if(gName == "rmat-19-32") startVertexIdx = 104802;
	if(gName == "rmat-21-128") startVertexIdx = 1177730;
	CSR* csr = new CSR(*gptr);
	std::cout << "Graph is loaded." << std::endl;
	char *depth = (char*)malloc(csr->vertexNum * sizeof(char));

	std::map<int, int> burstMap;
	std::map<int, int> burstAmount;
	bfsInit(csr->vertexNum, depth, startVertexIdx);

	std::vector<int> frontier;
	char level = 0;
	while(true){
		updateMap(burstMap, csr->vertexNum);
		updateMap(burstAmount, csr->vertexNum, sizeof(char));
		for(int i = 0; i < csr->vertexNum; i++){
			if(depth[i] == level){
				frontier.push_back(i);
				int offset = csr->rpao[i+1] - csr->rpao[i];
				if(offset > 0){
					updateMap(burstMap, offset);
					updateMap(burstAmount, offset, sizeof(int));
				}
				for(int cidx = csr->rpao[i]; cidx < csr->rpao[i+1]; cidx++){
					int ongb = csr->ciao[cidx];
					updateMap(burstMap, 1);
					updateMap(burstAmount, 1, sizeof(char));
					if(depth[ongb] == -1){
						depth[ongb] = level + 1;
						updateMap(burstMap, 1);
						updateMap(burstAmount, 1, sizeof(char));
					}
				}
			}
		}

		if(frontier.empty()){
			break;
		}

		std::cout << "level = " << (int)level << std::endl;
		level++;
		frontier.clear();
	}

	// Dump burst len infomration
	std::cout << "burst len = ";
	for(auto it = burstMap.begin(); it != burstMap.end(); it++){
		std::cout << it->first << ", ";
	}
	std::cout << std::endl;

	std::cout << "freq = ";
	for(auto it = burstMap.begin(); it != burstMap.end(); it++){
		std::cout << it->second << ", ";
	}
	std::cout << std::endl;

	std::cout << "burst amount: ";
	for(auto it = burstAmount.begin(); it != burstAmount.end(); it++){
		std::cout << it->second << ", ";
	}

	std::cout << std::endl;
	free(csr);
	free(gptr);

	return 0;
}
