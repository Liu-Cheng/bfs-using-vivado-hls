#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

// load depth for inspection 
static void bfs_stage0(
		const char *depth_for_inspect, 
		hls::stream<char> &depth_inspect_stream,
        int vertex_num)
{
    read_depth: for (int i = 0; i < vertex_num; i++){
    #pragma HLS pipeline
        depth_inspect_stream << depth_for_inspect[i];
    }
}


// load depth for inspection 
static void bfs_stage1(
		char *depth,
		hls::stream<char> &depth_inspect_stream,
		const int *rpao,
		const int *ciao,
		int *frontier_size,
        int vertex_num,
		const char level,
		const char level_plus1)
{
	char d;
	int start, end;
	int ngb_vidx;
	char ngb_depth;
	int counter = 0;
    for (int i = 0; i < vertex_num; i++){
        d = depth_inspect_stream.read();
		if(d == level){
			counter++;
			start = rpao[i];
			end = rpao[i+1];
			for(int j = start; j < end; j++){
            #pragma HLS pipeline
				ngb_vidx = ciao[j];
				ngb_depth = depth[ngb_vidx];
				if(ngb_depth == -1){
					depth[ngb_vidx] = level_plus1;
				}
			}
		}

		if(i == vertex_num - 1){
			*frontier_size = counter;
		}
    }
}


extern "C" {
void bfs(
		const char *depth_for_inspect,
		char *depth_for_update,
		const int *rpao, 
		const int *ciao,
		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=rpao offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=ciao offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem4
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

char level_plus1 = level + 1;

hls::stream<char> depth_inspect_stream;
#pragma HLS STREAM variable=depth_inspect_stream depth=32

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, vertex_num);
bfs_stage1(depth_for_update, depth_inspect_stream, rpao, ciao, frontier_size, vertex_num, level, level_plus1);

}
}

