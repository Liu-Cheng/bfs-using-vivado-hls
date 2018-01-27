#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define BUFFER_SIZE 32
typedef ap_uint<1> uint1_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<32> int32_dt;

// load depth for inspection 
static void read_depth(
		int32_dt *depth, 
		hls::stream<char> &depth_inspect_stream0,
		hls::stream<char> &depth_inspect_stream1,
		hls::stream<char> &depth_inspect_stream2,
		hls::stream<char> &depth_inspect_stream3,
        int inspect_num)
{
	int32_dt buffer[BUFFER_SIZE];
	for (int i = 0; i < inspect_num; i += BUFFER_SIZE){
		for(int j = 0; j < BUFFER_SIZE; j++){
#pragma HLS pipeline
			buffer[j] = depth[i+j];
			depth_inspect_stream0 << buffer[j].range(7, 0);
			depth_inspect_stream1 << buffer[j].range(15, 8);
			depth_inspect_stream2 << buffer[j].range(23, 16);
			depth_inspect_stream3 << buffer[j].range(31, 24);
		}
	}
}

// inspect depth for frontier 
static void frontier_inspect(
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &size_stream,
		int inspect_num,
		int lane,
		char level)
{
	char d;
	int count = 0;
    inspect: for (int i = 0; i < inspect_num; i++){
#pragma HLS pipeline
		d = depth_inspect_stream.read();
		if(d == level){
			count++;
			frontier_stream << ((i << 2) + lane);
		}

		if(i == inspect_num - 1){
			size_stream << count;
			inspect_done_stream << 1;
		}
	}
}

static void calculate_size(
		int *frontier_size,
		hls::stream<int> &size_stream0,
		hls::stream<int> &size_stream1,
		hls::stream<int> &size_stream2,
		hls::stream<int> &size_stream3
		){
	int c0 = size_stream0.read();
	int c1 = size_stream1.read();
	int c2 = size_stream2.read();
	int c3 = size_stream3.read();
	*frontier_size = c0 + c1 + c2 + c3;
}

static void read_rpao(
		const int *rpao,
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream)
{
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;
	int vidx;
	int64_dt rpitem;
	while((frontier_empty != 1) || (done != 1)){
#pragma HLS pipeline
		frontier_empty = frontier_stream.empty();
	    done_empty = inspect_done_stream.empty();

		if(frontier_empty != 1){
			vidx = frontier_stream.read();
			rpitem.range(31, 0) = rpao[vidx];
			rpitem.range(63, 32) = rpao[vidx+1];
			rpao_stream << rpitem;
		}
		else{
			if(done_empty != 1){
				done = inspect_done_stream.read();
				rpao_done_stream << 1;
			}
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
		int32_dt *depth_for_inspect,
		char *depth_for_expand0,	
		char *depth_for_expand1,
		char *depth_for_expand2,
		char *depth_for_expand3,
		int *frontier_size,
		int vertex_num,
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

#pragma HLS INTERFACE m_axi port=frontier_size  offset=slave bundle=gmem40

#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    hls::stream<char> depth_inspect_stream0;
    hls::stream<char> depth_inspect_stream1;
    hls::stream<char> depth_inspect_stream2;
    hls::stream<char> depth_inspect_stream3;

	hls::stream<int> frontier_stream0;
	hls::stream<int> frontier_stream1;
	hls::stream<int> frontier_stream2;
	hls::stream<int> frontier_stream3;

	hls::stream<int> size_stream0;
	hls::stream<int> size_stream1;
	hls::stream<int> size_stream2;
	hls::stream<int> size_stream3;

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

#pragma HLS STREAM variable=depth_inspect_stream0 depth=32
#pragma HLS STREAM variable=depth_inspect_stream1 depth=32
#pragma HLS STREAM variable=depth_inspect_stream2 depth=32
#pragma HLS STREAM variable=depth_inspect_stream3 depth=32

#pragma HLS STREAM variable=inspect_done_stream0 depth=2
#pragma HLS STREAM variable=inspect_done_stream1 depth=2
#pragma HLS STREAM variable=inspect_done_stream2 depth=2
#pragma HLS STREAM variable=inspect_done_stream3 depth=2

#pragma HLS STREAM variable=frontier_stream0 depth=32
#pragma HLS STREAM variable=frontier_stream1 depth=32
#pragma HLS STREAM variable=frontier_stream2 depth=32
#pragma HLS STREAM variable=frontier_stream3 depth=32

#pragma HLS STREAM variable=size_stream0 depth=2
#pragma HLS STREAM variable=size_stream1 depth=2
#pragma HLS STREAM variable=size_stream2 depth=2
#pragma HLS STREAM variable=size_stream3 depth=2

#pragma HLS STREAM variable=rpao_stream0 depth=32
#pragma HLS STREAM variable=rpao_stream1 depth=32
#pragma HLS STREAM variable=rpao_stream2 depth=32
#pragma HLS STREAM variable=rpao_stream3 depth=32

#pragma HLS STREAM variable=rpao_done_stream0 depth=2
#pragma HLS STREAM variable=rpao_done_stream1 depth=2
#pragma HLS STREAM variable=rpao_done_stream2 depth=2
#pragma HLS STREAM variable=rpao_done_stream3 depth=2

#pragma HLS STREAM variable=ciao_stream0 depth=64
#pragma HLS STREAM variable=ciao_stream1 depth=64
#pragma HLS STREAM variable=ciao_stream2 depth=64
#pragma HLS STREAM variable=ciao_stream3 depth=64

#pragma HLS STREAM variable=ciao_done_stream0 depth=2
#pragma HLS STREAM variable=ciao_done_stream1 depth=2
#pragma HLS STREAM variable=ciao_done_stream2 depth=2
#pragma HLS STREAM variable=ciao_done_stream3 depth=2

	int inspect_num = vertex_num/4;

#pragma HLS dataflow
    read_depth(depth_for_inspect, depth_inspect_stream0, 
			depth_inspect_stream1, depth_inspect_stream2, 
			depth_inspect_stream3, inspect_num);

	frontier_inspect(depth_inspect_stream0, frontier_stream0, 
			inspect_done_stream0, size_stream0, inspect_num, 0, level);
	frontier_inspect(depth_inspect_stream1, frontier_stream1, 
			inspect_done_stream1, size_stream1, inspect_num, 1, level);
	frontier_inspect(depth_inspect_stream2, frontier_stream2, 
			inspect_done_stream2, size_stream2, inspect_num, 2, level);
	frontier_inspect(depth_inspect_stream3, frontier_stream3, 
			inspect_done_stream3, size_stream3, inspect_num, 3, level);

	calculate_size(frontier_size, size_stream0, size_stream1, size_stream2, size_stream3);

	read_rpao(rpao0, frontier_stream0, inspect_done_stream0, rpao_stream0, rpao_done_stream0);
	read_rpao(rpao1, frontier_stream1, inspect_done_stream1, rpao_stream1, rpao_done_stream1);
	read_rpao(rpao2, frontier_stream2, inspect_done_stream2, rpao_stream2, rpao_done_stream2);
	read_rpao(rpao3, frontier_stream3, inspect_done_stream3, rpao_stream3, rpao_done_stream3);

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

