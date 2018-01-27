#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define BUFFER_SIZE 16
typedef ap_uint<1> uint1_dt;
typedef ap_int<128> int128_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<32> int32_dt;

// load depth for inspection 
static void read_depth(
		int128_dt *depth, 
		hls::stream<int128_dt> &depth_inspect_stream,
        int inspect_num)
{
	int len;
	int128_dt buffer[BUFFER_SIZE];
	for (int i = 0; i < inspect_num; i += BUFFER_SIZE){
		if(i + BUFFER_SIZE <= inspect_num){
			len = BUFFER_SIZE;
		}
		else{
			len = inspect_num - i;
		}
		for(int j = 0; j < len; j++){
#pragma HLS pipeline
			buffer[j] = depth[i+j];
		}
		for(int j = 0; j < len; j++){
#pragma HLS pipeline
			depth_inspect_stream << buffer[j];
		}
	}
}

// inspect depth for frontier 
static void frontier_inspect(
		hls::stream<int128_dt> &depth_inspect_stream, 
		hls::stream<int> &frontier_stream0,
		hls::stream<int> &frontier_stream1,
		hls::stream<int> &frontier_stream2,
		hls::stream<int> &frontier_stream3,
		int *frontier_size,
		int inspect_num,
		char level)
{
	int128_dt data;
	int vidx[16];
#pragma HLS array_partition variable=vidx complete dim=1
	int count[16];
#pragma HLS array_partition variable=count complete dim=1
    for (int i = 0; i < inspect_num; i++){
#pragma HLS pipeline
		if(i == 0){
			for(int j = 0; j < 16; j++){
				count[j] = 0;
			}
		}

		data = depth_inspect_stream.read();
		for(int j = 0; j < 16; j++){
			char d = data.range((j+1)*8-1, j * 8);
			if(d == level){
				vidx[j] = 16 * i + j;
				count[j]++;
			}
			else{
				vidx[j] = -1;
			}
		}

		for(int j = 0; j < 4; j++){
			frontier_stream0 << vidx[j];
			frontier_stream1 << vidx[4+j];
			frontier_stream2 << vidx[8+j];
			frontier_stream3 << vidx[12+j];
		}

		if(i == inspect_num - 1){
			int sum = 0;
			for(int j = 0; j < 16; j++){
				sum += count[j];
			}
			*frontier_size = sum;
		}
	}
}

// Read rpao of the frontier 
static void read_rpao(
		int *rpao,
		hls::stream<int> &frontier_stream,
		hls::stream<int64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream,
		int inspect_num)
{
	int vidx;
	int64_dt rpitem;
	for(int i = 0; i < inspect_num; i++){
#pragma HLS pipeline
		vidx = frontier_stream.read();
		if(vidx >= 0){
			rpitem.range(31, 0) = rpao[vidx];
			rpitem.range(63, 32) = rpao[vidx+1];
			rpao_stream << rpitem;
		}

		if(i == inspect_num - 1){
			rpao_done_stream << 1;
		}
	}
}

// read ciao
static void read_ciao(
		int *ciao,
		hls::stream<int64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream
		)
{
	uint1_dt rpao_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;
	int start, end;
	int64_dt rpitem;
	while((rpao_empty != 1) || (done != 1)){
		rpao_empty = rpao_stream.empty();
		done_empty = rpao_done_stream.empty();
		
		if(rpao_empty != 1){
			rpitem = rpao_stream.read();
			start = rpitem.range(31, 0);
			end = rpitem.range(63, 32);
			for(int i = start; i < end; i++){
#pragma HLS pipeline
				ciao_stream << ciao[i];
			}
		}

		if(done_empty != 1){
			done = rpao_done_stream.read();
			ciao_done_stream << 1;
		}
	}
}

// update depth
static void update_depth(
		char *depth,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		int level
		)
{
	uint1_dt done = 0;
	uint1_dt ciao_empty = 0;
	uint1_dt done_empty = 0;
	int vidx;
	while((ciao_empty != 1) || (done != 1)){
#pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		
		if(ciao_empty != 1){
			vidx = ciao_stream.read();
			if(depth[vidx] == -1){
				depth[vidx] = level + 1;
			}
		}

		if(done_empty != 1){
			done = ciao_done_stream.read();
		}
	}
}

extern "C" {
void bfs(
		int *rpao0, 
		int *rpao1,
		int *rpao2,
		int *rpao3,
		int *ciao0,
		int *ciao1,
		int *ciao2,
		int *ciao3,
		int128_dt *depth_for_inspect,
		char *depth_for_expand0,	
		char *depth_for_expand1,
		char *depth_for_expand2,
		char *depth_for_expand3,
		int *frontier_size,
		int vertexNum,
		char level
		)
{
#pragma HLS INTERFACE m_axi port=rpao0 offset=slave bundle=gmem00
#pragma HLS INTERFACE m_axi port=rpao1 offset=slave bundle=gmem01
#pragma HLS INTERFACE m_axi port=rpao2 offset=slave bundle=gmem02
#pragma HLS INTERFACE m_axi port=rpao3 offset=slave bundle=gmem03

#pragma HLS INTERFACE m_axi port=ciao0  offset=slave bundle=gmem10
#pragma HLS INTERFACE m_axi port=ciao1  offset=slave bundle=gmem11
#pragma HLS INTERFACE m_axi port=ciao2  offset=slave bundle=gmem12
#pragma HLS INTERFACE m_axi port=ciao3  offset=slave bundle=gmem13

#pragma HLS INTERFACE m_axi port=depth_for_inspect  offset=slave bundle=gmem20

#pragma HLS INTERFACE m_axi port=depth_for_expand0  offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=depth_for_expand1  offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=depth_for_expand2  offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=depth_for_expand3  offset=slave bundle=gmem33
#pragma HLS INTERFACE m_axi port=frontier_size  offset=slave bundle=gmem4

#pragma HLS INTERFACE s_axilite port=vertexNum bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    hls::stream<int128_dt> depth_inspect_stream;

	hls::stream<int> frontier_stream0;
	hls::stream<int> frontier_stream1;
	hls::stream<int> frontier_stream2;
	hls::stream<int> frontier_stream3;

	hls::stream<uint1_dt> inspect_done_stream0;
	hls::stream<uint1_dt> inspect_done_stream1;
	hls::stream<uint1_dt> inspect_done_stream2;
	hls::stream<uint1_dt> inspect_done_stream3;

	hls::stream<int64_dt> rpao_stream0;
	hls::stream<int64_dt> rpao_stream1;
	hls::stream<int64_dt> rpao_stream2;
	hls::stream<int64_dt> rpao_stream3;

	hls::stream<uint1_dt> rpao_done_stream0;
	hls::stream<uint1_dt> rpao_done_stream1;
	hls::stream<uint1_dt> rpao_done_stream2;
	hls::stream<uint1_dt> rpao_done_stream3;

	hls::stream<int> ciao_stream0;
	hls::stream<int> ciao_stream1;
	hls::stream<int> ciao_stream2;
	hls::stream<int> ciao_stream3;

	hls::stream<uint1_dt> ciao_done_stream0;
	hls::stream<uint1_dt> ciao_done_stream1;
	hls::stream<uint1_dt> ciao_done_stream2;
	hls::stream<uint1_dt> ciao_done_stream3;

#pragma HLS STREAM variable=depth_inspect_stream depth=64

#pragma HLS STREAM variable=frontier_stream0 depth=64
#pragma HLS STREAM variable=frontier_stream1 depth=64
#pragma HLS STREAM variable=frontier_stream2 depth=64
#pragma HLS STREAM variable=frontier_stream3 depth=64

#pragma HLS STREAM variable=rpao_stream0 depth=64
#pragma HLS STREAM variable=rpao_stream1 depth=64
#pragma HLS STREAM variable=rpao_stream2 depth=64
#pragma HLS STREAM variable=rpao_stream3 depth=64

#pragma HLS STREAM variable=rpao_done_stream0 depth=4
#pragma HLS STREAM variable=rpao_done_stream1 depth=4
#pragma HLS STREAM variable=rpao_done_stream2 depth=4
#pragma HLS STREAM variable=rpao_done_stream3 depth=4

#pragma HLS STREAM variable=ciao_stream0 depth=64
#pragma HLS STREAM variable=ciao_stream1 depth=64
#pragma HLS STREAM variable=ciao_stream2 depth=64
#pragma HLS STREAM variable=ciao_stream3 depth=64

#pragma HLS STREAM variable=ciao_done_stream0 depth=4
#pragma HLS STREAM variable=ciao_done_stream1 depth=4
#pragma HLS STREAM variable=ciao_done_stream2 depth=4
#pragma HLS STREAM variable=ciao_done_stream3 depth=4

	int block = vertexNum/16;
#pragma HLS dataflow
    read_depth(depth_for_inspect, depth_inspect_stream, block);
	frontier_inspect(depth_inspect_stream, 
			frontier_stream0, 
			frontier_stream1,
			frontier_stream2,
			frontier_stream3,
			frontier_size,
			block, 
			level);

	read_rpao(rpao0, frontier_stream0, rpao_stream0, rpao_done_stream0, block*4);
	read_rpao(rpao1, frontier_stream1, rpao_stream1, rpao_done_stream1, block*4);
	read_rpao(rpao2, frontier_stream2, rpao_stream2, rpao_done_stream2, block*4);
	read_rpao(rpao3, frontier_stream3, rpao_stream3, rpao_done_stream3, block*4);

	read_ciao(ciao0, rpao_stream0, rpao_done_stream0, ciao_stream0, ciao_done_stream0);
	read_ciao(ciao1, rpao_stream1, rpao_done_stream1, ciao_stream1, ciao_done_stream1);
	read_ciao(ciao2, rpao_stream2, rpao_done_stream2, ciao_stream2, ciao_done_stream2);
	read_ciao(ciao3, rpao_stream3, rpao_done_stream3, ciao_stream3, ciao_done_stream3);

    update_depth(depth_for_expand0, ciao_stream0, ciao_done_stream0, level);
    update_depth(depth_for_expand1, ciao_stream1, ciao_done_stream1, level);
    update_depth(depth_for_expand2, ciao_stream2, ciao_done_stream2, level);
    update_depth(depth_for_expand3, ciao_stream3, ciao_done_stream3, level);

}
}

