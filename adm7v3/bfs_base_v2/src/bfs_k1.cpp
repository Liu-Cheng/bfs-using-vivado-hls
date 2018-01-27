extern "C" {
void bfs_k1(
		char* g_graph_frontier_mask,
		char* g_graph_updating_mask,
		char* g_graph_visited_mask,
		int*  frontier_size,
		const unsigned int vertex_num)
{
#pragma HLS INTERFACE m_axi port=g_graph_frontier_mask offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=g_graph_updating_mask offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=g_graph_visited_mask offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem3
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int count = 0;
	for(int i = 0; i < vertex_num; i++){
    #pragma HLS pipeline
		if(g_graph_updating_mask[i] == 1){
			g_graph_frontier_mask[i] = 1;
			g_graph_visited_mask[i] = 1;
			g_graph_updating_mask[i] = 0;
			count++;
		}	
	}
	*frontier_size = count;
}
}
