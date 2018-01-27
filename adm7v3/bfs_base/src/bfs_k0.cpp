#include "stdio.h"

extern "C" {
void bfs_k0(
		const int* g_graph_node_start,
		const int* g_graph_edge_num,
		const int* g_graph_edges,
		const char* g_graph_frontier_mask,
		char* g_graph_updating_mask,
		char* g_graph_visited_mask,
		char*  g_cost,
		const int nodes_num,
		const char level) 
{
#pragma HLS INTERFACE m_axi port=g_graph_node_start offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=g_graph_edge_num offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=g_graph_edges offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=g_graph_frontier_mask offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=g_graph_updating_mask offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=g_graph_visited_mask offset=slave bundle=gmem5
#pragma HLS INTERFACE m_axi port=g_cost offset=slave bundle=gmem6
#pragma HLS INTERFACE s_axilite port=nodes_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	for(int i = 0; i < nodes_num; i++){
		if(g_graph_frontier_mask[i] == 1){ // frontier vertices
			unsigned int start = g_graph_node_start[i];
			unsigned int end   = start + g_graph_edge_num[i];
			for(int i = start; i < end; i++){
            #pragma HLS pipeline
				int id_ = g_graph_edges[i];
				if(g_graph_visited_mask[id_] == 0){
					g_cost[id_] =  level + 1;
					g_graph_updating_mask[id_] = 1;
				}
		 	}
		}
	}
}
}
