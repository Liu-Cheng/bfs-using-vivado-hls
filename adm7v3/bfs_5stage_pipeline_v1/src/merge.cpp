/**
 * File              : src/merge.cpp
 * Author            : Cheng Liu <st.liucheng@gmail.com>
 * Date              : 10.12.2017
 * Last Modified Date: 18.12.2017
 * Last Modified By  : Cheng Liu <st.liucheng@gmail.com>
 */
#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

typedef ap_uint<1> uint1_dt;
typedef ap_int<64> int64_dt;

// load depth for inspection 
static void load_depth(
		const char *depth0, 
		const char *depth1, 
		const char *depth2, 
		const char *depth3, 
		hls::stream<char> &depth_stream0,
		hls::stream<char> &depth_stream1,
		hls::stream<char> &depth_stream2,
		hls::stream<char> &depth_stream3,
        int vertex_num
		)
{
	for (int i = 0; i < vertex_num; i++){
        #pragma HLS pipeline
			depth_stream0 << depth0[i];
			depth_stream1 << depth1[i];
			depth_stream2 << depth2[i];
			depth_stream3 << depth3[i];
    }
}

// inspect depth for frontier 
static void merge_depth(
		char *depth_update,
		hls::stream<char> &depth_stream0, 
		hls::stream<char> &depth_stream1, 
		hls::stream<char> &depth_stream2, 
		hls::stream<char> &depth_stream3, 
		int *frontier_size,
		const int vertex_num,
		const char level_plus1
		)
{
	int counter = 0;
	printf("Level=%d, frontier:", (int)level_plus1);
	for (int i = 0; i < vertex_num; i++){
    #pragma HLS pipeline
		char d0 = depth_stream0.read();
		char d1 = depth_stream1.read();
		char d2 = depth_stream2.read();
		char d3 = depth_stream3.read();
		if(d0 == level_plus1 || d1 == level_plus1 || d2 == level_plus1 || d3 == level_plus1){
			depth_update[i] = level_plus1;
			counter++;
		}
	}
	printf("\n");
	printf("Level = %d, counter=%d\n",(int)level_plus1, counter);
	*frontier_size = counter;
}


extern "C" {
void merge(
		char *depth_update,
		const char *depth0,
		const char *depth1,
		const char *depth2,
		const char *depth3,
		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_update offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth0 offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=depth1 offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=depth2 offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=depth3 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem5
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

hls::stream<char> depth_stream0;
hls::stream<char> depth_stream1;
hls::stream<char> depth_stream2;
hls::stream<char> depth_stream3;

#pragma HLS STREAM variable=depth_stream0 depth=32
#pragma HLS STREAM variable=depth_stream1 depth=32
#pragma HLS STREAM variable=depth_stream2 depth=32
#pragma HLS STREAM variable=depth_stream3 depth=32

char level_plus1 = level + 1;

#pragma HLS dataflow
load_depth(depth0, depth1, depth2, depth3, 
		depth_stream0, depth_stream1, 
		depth_stream2, depth_stream3, 
		vertex_num);

merge_depth(depth_update, depth_stream0, depth_stream1, 
		depth_stream2, depth_stream3, frontier_size, 
		vertex_num, level_plus1);

}
}

