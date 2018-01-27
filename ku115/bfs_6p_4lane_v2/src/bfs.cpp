/**
 * File              : src/bfs.cpp
 * Author            : Cheng Liu <st.liucheng@gmail.com>
 * Date              : 13.11.2017
 * Last Modified Date: 15.12.2017
 * Last Modified By  : Cheng Liu <st.liucheng@gmail.com>
 */
#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define BUF_SIZE 64

typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_uint<4> uint4_dt;
typedef ap_uint<5> uint5_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_uint<64> uint64_dt;
typedef ap_int<512> int512_dt;

static void read_depth(
		char *depth_b0, 
		char *depth_b1, 
		hls::stream<char> &depth_inspect_stream,
        int vertex_num)
{
	char buffer0[BUF_SIZE];
	char buffer1[BUF_SIZE];

    read_depth: 
	for (uint32_dt i = 0; i < vertex_num; i += 2 * BUF_SIZE){
		uint32_dt bank_idx;
		bank_idx.range(5, 0) = i.range(5, 0);
		bank_idx.range(30, 6) = i.range(31, 7);
		bank_idx.range(31, 31) = 0;

        mem2depth_buffer:
		for(int j = 0; j < BUF_SIZE; j++){
        #pragma HLS pipeline
			buffer0[j] = depth_b0[bank_idx + j];
			buffer1[j] = depth_b1[bank_idx + j];
		}

        depth0_buffer2stream:
		for(int j = 0; j < BUF_SIZE; j++){
        #pragma HLS pipeline
			depth_inspect_stream << buffer0[j];
		}

        depth1_buffer2stream:
		for(int j = 0; j < BUF_SIZE; j++){
        #pragma HLS pipeline
			depth_inspect_stream << buffer1[j];
		} 
	}
}

// inspect depth for frontier 
static void frontier_analysis(
		int *frontier_size,
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontier_stream,
		int vertex_num,
		const char level)
{
	int counter = 0;
    frontier_analysis:
    for (int i = 0; i < vertex_num; i++){
    #pragma HLS pipeline II=1
		char d = depth_inspect_stream.read();
		if(d == level){
			frontier_stream << i;
			counter++;
		}
	}

	inspect_done_stream << 1;
	*frontier_size = counter;
}

// Divide the frontier based on its ciao bank.
static void frontier_splitter(
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontier_b0_stream,
		hls::stream<int> &frontier_b1_stream,
		hls::stream<int> &frontier_b2_stream,
		hls::stream<int> &frontier_b3_stream,
		hls::stream<uint1_dt> &frontier_b0_done_stream,
		hls::stream<uint1_dt> &frontier_b1_done_stream,
		hls::stream<uint1_dt> &frontier_b2_done_stream,
		hls::stream<uint1_dt> &frontier_b3_done_stream,
		const char level
		){

	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;

	while(frontier_empty != 1 || done != 1){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS PIPELINE
		frontier_empty = frontier_stream.empty();
		done_empty = inspect_done_stream.empty();
		if(frontier_empty != 1){
			uint32_dt vidx = frontier_stream.read();
			uint2_dt channel = vidx.range(7, 6);

			if(channel == 0){
				frontier_b0_stream << vidx;
			}

			if(channel == 1){
				frontier_b1_stream << vidx;
			}

			if(channel == 2){
				frontier_b2_stream << vidx;
			}

			if(channel == 3){
				frontier_b3_stream << vidx;
			}
		}

		if(done_empty != 1 && frontier_empty == 1){
			done = inspect_done_stream.read();
		}
	}

	frontier_b0_done_stream << 1;
	frontier_b1_done_stream << 1;
	frontier_b2_done_stream << 1;
	frontier_b3_done_stream << 1;
}

// Read rpao of the frontier 
static void read_rpao(
		const uint64_dt *rpao,
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &frontier_done_stream,
		hls::stream<uint64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream)
{
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;

	while(frontier_empty != 1 || done != 1){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS PIPELINE
		frontier_empty = frontier_stream.empty();
		done_empty = frontier_done_stream.empty();
		if(frontier_empty != 1){
			uint32_dt vidx = frontier_stream.read();
			uint32_dt bank_vidx;
			bank_vidx.range(5, 0) = vidx.range(5, 0);
			bank_vidx.range(29, 6) = vidx.range(31, 8);
			bank_vidx.range(31, 30) = 0;
			rpao_stream << rpao[bank_vidx];
		}

		if(frontier_empty == 1 && done_empty != 1){
			done = frontier_done_stream.read();
		}
	}
	rpao_done_stream << 1;
}

static void read_ngb(
		const int *ciao,
		hls::stream<uint64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		const char level
		)
{
	uint1_dt rpao_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;
	int buffer[BUF_SIZE];

    read_ngb_L0:
	while((rpao_empty != 1) || (done != 1)){
	#pragma HLS LOOP_FLATTEN off
		rpao_empty = rpao_stream.empty();
		done_empty = rpao_done_stream.empty();
		if(rpao_empty != 1){
			uint64_dt rpitem = rpao_stream.read();
			int start = rpitem.range(31, 0);
			int num = rpitem.range(63, 32);
			int len = BUF_SIZE;
			for(int i = 0; i < num; i += BUF_SIZE){
				if(i + BUF_SIZE > num) len = num - i;
				for(int j = 0; j < len; j++){
                #pragma HLS pipeline
					buffer[j] = ciao[start + i + j];
				}

				for(int j = 0; j < len; j++){
                #pragma HLS pipeline
					ciao_stream << buffer[j];
				}
			}
		}

		if(done_empty != 1 && rpao_empty == 1){
			done = rpao_done_stream.read();
		}
	}
	ciao_done_stream << 1;
}

// Split the depth into two partitions
static void merge_ciao(
		hls::stream<int> &ciao_b0_stream,
		hls::stream<int> &ciao_b1_stream,
		hls::stream<int> &ciao_b2_stream,
		hls::stream<int> &ciao_b3_stream,
		hls::stream<uint1_dt> &ciao_b0_done_stream,
		hls::stream<uint1_dt> &ciao_b1_done_stream,
		hls::stream<uint1_dt> &ciao_b2_done_stream,
		hls::stream<uint1_dt> &ciao_b3_done_stream,
		hls::stream<int> &merged_ciao_b0_stream,
		hls::stream<int> &merged_ciao_b1_stream,
		hls::stream<uint1_dt> &merged_ciao_b0_done_stream,
		hls::stream<uint1_dt> &merged_ciao_b1_done_stream
		)
{
	uint1_dt ciao_b0_empty = 0;
	uint1_dt ciao_b1_empty = 0;
	uint1_dt ciao_b2_empty = 0;
	uint1_dt ciao_b3_empty = 0;
	uint1_dt done_b0_empty = 0;
	uint1_dt done_b1_empty = 0;
	uint1_dt done_b2_empty = 0;
	uint1_dt done_b3_empty = 0;
	uint1_dt done_b0 = 0;
	uint1_dt done_b1 = 0;
	uint1_dt done_b2 = 0;
	uint1_dt done_b3 = 0;

	while((ciao_b0_empty != 1 || ciao_b1_empty != 1 || ciao_b2_empty != 1 || ciao_b3_empty != 1) 
			|| (done_b0 != 1 || done_b1 != 1 || done_b2 != 1 || done_b3 != 1))
	{
		ciao_b0_empty = ciao_b0_stream.empty();
		ciao_b1_empty = ciao_b1_stream.empty();
		ciao_b2_empty = ciao_b2_stream.empty();
		ciao_b3_empty = ciao_b3_stream.empty();
		done_b0_empty = ciao_b0_done_stream.empty();
		done_b1_empty = ciao_b1_done_stream.empty();
		done_b2_empty = ciao_b2_done_stream.empty();
		done_b3_empty = ciao_b3_done_stream.empty();

		// data path 0 processing
		if(ciao_b0_empty != 1){
			uint32_dt vidx = ciao_b0_stream.read();
			uint1_dt bank_sel = vidx.range(6, 6);
			if(bank_sel == 0){
				merged_ciao_b0_stream << vidx;
			}
			else{
				merged_ciao_b1_stream << vidx;
			}
		}

		// data path 1 processing
		if(ciao_b1_empty != 1){
			uint32_dt vidx = ciao_b1_stream.read();
			uint1_dt bank_sel = vidx.range(6, 6);
			if(bank_sel == 0){
				merged_ciao_b0_stream << vidx;
			}
			else{
				merged_ciao_b1_stream << vidx;
			}
		}

		// data path 2 processing
		if(ciao_b2_empty != 1){
			uint32_dt vidx = ciao_b2_stream.read();
			uint1_dt bank_sel = vidx.range(6, 6);
			if(bank_sel == 0){
				merged_ciao_b0_stream << vidx;
			}
			else{
				merged_ciao_b1_stream << vidx;
			}
		}

		// data path 3 processing
		if(ciao_b3_empty != 1){
			uint32_dt vidx = ciao_b3_stream.read();
			uint1_dt bank_sel = vidx.range(6, 6);
			if(bank_sel == 0){
				merged_ciao_b0_stream << vidx;
			}
			else{
				merged_ciao_b1_stream << vidx;
			}
		}

		// done analysis
		if(ciao_b0_empty == 1 && done_b0_empty != 1){
			done_b0 = ciao_b0_done_stream.read();
		}
		if(ciao_b1_empty == 1 && done_b1_empty != 1){
			done_b1 = ciao_b1_done_stream.read();
		}
		if(ciao_b2_empty == 1 && done_b2_empty != 1){
			done_b2 = ciao_b2_done_stream.read();
		}
		if(ciao_b3_empty == 1 && done_b3_empty != 1){
			done_b3 = ciao_b3_done_stream.read();
		}
	}

	merged_ciao_b0_done_stream << 1;
	merged_ciao_b1_done_stream << 1;
	
}

static void update_depth_s0(
		char *depth,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int> &vidx_update_stream,
		hls::stream<uint1_dt> &update_done_stream,
		char level
		){
	uint1_dt done = 0;
	uint1_dt ciao_empty = 0;
	uint1_dt done_empty = 0;

    update_depth:
	while((ciao_empty != 1) || (done != 1)){
    #pragma HLS LOOP FLATTEN OFF
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			uint32_dt bank_vidx;
			uint32_dt vidx = ciao_stream.read();
			bank_vidx.range(5, 0) = vidx.range(5, 0);
			bank_vidx.range(30, 6) = vidx.range(31, 7);
			bank_vidx.range(31, 31) = 0;
			char d = depth[bank_vidx]; 
			if(d == -1) vidx_update_stream << bank_vidx;
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();	
		}
	}
	update_done_stream << 1;
}

static void update_depth_s1(
		char *depth,
		hls::stream<int> &vidx_update_stream,
		hls::stream<uint1_dt> &update_done_stream,
		char level
		){
	uint1_dt done = 0;
	uint1_dt update_empty = 0;
	uint1_dt done_empty = 0;

    update_depth:
	while((update_empty != 1) || (done != 1)){
    #pragma HLS LOOP FLATTEN OFF
    #pragma HLS pipeline
		update_empty = vidx_update_stream.empty();
		done_empty = update_done_stream.empty();
		if(update_empty != 1){
			uint32_dt vidx = vidx_update_stream.read();
			depth[vidx] = level + 1;
		}

		if(done_empty != 1 && update_empty == 1){
			done = update_done_stream.read();	
		}
	}
}

extern "C" {
void bfs(
		char *depth_inspect_b0,
		char *depth_inspect_b1,

		char *depth_update_b0,
		char *depth_update_b1,

		const uint64_dt *rpao_b0,	
		const uint64_dt *rpao_b1,	
		const uint64_dt *rpao_b2,	
		const uint64_dt *rpao_b3,	

		const int *ciao_b0,
		const int *ciao_b1,
		const int *ciao_b2,
		const int *ciao_b3,

		int *frontier_size,
		char *depth_read_b0,
		char *depth_read_b1,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_inspect_b0 offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_inspect_b1 offset=slave bundle=gmem1

#pragma HLS INTERFACE m_axi port=depth_update_b0 offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=depth_update_b1 offset=slave bundle=gmem3

#pragma HLS INTERFACE m_axi port=rpao_b0 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=rpao_b1 offset=slave bundle=gmem5
#pragma HLS INTERFACE m_axi port=rpao_b2 offset=slave bundle=gmem6
#pragma HLS INTERFACE m_axi port=rpao_b3 offset=slave bundle=gmem7

#pragma HLS INTERFACE m_axi port=ciao_b0 offset=slave bundle=gmem8
#pragma HLS INTERFACE m_axi port=ciao_b1 offset=slave bundle=gmem9
#pragma HLS INTERFACE m_axi port=ciao_b2 offset=slave bundle=gmem10
#pragma HLS INTERFACE m_axi port=ciao_b3 offset=slave bundle=gmem11

#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem12

#pragma HLS INTERFACE m_axi port=depth_read_b0 offset=slave bundle=gmem13
#pragma HLS INTERFACE m_axi port=depth_read_b1 offset=slave bundle=gmem14

#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

hls::stream<char> depth_inspect_stream;
hls::stream<uint1_dt> inspect_done_stream;
hls::stream<int> frontier_stream;

hls::stream<int> frontier_b0_stream;
hls::stream<int> frontier_b1_stream;
hls::stream<int> frontier_b2_stream;
hls::stream<int> frontier_b3_stream;

hls::stream<uint1_dt> frontier_b0_done_stream;
hls::stream<uint1_dt> frontier_b1_done_stream;
hls::stream<uint1_dt> frontier_b2_done_stream;
hls::stream<uint1_dt> frontier_b3_done_stream;

hls::stream<uint64_dt> rpao_b0_stream;
hls::stream<uint64_dt> rpao_b1_stream;
hls::stream<uint64_dt> rpao_b2_stream;
hls::stream<uint64_dt> rpao_b3_stream;

hls::stream<uint1_dt> rpao_b0_done_stream;
hls::stream<uint1_dt> rpao_b1_done_stream;
hls::stream<uint1_dt> rpao_b2_done_stream;
hls::stream<uint1_dt> rpao_b3_done_stream;

hls::stream<int> ciao_b0_stream;
hls::stream<int> ciao_b1_stream;
hls::stream<int> ciao_b2_stream;
hls::stream<int> ciao_b3_stream;

hls::stream<uint1_dt> ciao_b0_done_stream;
hls::stream<uint1_dt> ciao_b1_done_stream;
hls::stream<uint1_dt> ciao_b2_done_stream;
hls::stream<uint1_dt> ciao_b3_done_stream;

hls::stream<int> merged_ciao_b0_stream;
hls::stream<int> merged_ciao_b1_stream;

hls::stream<uint1_dt> merged_ciao_b0_done_stream;
hls::stream<uint1_dt> merged_ciao_b1_done_stream;

hls::stream<int> vidx_update_b0_stream;
hls::stream<int> vidx_update_b1_stream;

hls::stream<uint1_dt> update_done_b0_stream;
hls::stream<uint1_dt> update_done_b1_stream;


#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=frontier_stream depth=32

#pragma HLS STREAM variable=frontier_b0_stream depth=32
#pragma HLS STREAM variable=frontier_b1_stream depth=32
#pragma HLS STREAM variable=frontier_b2_stream depth=32
#pragma HLS STREAM variable=frontier_b3_stream depth=32

#pragma HLS STREAM variable=frontier_b0_done_stream depth=4
#pragma HLS STREAM variable=frontier_b1_done_stream depth=4
#pragma HLS STREAM variable=frontier_b2_done_stream depth=4
#pragma HLS STREAM variable=frontier_b3_done_stream depth=4

#pragma HLS STREAM variable=rpao_b0_stream depth=32
#pragma HLS STREAM variable=rpao_b1_stream depth=32
#pragma HLS STREAM variable=rpao_b2_stream depth=32
#pragma HLS STREAM variable=rpao_b3_stream depth=32

#pragma HLS STREAM variable=rpao_b0_done_stream depth=4
#pragma HLS STREAM variable=rpao_b1_done_stream depth=4
#pragma HLS STREAM variable=rpao_b2_done_stream depth=4
#pragma HLS STREAM variable=rpao_b3_done_stream depth=4

#pragma HLS STREAM variable=ciao_b0_stream depth=32
#pragma HLS STREAM variable=ciao_b1_stream depth=32
#pragma HLS STREAM variable=ciao_b2_stream depth=32
#pragma HLS STREAM variable=ciao_b3_stream depth=32

#pragma HLS STREAM variable=ciao_b0_done_stream depth=4
#pragma HLS STREAM variable=ciao_b1_done_stream depth=4
#pragma HLS STREAM variable=ciao_b2_done_stream depth=4
#pragma HLS STREAM variable=ciao_b3_done_stream depth=4

#pragma HLS STREAM variable=merged_ciao_b0_stream depth=32
#pragma HLS STREAM variable=merged_ciao_b1_stream depth=32

#pragma HLS STREAM variable=merged_ciao_b0_done_stream depth=4
#pragma HLS STREAM variable=merged_ciao_b1_done_stream depth=4

#pragma HLS STREAM variable=vidx_update_b0_stream depth=32
#pragma HLS STREAM variable=vidx_update_b1_stream depth=32

#pragma HLS STREAM variable=update_done_b0_stream depth=4
#pragma HLS STREAM variable=update_done_b1_stream depth=4

#pragma HLS dataflow
read_depth(
		depth_inspect_b0, 
		depth_inspect_b1, 
		depth_inspect_stream, 
		vertex_num);


frontier_analysis(
		frontier_size, 
		depth_inspect_stream, 
		inspect_done_stream, 
		frontier_stream, 
		vertex_num, 
		level);


frontier_splitter(
		frontier_stream,
		inspect_done_stream,
		frontier_b0_stream,
		frontier_b1_stream,
		frontier_b2_stream,
		frontier_b3_stream,
		frontier_b0_done_stream,
		frontier_b1_done_stream,
		frontier_b2_done_stream,
		frontier_b3_done_stream,
		level
		);

read_rpao(
		rpao_b0,
		frontier_b0_stream,
		frontier_b0_done_stream,
		rpao_b0_stream,
		rpao_b0_done_stream);


read_rpao(
		rpao_b1,
		frontier_b1_stream,
		frontier_b1_done_stream,
		rpao_b1_stream,
		rpao_b1_done_stream);

read_rpao(
		rpao_b2,
		frontier_b2_stream,
		frontier_b2_done_stream,
		rpao_b2_stream,
		rpao_b2_done_stream);

read_rpao(
		rpao_b3,
		frontier_b3_stream,
		frontier_b3_done_stream,
		rpao_b3_stream,
		rpao_b3_done_stream);

read_ngb(
		ciao_b0,
		rpao_b0_stream,
		rpao_b0_done_stream,
		ciao_b0_stream,
		ciao_b0_done_stream,
		level);

read_ngb(
		ciao_b1,
		rpao_b1_stream,
		rpao_b1_done_stream,
		ciao_b1_stream,
		ciao_b1_done_stream,
		level);

read_ngb(
		ciao_b2,
		rpao_b2_stream,
		rpao_b2_done_stream,
		ciao_b2_stream,
		ciao_b2_done_stream,
		level);

read_ngb(
		ciao_b3,
		rpao_b3_stream,
		rpao_b3_done_stream,
		ciao_b3_stream,
		ciao_b3_done_stream,
		level);

merge_ciao(
		ciao_b0_stream,
		ciao_b1_stream, 
		ciao_b2_stream,
		ciao_b3_stream,
		ciao_b0_done_stream,
		ciao_b1_done_stream,
		ciao_b2_done_stream,
		ciao_b3_done_stream,
		merged_ciao_b0_stream,
		merged_ciao_b1_stream,
		merged_ciao_b0_done_stream,
		merged_ciao_b1_done_stream
		);

update_depth_s0(
		depth_read_b0,
		merged_ciao_b0_stream,
		merged_ciao_b0_done_stream,
		vidx_update_b0_stream,
		update_done_b0_stream,
		level
		);

update_depth_s0(
		depth_read_b1,
		merged_ciao_b1_stream,
		merged_ciao_b1_done_stream,
		vidx_update_b1_stream,
		update_done_b1_stream,
		level
		);

update_depth_s1(
		depth_update_b0,
		vidx_update_b0_stream,
		update_done_b0_stream,
		level
		);

update_depth_s1(
		depth_update_b1,
		vidx_update_b1_stream,
		update_done_b1_stream,
		level
		);

}
}



