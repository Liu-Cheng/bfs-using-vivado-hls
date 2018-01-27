#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define OFFSET 1024
#define STRIDE 2048

typedef ap_uint<1> uint1_dt;
typedef ap_int<8> int8_dt;
typedef ap_int<40> int40_dt;
typedef ap_int<64> int64_dt;

// load depth for inspection 
static void read_depth(
		const char *depth, 
		hls::stream<char> &depth_inspect_stream,
		const int start_idx,
        const int vertex_num)
{
    for (int i = start_idx; i < vertex_num; i += STRIDE){
		for(int j = 0; j < OFFSET; j++){
        #pragma HLS pipeline
			depth_inspect_stream << depth[i + j];
		}
    }
}

// inspect depth for frontier 
static void frontier_inspect(
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontier_stream,
		const int start_idx,
		const int vertex_num,
		int *frontier_size,
		const char level)
{
	char data; 
	int count = 0;
	for (int i = start_idx; i < vertex_num; i += STRIDE){
		for(int j = 0; j < OFFSET; j++){
        #pragma HLS pipeline
			data = depth_inspect_stream.read();
			if(data == level){
				frontier_stream << (i + j);
				count++;
			}
		}
		
		if(i + STRIDE >= vertex_num){
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
	int rpidx;
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
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream
		){
	uint1_dt done = 0;
	uint1_dt ciao_empty = 0;
	uint1_dt done_empty = 0;
	int vidx;
	char d;
	int8_dt delay_counter = 0;
	while((ciao_empty != 1) || (done != 1)){
#pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			vidx = ciao_stream.read();
			d = depth[vidx];
			if(d == -1){
				ngb_stream << vidx;
			}
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



// update depth
static void update_depth(
		char *depth,
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		int level_plus1
		)
{
	uint1_dt done = 0;
	uint1_dt ngb_empty = 0;
	uint1_dt done_empty = 0;
	int vidx;
	while((ngb_empty != 1) || (done != 1)){
#pragma HLS pipeline
		ngb_empty = ngb_stream.empty();
		done_empty = ngb_done_stream.empty();
		
		if(ngb_empty != 1){
			vidx = ngb_stream.read();
			depth[vidx] = level_plus1;
		}

		if(done_empty != 1 && ngb_empty == 1){
			done = ngb_done_stream.read();
		}
	}
}

extern "C" {
void bfs(
		const char *depth_for_inspect0,
		const char *depth_for_inspect1,
		int *frontier_size0,
		int *frontier_size1,	
		const int *rpao0,
		const int *rpao1,	
		const int *ciao0,
		const int *ciao1,
		const char *depth_for_expand0,
		const char *depth_for_expand1,
		char *depth_for_update0,
		char *depth_for_update1,
		const int vertex_num,
		char level
		)
{
#pragma HLS INTERFACE m_axi port=rpao0 offset=slave bundle=gmem00
#pragma HLS INTERFACE m_axi port=rpao1 offset=slave bundle=gmem01
#pragma HLS INTERFACE m_axi port=ciao0 offset=slave bundle=gmem10
#pragma HLS INTERFACE m_axi port=ciao1 offset=slave bundle=gmem11
#pragma HLS INTERFACE m_axi port=depth_for_inspect0 offset=slave bundle=gmem20
#pragma HLS INTERFACE m_axi port=depth_for_inspect1 offset=slave bundle=gmem21
#pragma HLS INTERFACE m_axi port=depth_for_expand0 offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=depth_for_expand1 offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=depth_for_update0 offset=slave bundle=gmem40
#pragma HLS INTERFACE m_axi port=depth_for_update1 offset=slave bundle=gmem41
#pragma HLS INTERFACE m_axi port=frontier_size0 offset=slave bundle=gmem50
#pragma HLS INTERFACE m_axi port=frontier_size1 offset=slave bundle=gmem51
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	hls::stream<char> depth_inspect_stream0;
	hls::stream<char> depth_inspect_stream1;
	hls::stream<int> frontier_stream0;
	hls::stream<int> frontier_stream1;
	hls::stream<uint1_dt> inspect_done_stream0;
	hls::stream<uint1_dt> inspect_done_stream1;
	hls::stream<int64_dt> rpao_stream0;
	hls::stream<int64_dt> rpao_stream1;
	hls::stream<uint1_dt> rpao_done_stream0;
	hls::stream<uint1_dt> rpao_done_stream1;
	hls::stream<int> ciao_stream0;
	hls::stream<int> ciao_stream1;
	hls::stream<uint1_dt> ciao_done_stream0;
	hls::stream<uint1_dt> ciao_done_stream1;
	hls::stream<int> ngb_stream0;
	hls::stream<int> ngb_stream1;
	hls::stream<uint1_dt> ngb_done_stream0;
	hls::stream<uint1_dt> ngb_done_stream1;

	char level_plus1 = level + 1;

#pragma HLS STREAM variable=depth_inspect_stream0 depth=32
#pragma HLS STREAM variable=depth_inspect_stream1 depth=32
#pragma HLS STREAM variable=inspect_done_stream0 depth=4
#pragma HLS STREAM variable=inspect_done_stream1 depth=4
#pragma HLS STREAM variable=frontier_stream0 depth=32
#pragma HLS STREAM variable=frontier_stream1 depth=32
#pragma HLS STREAM variable=rpao_stream0 depth=32
#pragma HLS STREAM variable=rpao_stream1 depth=32
#pragma HLS STREAM variable=rpao_done_stream0 depth=4
#pragma HLS STREAM variable=rpao_done_stream1 depth=4
#pragma HLS STREAM variable=ciao_stream0 depth=64
#pragma HLS STREAM variable=ciao_stream1 depth=64
#pragma HLS STREAM variable=ciao_done_stream0 depth=4
#pragma HLS STREAM variable=ciao_done_stream1 depth=4
#pragma HLS STREAM variable=ngb_stream0 depth=64
#pragma HLS STREAM variable=ngb_stream1 depth=64
#pragma HLS STREAM variable=ngb_done_stream0 depth=4
#pragma HLS STREAM variable=ngb_done_stream1 depth=4

#pragma HLS dataflow
    read_depth(depth_for_inspect0, depth_inspect_stream0, 0, vertex_num);
    read_depth(depth_for_inspect1, depth_inspect_stream1, OFFSET, vertex_num);

	frontier_inspect(depth_inspect_stream0, inspect_done_stream0, 
			frontier_stream0, 0, vertex_num, &frontier_size0[0], level);
	frontier_inspect(depth_inspect_stream1, inspect_done_stream1, 
			frontier_stream1, OFFSET, vertex_num, &frontier_size1[1], level);

	read_rpao(rpao0, frontier_stream0, inspect_done_stream0, rpao_stream0, rpao_done_stream0);
	read_rpao(rpao1, frontier_stream1, inspect_done_stream1, rpao_stream1, rpao_done_stream1);

	read_ciao(ciao0, rpao_stream0, rpao_done_stream0, ciao_stream0, ciao_done_stream0);
	read_ciao(ciao1, rpao_stream1, rpao_done_stream1, ciao_stream1, ciao_done_stream1);

	get_ngb_depth(depth_for_expand0, ciao_stream0, ciao_done_stream0, ngb_stream0, ngb_done_stream0);
	get_ngb_depth(depth_for_expand1, ciao_stream1, ciao_done_stream1, ngb_stream1, ngb_done_stream1);

    update_depth(depth_for_update0, ngb_stream0, ngb_done_stream0, level_plus1);
    update_depth(depth_for_update1, ngb_stream1, ngb_done_stream1, level_plus1);

}
}

