/**
 * File              : graph.h
 * Date              : 22.08.2018
 * Last Modified Date: 22.08.2018
 */
/**
 * File              : graph.h
 * Date              : 22.08.2018
 * Last Modified Date: 22.08.2018
 */
/**
 * File              : graph.h
 * Date              : 22.08.2018
 * Last Modified Date: 22.08.2018
 */
/**
 * File              : graph.h
 * Date              : 22.08.2018
 * Last Modified Date: 22.08.2018
 */
#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <vector>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdlib>
#include <sstream>
#include <cmath>
#include <map>
#include <algorithm>

#define HERE do {std::cout << "File: " << __FILE__ << " Line: " << __LINE__ << std::endl;} while(0)

class Vertex {
    public:
        int idx;
        int inDeg;
        int outDeg;

        std::vector<int> inVid;
        std::vector<int> outVid;

        explicit Vertex(int _idx) {
            idx = _idx;
        }

        ~Vertex(){
            // Nothing is done here.
        }

};

class Graph{
    public:
        int vertexNum;
        int edgeNum;
        std::vector<Vertex*> vertices; 

        Graph(const std::string &fName);
        ~Graph();
        void getRandomStartIndices(std::vector<int> &startIndices);
        void getStat();

    private:
        bool isUgraph;
        int getMaxIdx(const std::vector<std::vector<int>> &data);
        int getMinIdx(const std::vector<std::vector<int>> &data);
        void loadFile(
				const std::string& fName,
                std::vector<std::vector<int>> &data
				);

};

class CSR{
    public:
		const int vertexNum;
		const int edgeNum;
		const int padLen;
        std::vector<int> rpao; 
        std::vector<int> ciao;
        std::vector<int> rpai;
        std::vector<int> ciai;

        // The CSR is constructed based on the simple graph
        explicit CSR(const Graph &g);
		explicit CSR(const Graph &g, const int _padLen);
		~CSR();

	private:
		int pad(const int &len);
};

#endif
