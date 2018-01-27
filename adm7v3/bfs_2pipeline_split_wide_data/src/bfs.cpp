#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define BUFFER_SIZE 32
typedef ap_uint<1> uint1_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<32> int32_dt;
typedef ap_int<16> int16_dt;

// load depth for inspection 
static void read_depth(
		const int16_dt *depth, 
		hls::stream<char> &depth_inspect_stream0,
		hls::stream<char> &depth_inspect_stream1,
        const int inspect_num)
{
	int16_dt buffer[BUFFER_SIZE];
	for (int i = 0; i < inspect_num; i += BUFFER_SIZE){
		for(int j = 0; j < BUFFER_SIZE; j++){
#pragma HLS pipeline
			buffer[j] = depth[i+j];
			depth_inspect_stream0 << buffer[j].range(7, 0);
			depth_inspect_stream1 << buffer[j].range(15, 8);
		}
	}
}

// inspect depth for frontier 
static void frontier_inspect(
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		int *frontier_size,
		const int inspect_num,
		const int lane,
		const char level)
{
	char d;
	int count = 0;
    for (int i = 0; i < inspect_num; i++){
#pragma HLS pipeline
		d = depth_inspect_stream.read();
		if(d == level){
			count++;
			frontier_stream << ((i << 1) + lane);
		}

		if(i == inspect_num - 1){
			*(frontier_size + lane) = count;
			inspect_done_stream << 1;
		}
	}
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

		if(done_empty != 1){
			done = rpao_done_stream.read();
			ciao_done_stream << 1;
		}
	}
}

static void get_ngb(
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
	int16_dt delay_counter = 0;
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

		else{
			if(done_empty != 1){
				delay_counter++;
				if(delay_counter == 100){
					done = ciao_done_stream.read();
					ngb_done_stream << 1;
				}
			}
		}
	}
}

static void update_depth(
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
		const int *rpao0, 
		const int *rpao1,
		const int *ciao0,
		const int *ciao1,
		const int16_dt *depth_for_inspect,
		const char *depth_for_expand0,	
		const char *depth_for_expand1,
		char *depth_for_update0,
		char *depth_for_update1,
		int *frontier_size0,
		int *frontier_size1,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=rpao0 offset=slave bundle=gmem00
#pragma HLS INTERFACE m_axi port=rpao1 offset=slave bundle=gmem01

#pragma HLS INTERFACE m_axi port=ciao0  offset=slave bundle=gmem10
#pragma HLS INTERFACE m_axi port=ciao1  offset=slave bundle=gmem11

#pragma HLS INTERFACE m_axi port=depth_for_inspect  offset=slave bundle=gmem20

#pragma HLS INTERFACE m_axi port=depth_for_expand0  offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=depth_for_expand1  offset=slave bundle=gmem31

#pragma HLS INTERFACE m_axi port=depth_for_update0  offset=slave bundle=gmem40
#pragma HLS INTERFACE m_axi port=depth_for_update1  offset=slave bundle=gmem41

#pragma HLS INTERFACE m_axi port=frontier_size0  offset=slave bundle=gmem50
#pragma HLS INTERFACE m_axi port=frontier_size1  offset=slave bundle=gmem51

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

#pragma HLS STREAM variable=depth_inspect_stream0 depth=32
#pragma HLS STREAM variable=depth_inspect_stream1 depth=32

#pragma HLS STREAM variable=inspect_done_stream0 depth=2
#pragma HLS STREAM variable=inspect_done_stream1 depth=2

#pragma HLS STREAM variable=frontier_stream0 depth=32
#pragma HLS STREAM variable=frontier_stream1 depth=32

#pragma HLS STREAM variable=rpao_stream0 depth=32
#pragma HLS STREAM variable=rpao_stream1 depth=32

#pragma HLS STREAM variable=rpao_done_stream0 depth=2
#pragma HLS STREAM variable=rpao_done_stream1 depth=2

#pragma HLS STREAM variable=ciao_stream0 depth=64
#pragma HLS STREAM variable=ciao_stream1 depth=64

#pragma HLS STREAM variable=ciao_done_stream0 depth=2
#pragma HLS STREAM variable=ciao_done_stream1 depth=2

#pragma HLS STREAM variable=ngb_stream0 depth=64
#pragma HLS STREAM variable=ngb_stream1 depth=64

#pragma HLS STREAM variable=ngb_done_stream0 depth=2
#pragma HLS STREAM variable=ngb_done_stream1 depth=2


	int inspect_num = vertex_num/2;
	char level_plus1 = level + 1;

#pragma HLS dataflow
    read_depth(depth_for_inspect, depth_inspect_stream0, depth_inspect_stream1, inspect_num);
	frontier_inspect(depth_inspect_stream0, frontier_stream0, 
			inspect_done_stream0, frontier_size0, inspect_num, 0, level);
	frontier_inspect(depth_inspect_stream1, frontier_stream1, 
			inspect_done_stream1, frontier_size1, inspect_num, 1, level);

	read_rpao(rpao0, frontier_stream0, inspect_done_stream0, rpao_stream0, rpao_done_stream0);
	read_rpao(rpao1, frontier_stream1, inspect_done_stream1, rpao_stream1, rpao_done_stream1);

	read_ciao(ciao0, rpao_stream0, rpao_done_stream0, ciao_stream0, ciao_done_stream0);
	read_ciao(ciao1, rpao_stream1, rpao_done_stream1, ciao_stream1, ciao_done_stream1);

	get_ngb(depth_for_expand0, ciao_stream0, ciao_done_stream0, ngb_stream0, ngb_done_stream0);
	get_ngb(depth_for_expand1, ciao_stream1, ciao_done_stream1, ngb_stream1, ngb_done_stream1);

    update_depth(depth_for_update0, ngb_stream0, ngb_done_stream0, level_plus1);
    update_depth(depth_for_update1, ngb_stream1, ngb_done_stream1, level_plus1);
}
}

