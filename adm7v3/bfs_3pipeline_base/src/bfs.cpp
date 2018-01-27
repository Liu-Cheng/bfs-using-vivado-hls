#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

typedef ap_uint<1> uint1_dt;
typedef ap_int<40> int40_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<16> int16_dt;
typedef ap_int<8> int8_dt;

// load depth for inspection 
static void read_depth(
		const char *depth, 
		hls::stream<char> &depth_inspect_stream,
		const int startIdx,
        const int blockSize)
{
    for (int i = 0; i < blockSize; i++){
#pragma HLS pipeline
        depth_inspect_stream << depth[startIdx + i];
    }
}

// inspect depth for frontier 
static void frontier_inspect(
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontier_stream,
		hls::stream<int> &size_stream,
		const int startIdx,
		const int blockSize,
		const char level)
{
	char data; 
	int count = 0;
    inspect: for (int i = 0; i < blockSize; i++){
#pragma HLS pipeline
		data = depth_inspect_stream.read();
		if(data == level){
			frontier_stream << (startIdx + i);
			count++;
		}

		if(i == (blockSize - 1)){
			inspect_done_stream << 1;
			size_stream << count;
		}
	}
}

// Read rpao of the frontier 
static void read_rpao(
		const int *rpao,
		int *frontierSize,
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream,
		hls::stream<int> &size_stream)
{
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;
	int vidx;
	int rpidx;
	int64_dt rpitem;
	int size;
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

		if(done_empty != 1 && frontier_empty == 1){
			done = inspect_done_stream.read();
			rpao_done_stream << 1;
		}
		if(size_stream.empty() != 1){
			size = size_stream.read();
			*frontierSize = size;
		}
	}
}

// read ciao
static void read_ciao(
		const int *ciao,
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

		if(done_empty != 1 && rpao_empty == 1){
			done = rpao_done_stream.read();
			ciao_done_stream << 1;
		}
	}
}

static void get_ngb(
		const char *depth,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int40_dt> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream
		){
	uint1_dt done = 0;
	uint1_dt ciao_empty = 0;
	uint1_dt done_empty = 0;
	int vidx;
	char d;
	int8_dt delay_counter = 0;
	int40_dt item;
	while((ciao_empty != 1) || (done != 1)){
#pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			vidx = ciao_stream.read();
			d = depth[vidx];
			item.range(39, 32) = d;
			item.range(31, 0) = vidx;
			ngb_stream << item;
		}

		if(done_empty != 1 && ciao_empty == 1){
			delay_counter++;
			if(delay_counter == 100){
				done = ciao_done_stream.read();
				ngb_done_stream << 1;
			}
		}
	}
}

static void select_frontier(
		hls::stream<int40_dt> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		hls::stream<int> &next_frontier_stream,
		hls::stream<uint1_dt> &next_frontier_done_stream
		){
	uint1_dt done = 0;
	uint1_dt ngb_empty = 0;
	uint1_dt done_empty = 0;
	int vidx;
	char d;
	int40_dt item;
	while((ngb_empty != 1) || (done != 1)){
#pragma HLS pipeline
		ngb_empty = ngb_stream.empty();
		done_empty = ngb_done_stream.empty();
		if(ngb_empty != 1){
			item = ngb_stream.read();
			d = item.range(39, 32);
			vidx = item.range(31, 0);
			if(d == -1){
				next_frontier_stream << vidx;
			}
		}
		if(done_empty != 1 && ngb_empty == 1){
			done = ngb_done_stream.read();
			next_frontier_done_stream << 1;
		}
	}
}
// load depth for inspection 
static void update_depth(
		char *depth,
		hls::stream<int> &next_frontier_stream,
		hls::stream<uint1_dt> &next_frontier_done_stream,
		const char level_plus1)
{
	uint1_dt done = 0;
	uint1_dt next_frontier_empty = 0;
	uint1_dt done_empty = 0;
	int vidx;
	while((next_frontier_empty != 1) || (done != 1)){
#pragma HLS pipeline
		next_frontier_empty = next_frontier_stream.empty();
		done_empty = next_frontier_done_stream.empty();
		if(next_frontier_empty != 1){
			vidx = next_frontier_stream.read();
			depth[vidx] = level_plus1;
		}
		if(done_empty != 1 && next_frontier_empty == 1){
			done = next_frontier_done_stream.read();
		}
	}
}



extern "C" {
void bfs(
		const char *depth_for_inspect0,
		const char *depth_for_inspect1,
		const char *depth_for_inspect2,
		int *frontierSize0,
		int *frontierSize1,
		int *frontierSize2,
		const int *rpao0,
		const int *rpao1,	
		const int *rpao2,	
		const int *ciao0,
		const int *ciao1,
		const int *ciao2,
		const char *depth_for_expand0,
		const char *depth_for_expand1,
		const char *depth_for_expand2,
		char *depth_for_update0,
		char *depth_for_update1,
		char *depth_for_update2,
		const int vertexNum,
		char level
		)
{
#pragma HLS INTERFACE m_axi port=rpao0 offset=slave bundle=gmem00
#pragma HLS INTERFACE m_axi port=rpao1 offset=slave bundle=gmem01
#pragma HLS INTERFACE m_axi port=rpao2 offset=slave bundle=gmem02
#pragma HLS INTERFACE m_axi port=ciao0 offset=slave bundle=gmem10
#pragma HLS INTERFACE m_axi port=ciao1 offset=slave bundle=gmem11
#pragma HLS INTERFACE m_axi port=ciao2 offset=slave bundle=gmem12
#pragma HLS INTERFACE m_axi port=depth_for_inspect0 offset=slave bundle=gmem20
#pragma HLS INTERFACE m_axi port=depth_for_inspect1 offset=slave bundle=gmem21
#pragma HLS INTERFACE m_axi port=depth_for_inspect2 offset=slave bundle=gmem22
#pragma HLS INTERFACE m_axi port=depth_for_expand0 offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=depth_for_expand1 offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=depth_for_expand2 offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=depth_for_update0 offset=slave bundle=gmem40
#pragma HLS INTERFACE m_axi port=depth_for_update1 offset=slave bundle=gmem41
#pragma HLS INTERFACE m_axi port=depth_for_update2 offset=slave bundle=gmem42
#pragma HLS INTERFACE m_axi port=frontierSize0 offset=slave bundle=gmem00
#pragma HLS INTERFACE m_axi port=frontierSize1 offset=slave bundle=gmem01
#pragma HLS INTERFACE m_axi port=frontierSize2 offset=slave bundle=gmem02

#pragma HLS INTERFACE s_axilite port=vertexNum bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	hls::stream<char> depth_inspect_stream0;
	hls::stream<char> depth_inspect_stream1;
	hls::stream<char> depth_inspect_stream2;
	hls::stream<int> frontier_stream0;
	hls::stream<int> frontier_stream1;
	hls::stream<int> frontier_stream2;
	hls::stream<uint1_dt> inspect_done_stream0;
	hls::stream<uint1_dt> inspect_done_stream1;
	hls::stream<uint1_dt> inspect_done_stream2;
	hls::stream<int64_dt> rpao_stream0;
	hls::stream<int64_dt> rpao_stream1;
	hls::stream<int64_dt> rpao_stream2;
	hls::stream<uint1_dt> rpao_done_stream0;
	hls::stream<uint1_dt> rpao_done_stream1;
	hls::stream<uint1_dt> rpao_done_stream2;
	hls::stream<int> ciao_stream0;
	hls::stream<int> ciao_stream1;
	hls::stream<int> ciao_stream2;
	hls::stream<uint1_dt> ciao_done_stream0;
	hls::stream<uint1_dt> ciao_done_stream1;
	hls::stream<uint1_dt> ciao_done_stream2;
	hls::stream<int40_dt> ngb_stream0;
	hls::stream<int40_dt> ngb_stream1;
	hls::stream<int40_dt> ngb_stream2;
	hls::stream<uint1_dt> ngb_done_stream0;
	hls::stream<uint1_dt> ngb_done_stream1;
	hls::stream<uint1_dt> ngb_done_stream2;
	hls::stream<int> size_stream0;
	hls::stream<int> size_stream1;
	hls::stream<int> size_stream2;
	hls::stream<int> next_frontier_stream0;
	hls::stream<int> next_frontier_stream1;
	hls::stream<int> next_frontier_stream2;
	hls::stream<uint1_dt> next_frontier_done_stream0;
	hls::stream<uint1_dt> next_frontier_done_stream1;
	hls::stream<uint1_dt> next_frontier_done_stream2;

	char level_plus1 = level + 1;

#pragma HLS STREAM variable=depth_inspect_stream0 depth=32
#pragma HLS STREAM variable=depth_inspect_stream1 depth=32
#pragma HLS STREAM variable=depth_inspect_stream2 depth=32
#pragma HLS STREAM variable=inspect_done_stream0 depth=4
#pragma HLS STREAM variable=inspect_done_stream1 depth=4
#pragma HLS STREAM variable=inspect_done_stream2 depth=4
#pragma HLS STREAM variable=frontier_stream0 depth=32
#pragma HLS STREAM variable=frontier_stream1 depth=32
#pragma HLS STREAM variable=frontier_stream2 depth=32
#pragma HLS STREAM variable=rpao_stream0 depth=32
#pragma HLS STREAM variable=rpao_stream1 depth=32
#pragma HLS STREAM variable=rpao_stream2 depth=32
#pragma HLS STREAM variable=rpao_done_stream0 depth=4
#pragma HLS STREAM variable=rpao_done_stream1 depth=4
#pragma HLS STREAM variable=rpao_done_stream2 depth=4
#pragma HLS STREAM variable=ciao_stream0 depth=64
#pragma HLS STREAM variable=ciao_stream1 depth=64
#pragma HLS STREAM variable=ciao_stream2 depth=64
#pragma HLS STREAM variable=ciao_done_stream0 depth=4
#pragma HLS STREAM variable=ciao_done_stream1 depth=4
#pragma HLS STREAM variable=ciao_done_stream2 depth=4
#pragma HLS STREAM variable=ngb_stream0 depth=256
#pragma HLS STREAM variable=ngb_stream1 depth=256
#pragma HLS STREAM variable=ngb_stream2 depth=256
#pragma HLS STREAM variable=ngb_done_stream0 depth=4
#pragma HLS STREAM variable=ngb_done_stream1 depth=4
#pragma HLS STREAM variable=ngb_done_stream2 depth=4
#pragma HLS STREAM variable=size_stream0 depth=4
#pragma HLS STREAM variable=size_stream1 depth=4
#pragma HLS STREAM variable=size_stream2 depth=4
#pragma HLS STREAM variable=next_frontier_stream0 depth=256
#pragma HLS STREAM variable=next_frontier_stream1 depth=256
#pragma HLS STREAM variable=next_frontier_stream2 depth=256
#pragma HLS STREAM variable=next_frontier_done_stream0 depth=4
#pragma HLS STREAM variable=next_frontier_done_stream1 depth=4
#pragma HLS STREAM variable=next_frontier_done_stream2 depth=4

	int block = vertexNum/3;
	int last_block = vertexNum - 2*block;
#pragma HLS dataflow
    read_depth(depth_for_inspect0, depth_inspect_stream0, 0, block);
    read_depth(depth_for_inspect1, depth_inspect_stream1, block, block);
    read_depth(depth_for_inspect2, depth_inspect_stream2, 2*block, last_block);

	frontier_inspect(depth_inspect_stream0, inspect_done_stream0, 
			frontier_stream0, size_stream0,  0, block, level);
	frontier_inspect(depth_inspect_stream1, inspect_done_stream1, 
			frontier_stream1, size_stream1, block, block, level);
	frontier_inspect(depth_inspect_stream2, inspect_done_stream2, 
			frontier_stream2, size_stream2, 2*block, last_block, level);

	read_rpao(rpao0, &frontierSize0[0], frontier_stream0, 
			inspect_done_stream0, rpao_stream0, rpao_done_stream0, size_stream0);
	read_rpao(rpao1, &frontierSize1[1], frontier_stream1, 
			inspect_done_stream1, rpao_stream1, rpao_done_stream1, size_stream1);
	read_rpao(rpao2, &frontierSize2[2], frontier_stream2, 
			inspect_done_stream2, rpao_stream2, rpao_done_stream2, size_stream2);

	read_ciao(ciao0, rpao_stream0, rpao_done_stream0, ciao_stream0, ciao_done_stream0);
	read_ciao(ciao1, rpao_stream1, rpao_done_stream1, ciao_stream1, ciao_done_stream1);
	read_ciao(ciao2, rpao_stream2, rpao_done_stream2, ciao_stream2, ciao_done_stream2);

	get_ngb(depth_for_expand0, ciao_stream0, ciao_done_stream0, ngb_stream0, ngb_done_stream0);
	get_ngb(depth_for_expand1, ciao_stream1, ciao_done_stream1, ngb_stream1, ngb_done_stream1);
	get_ngb(depth_for_expand2, ciao_stream2, ciao_done_stream2, ngb_stream2, ngb_done_stream2);
	select_frontier(ngb_stream0, ngb_done_stream0, next_frontier_stream0, next_frontier_done_stream0);
	select_frontier(ngb_stream1, ngb_done_stream1, next_frontier_stream1, next_frontier_done_stream1);
	select_frontier(ngb_stream2, ngb_done_stream2, next_frontier_stream2, next_frontier_done_stream2);

	update_depth(depth_for_update0, next_frontier_stream0, next_frontier_done_stream0, level_plus1);
	update_depth(depth_for_update1, next_frontier_stream1, next_frontier_done_stream1, level_plus1);
	update_depth(depth_for_update2, next_frontier_stream2, next_frontier_done_stream2, level_plus1);
}
}

