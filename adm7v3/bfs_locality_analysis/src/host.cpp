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
#include <cmath>
#include <climits>

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
	else if(gName == "rmat1k10k"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat1k10k.txt");
	}
	else if(gName == "rmat10k100k"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat10k100k.txt");
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

void updateStrideStat(
		const std::vector<int> &trace, 
		std::map<int, int> &stat,
		const int &windowSize){
	for(size_t i = windowSize; i < trace.size(); i++){

		// Search a window for reference distance
		int currentAddr = trace[i];
		int minDistance = INT_MAX;
		for(int j = 0; j < windowSize; j++){
			int dist = std::abs(trace[i - windowSize + j] - currentAddr);
			if(minDistance > dist){
				minDistance = dist;
			}
		}

		// Update stat information
		if(stat.find(minDistance) == stat.end()){
			stat[minDistance] = 1;
		}
		else{
			stat[minDistance]++;
		}
	}
}

void updateTempLocalityStat(
		const std::vector<int> &trace, 
		std::map<int, int> &stat){	

	// check last trace index at which an address is visited
	std::map<int, int> lastIndex; 

	// Check the repeated number of each address
	// We need this to determine the number of unique vertex
	std::map<int, int> idxStat;
	for(size_t i = 0; i < trace.size(); i++){
		// Update map container
		int addr = trace[i];

		if(idxStat.find(addr) == idxStat.end()){
			idxStat[addr] = 1;
		}
		else{
			idxStat[addr]++;
			
			//std::cout << "ref pair: " << lastIndex[addr] << " --> " << i << std::endl;
			std::map<int, int> unique;
			int refDist = 0;
			for(size_t j = lastIndex[addr]; j < i; j++){
				int tmp = trace[j];
				if(unique.find(tmp) == unique.end()){
					refDist++;
					unique[tmp] = 1;
					if(refDist > 4096){
						break;
					}
				}
			}
			if(stat.find(refDist) == stat.end()){
				stat[refDist] = 1;
			}
			else{
				stat[refDist]++;
			}
		}

		lastIndex[addr] = i;
	}

	// Check the unique vertices
	int counter = 0;
	for(auto it = idxStat.begin(); it != idxStat.end(); it++){
		if(it->second == 1){
			counter++;
		}
	}
	std::cout << "unique vertex amount: " << counter << std::endl;
}

void redundantRemove(
		const std::vector<int> &trace, 
		std::vector<int> &ngb
		){
	std::map<int, int> redundancyVerify;
	for(auto x : trace){
		if(redundancyVerify.find(x) == redundancyVerify.end()){
			redundancyVerify[x] = 1;
			ngb.push_back(x);
		}
	}
}

int main(int argc, char **argv) {
	int startVertexIdx = 365723;
	//int startVertexIdx = 23;
	std::string gName = "youtube";
	Graph* gptr = createGraph(gName);
	CSR* csr = new CSR(*gptr);
	std::cout << "Graph is loaded." << std::endl;
	char *depth = (char*)malloc(csr->vertexNum * sizeof(char));

	std::map<int, int> allNgbStrideStat;
	std::map<int, int> ngbStrideStat;
	std::map<int, int> tempLocalityStat;

	bfsInit(csr->vertexNum, depth, startVertexIdx);

	std::vector<int> frontier;
	char level = 0;
	while(true){
		std::vector<int> allNgbVertices;
		std::vector<int> ngbVertices;
		for(int i = 0; i < csr->vertexNum; i++){
			if(depth[i] == level){
				frontier.push_back(i);
				for(int cidx = csr->rpao[i]; cidx < csr->rpao[i+1]; cidx++){
					int ongb = csr->ciao[cidx];
					allNgbVertices.push_back(ongb);
					if(depth[ongb] == -1){
						depth[ongb] = level + 1;
					}
				}
			}
		}

		if(frontier.empty()){
			break;
		}

		if(allNgbVertices.size() >= 100){
			updateStrideStat(allNgbVertices, allNgbStrideStat, 32);
			redundantRemove(allNgbVertices, ngbVertices);
			updateTempLocalityStat(allNgbVertices, tempLocalityStat);
			if(ngbVertices.size() >= 100){
				updateStrideStat(ngbVertices, ngbStrideStat, 32);
			}
		}

		std::cout << "level = " << (int)level << std::endl;
		level++;
		frontier.clear();
	}

	std::cout << "spacial locality: " << std::endl;
	for(auto it = allNgbStrideStat.begin(); it != allNgbStrideStat.end(); it++){
		std::cout << it->first << ", ";
	}
	std::cout << std::endl;
	for(auto it = allNgbStrideStat.begin(); it != allNgbStrideStat.end(); it++){
		std::cout << it->second << ", ";
	}
	std::cout << std::endl;

	std::cout << "spacial locality (no redundancy): " << std::endl;
	for(auto it = ngbStrideStat.begin(); it != ngbStrideStat.end(); it++){
		std::cout << it->first << " ";
	}
	std::cout << std::endl;
	for(auto it = ngbStrideStat.begin(); it != ngbStrideStat.end(); it++){
		std::cout << it->second << " ";
	}
	std::cout << std::endl;


	std::cout << "temporal locality: " << std::endl;
	for(auto it = tempLocalityStat.begin(); it != tempLocalityStat.end(); it++){
		std::cout << it->first << " "; 
	}
	std::cout << std::endl;
	for(auto it = tempLocalityStat.begin(); it != tempLocalityStat.end(); it++){
		std::cout << it->second << " ";
	}
	std::cout << std::endl;

	free(csr);
	free(gptr);

	return 0;
}
