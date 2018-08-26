#include "graph.h"

void Graph::loadFile(
        const std::string& gName, 
        std::vector<std::vector<int>> &data
        )
{
    std::ifstream fhandle(gName.c_str());
    if(!fhandle.is_open()){
        HERE;
        std::cout << "Failed to open " << gName << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;
    while(std::getline(fhandle, line)){
        std::istringstream iss(line);
        data.push_back(
            std::vector<int>(std::istream_iterator<int>(iss),
            std::istream_iterator<int>())
        );
    }
    fhandle.close();
}

int Graph::getMaxIdx(const std::vector<std::vector<int>> &data){
    int maxIdx = data[0][0]; 
    for(auto it1 = data.begin(); it1 != data.end(); it1++){
        for(auto it2 = it1->begin(); it2 != it1->end(); it2++){            
            if(maxIdx <= (*it2)){
                maxIdx = *it2;
            }
        }
    }
    return maxIdx;
}

int Graph::getMinIdx(const std::vector<std::vector<int>> &data){
    int minIdx = data[0][0]; 
    for(auto it1 = data.begin(); it1 != data.end(); it1++){
        for(auto it2 = it1->begin(); it2 != it1->end(); it2++){            
            if(minIdx >= (*it2)){
                minIdx = *it2;
            }
        }
    }
    return minIdx;
}

Graph::Graph(const std::string& gName){

    // Check if it is undirectional graph
    auto found = gName.find("ungraph", 0);
    if(found != std::string::npos)
        isUgraph = true;
    else
        isUgraph = false;

    std::vector<std::vector<int>> data;
    loadFile(gName, data);
    vertexNum = getMaxIdx(data) + 1;
    edgeNum = (int)data.size();
    std::cout << "vertex num: " << vertexNum << std::endl;
    std::cout << "edge num: " << edgeNum << std::endl;

    for(int i = 0; i < vertexNum; i++){
        Vertex* v = new Vertex(i);
        vertices.push_back(v);
    }

    for(auto it = data.begin(); it != data.end(); it++){
        int srcIdx = (*it)[0];
        int dstIdx = (*it)[1];
        vertices[srcIdx]->outVid.push_back(dstIdx);
        vertices[dstIdx]->inVid.push_back(srcIdx);
        if(isUgraph && srcIdx != dstIdx){
            vertices[dstIdx]->outVid.push_back(srcIdx);
            vertices[srcIdx]->inVid.push_back(dstIdx);
        }
    }

    for(auto it = vertices.begin(); it != vertices.end(); it++){
        (*it)->inDeg = (int)(*it)->inVid.size();
        (*it)->outDeg = (int)(*it)->outVid.size();
    }
}

CSR::CSR(const Graph &g) 
	: vertexNum(g.vertexNum), edgeNum(g.edgeNum)
{
    rpao.resize(2 * vertexNum);
    rpai.resize(2 * vertexNum);
    for(int i = 0; i < vertexNum; i++)
	{
		int outDeg = g.vertices[i]->outDeg;
		int inDeg  = g.vertices[i]->inDeg;
		if(i == 0)
		{
			rpao[2 * i] = 0;
			rpai[2 * i] = 0;
		}
		else
		{
			rpao[2 * i] = rpao[2 * (i - 1)] + rpao[2 * (i - 1) + 1];
			rpai[2 * i] = rpai[2 * (i - 1)] + rpai[2 * (i - 1) + 1];
		}

		rpao[2 * i + 1] = outDeg;
		rpai[2 * i + 1] = inDeg;

		for(auto vid : g.vertices[i]->outVid){
            ciao.push_back(vid);
        }
        for(auto vid : g.vertices[i]->inVid){
            ciai.push_back(vid);
        }

    }
}

int CSR::pad(const int &len)
{
	if(len%padLen == 0) return len;
	else return ((len/padLen + 1) * padLen);
}

CSR::CSR(const Graph &g, const int &padLen) : 
	vertexNum(g.vertexNum), edgeNum(g.edgeNum), padLen(_padLen)
{
	rpao.resize(2 * vertexNum);
	rpai.resize(2 * vertexNum);
	for(int i = 0; i < vertexNum; i++)
	{
		if(i == 0)
		{
			rpao[2 * i] = 0;
			rpai[2 * i] = 0
		}
		else
		{
			rpao[2 * i] = rpao[2 * (i - 1)] + rpao[2 * (i - 1) + 1];
			rpai[2 * i] = rpai[2 * (i - 1)] + rpai[2 * (i - 1) + 1];
		}

		// update rpao and ciao
		int len = g.vertices[i]->outDeg;
		int paddedLen = pad(len);
		rpao[2 * i + 1] = paddedLen;

        for(int j = 0; j < paddedLen; j++)
		{
			if(j < len){
				ciao.push_back(g.vertices[i]->outVid[j]);
			}
			else{
				ciao.push_back(-1);
			}
		}

		// update rpai and ciai
		len =  g.vertices[i]->inDeg;
		paddedLen = pad(len);
		rpai[2 * i + 1] = paddedLen;

		for(int j = 0; j < paddedLen; j++)
		{
			if(j < len){
				ciai.push_back(g.vertices[i]->inVid[j]);
			}
			else{
				ciai.push_back(-1);
			}
		}
	}
} 

