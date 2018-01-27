#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

typedef ap_uint<1> uint1_dt;
typedef ap_int<16> int16_dt;
typedef ap_int<64> int64_dt;

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

// inspect depth for frontier 
static void bfs_stage1(
		const int *rpao,
		const int *ciao,
		const char *depth,
		int *frontier_size,
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		const int vertex_num,
		const int level)
{
	char d;
	int ngb;
	int counter = 0;
    inspect: for (int i = 0; i < vertex_num; i++){
#pragma HLS pipeline
		d = depth_inspect_stream.read();
		if(d == level){
			counter++;
			for(int j = rpao[i]; j < rpao[i+1]; j++){
				ngb = ciao[j];
				if(depth[ngb] == -1){
					ngb_stream << ngb;
				}
			}
		}

		if(i == vertex_num - 1){
			ngb_done_stream << 1;
			*frontier_size = counter;
		}
	}
}

// load depth for inspection 
static void bfs_stage2(
		char *depth,
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		const char level_plus1)
{
	int vidx;
	int done = 0;
	int ngb_empty = 0;
	int done_empty = 0;
    while(ngb_empty != 1 || done != 1){
        #pragma HLS pipeline
		ngb_empty = ngb_stream.empty();
		done_empty = ngb_done_stream.empty();
		if(ngb_empty != 1){
			vidx = ngb_stream.read();
			depth[vidx] = level_plus1;
		}
		else{
			if(done_empty != 1){
				done = ngb_done_stream.read();
			}
		}
    }
}


extern "C" {
void bfs(
		const char *depth_for_inspect,
		const char *depth_for_expand,
		char *depth_for_update,
		const int *rpao, 
		const int *ciao,
		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_for_expand offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=rpao offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=ciao offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem5
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

char level_plus1 = level + 1;

hls::stream<char> depth_inspect_stream;
hls::stream<int> ngb_stream;
hls::stream<uint1_dt> ngb_done_stream;


#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=ngb_stream depth=64
#pragma HLS STREAM variable=ngb_done_stream depth=4

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, vertex_num);
bfs_stage1(rpao, ciao, depth_for_expand, frontier_size, 
		depth_inspect_stream, ngb_stream, ngb_done_stream, vertex_num, level);
bfs_stage2(depth_for_update, ngb_stream, ngb_done_stream, level_plus1);
}
}

