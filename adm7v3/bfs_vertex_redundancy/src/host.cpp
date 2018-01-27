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



int main(int argc, char **argv) {
	int startVertexIdx = 365723;
	//int startVertexIdx = 23;
	std::string gName = "youtube";
	Graph* gptr = createGraph(gName);
	CSR* csr = new CSR(*gptr);
	std::cout << "Graph is loaded." << std::endl;
	char *depth = (char*)malloc(csr->vertexNum * sizeof(char));

	std::vector<int> actualVisitedVertexNum;
	std::vector<int> visitedUniqueVertexNum;
	std::vector<int> optimizedVertexNum;
	std::vector<int> frontierSize;

	bfsInit(csr->vertexNum, depth, startVertexIdx);

	std::vector<int> frontier;
	char level = 0;
	while(true){
		std::vector<int> traversedVertices;
		for(int i = 0; i < csr->vertexNum; i++){
			if(depth[i] == level){
				frontier.push_back(i);
				for(int cidx = csr->rpao[i]; cidx < csr->rpao[i+1]; cidx++){
					int ongb = csr->ciao[cidx];
					traversedVertices.push_back(ongb);
					if(depth[ongb] == -1){
						depth[ongb] = level + 1;
					}
				}
			}
		}
		std::vector<int> combinedWithFrontier;
		combinedWithFrontier.reserve(frontier.size() + traversedVertices.size());
		combinedWithFrontier.insert(combinedWithFrontier.end(), traversedVertices.begin(), traversedVertices.end());
		combinedWithFrontier.insert(combinedWithFrontier.end(), frontier.begin(), frontier.end());
		std::sort(combinedWithFrontier.begin(), combinedWithFrontier.end());
		std::sort(frontier.begin(), frontier.end());
		std::sort(traversedVertices.begin(), traversedVertices.end());
		auto it0 = std::unique(combinedWithFrontier.begin(), combinedWithFrontier.end());
		int optimizedNum = std::distance(combinedWithFrontier.begin(), it0) - frontier.size();
		optimizedVertexNum.push_back(optimizedNum);

		actualVisitedVertexNum.push_back(traversedVertices.size());
		auto it1 = std::unique(traversedVertices.begin(), traversedVertices.end());
		visitedUniqueVertexNum.push_back(std::distance(traversedVertices.begin(), it1));

		if(frontier.empty()){
			break;
		}

		frontierSize.push_back(frontier.size());

		std::cout << "level = " << (int)level << std::endl;
		level++;
		frontier.clear();
	}

	std::cout << "visited neighbors in each frontier: " << std::endl;
	for(auto x : actualVisitedVertexNum){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "visited unique neighbor vertices in each frontier: " << std::endl;
	for(auto x : visitedUniqueVertexNum){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "visited neighbor vertices after removing ";
	std::cout << "the visited frontier and redudannt vertices: " << std::endl;
	for(auto x : optimizedVertexNum){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "frontier size: " << std::endl;
	for(auto x : frontierSize){
		std::cout << x << ", ";
	}
	std::cout << std::endl;

	free(csr);
	free(gptr);

	return 0;
}
