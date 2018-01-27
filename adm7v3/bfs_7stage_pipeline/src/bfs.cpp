#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

typedef ap_uint<1> uint1_dt;
typedef ap_int<8> int8_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<40> int40_dt;

// load depth for inspection 
static void read_depth(
		const char *depth, 
		hls::stream<char> &depth_inspect_stream,
        const int vertex_num)
{
    read_depth: for (int i = 0; i < vertex_num; i++){
#pragma HLS pipeline
        depth_inspect_stream << depth[i];
    }
}

// inspect depth for frontier 
static void frontier_inspect(
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontier_stream,
		const int vertex_num,
		int *frontier_size,
		const char level)
{
	char data; 
	int count = 0;
    inspect: for (int i = 0; i < vertex_num; i++){
#pragma HLS pipeline
		data = depth_inspect_stream.read();
		if(data == level){
			frontier_stream << i;
			count++;
		}

		if(i == vertex_num - 1){
			inspect_done_stream << 1;
			*frontier_size = count;
		}
	}
}

// Read rpao of the frontier 
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

		if(done_empty != 1 && frontier_empty == 1){
			done = inspect_done_stream.read();
			rpao_done_stream << 1;
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

static void get_ngb_depth(
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
	int40_dt item;
	int8_dt delay_counter = 0;
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

// update depth
static void update_depth(
		char *depth,
		hls::stream<int> &next_frontier_stream,
		hls::stream<uint1_dt> &next_frontier_done_stream,
		int level_plus1
		)
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
		const char *depth_for_inspect,
		int *frontier_size,
		const int *rpao, 
		const int *ciao,
		const char *depth_for_expand,
		char *depth_for_update,
		const int vertex_num,
		char level
		)
{
#pragma HLS INTERFACE m_axi port=rpao offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=ciao offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=depth_for_expand offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem5
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control


	hls::stream<char> depth_inspect_stream;
	hls::stream<int> frontier_stream;
	hls::stream<uint1_dt> inspect_done_stream;
	hls::stream<int64_dt> rpao_stream;
	hls::stream<uint1_dt> rpao_done_stream;
	hls::stream<int> ciao_stream;
	hls::stream<uint1_dt> ciao_done_stream;
	hls::stream<int40_dt> ngb_stream;
	hls::stream<uint1_dt> ngb_done_stream;
	hls::stream<int> next_frontier_stream;
	hls::stream<uint1_dt> next_frontier_done_stream;

	char level_plus1 = level + 1;

#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=frontier_stream depth=32
#pragma HLS STREAM variable=rpao_stream depth=32
#pragma HLS STREAM variable=rpao_done_stream depth=4
#pragma HLS STREAM variable=ciao_stream depth=32
#pragma HLS STREAM variable=ciao_done_stream depth=4
#pragma HLS STREAM variable=ngb_stream depth=32
#pragma HLS STREAM variable=ngb_done_stream depth=4
#pragma HLS STREAM variable=next_frontier_stream depth=32
#pragma HLS STREAM variable=next_frontier_done_stream depth=4

#pragma HLS dataflow
    read_depth(depth_for_inspect, depth_inspect_stream, vertex_num);
	frontier_inspect(depth_inspect_stream, inspect_done_stream, frontier_stream, vertex_num, frontier_size, level);
	read_rpao(rpao, frontier_stream, inspect_done_stream, rpao_stream, rpao_done_stream);
	read_ciao(ciao, rpao_stream, rpao_done_stream, ciao_stream, ciao_done_stream);
	get_ngb_depth(depth_for_expand, ciao_stream, ciao_done_stream, ngb_stream, ngb_done_stream);
	select_frontier(ngb_stream, ngb_done_stream, next_frontier_stream, next_frontier_done_stream);
    update_depth(depth_for_update, next_frontier_stream, next_frontier_done_stream, level_plus1);

}
}

