#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_int<16> int16_dt;
typedef ap_uint<30> uint30_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_int<64> int64_dt;


#define BUF_SIZE 64
#define HB 7
#define LB 6
//#define DEBUG

// Note that the vertex_num must be aligned to 64B
// such that the data in the buffer stays in a single 
// partition.  
static void bfs_stage0(
		char *depth_b0, 
		char *depth_b1, 
		char *depth_b2, 
		char *depth_b3, 
		hls::stream<char> &depth_inspect_stream,
        int vertex_num)
{
	char buffer0[BUF_SIZE];
	char buffer1[BUF_SIZE];
	char buffer2[BUF_SIZE];
	char buffer3[BUF_SIZE];

    read_depth: 
	for (uint32_dt i = 0; i < vertex_num; i += 4 * BUF_SIZE){
		uint2_dt bank_idx = i.range(HB, LB);
		uint30_dt bank_offset;
		bank_offset.range(LB - 1, 0) = i.range(LB - 1, 0);
		bank_offset.range(29, LB) = i.range(31, HB + 1);

        mem2depth_buffer:
		for(int j = 0; j < BUF_SIZE; j++){
        #pragma HLS pipeline
			buffer0[j] = depth_b0[bank_offset + j];
			buffer1[j] = depth_b1[bank_offset + j];
			buffer2[j] = depth_b2[bank_offset + j];
			buffer3[j] = depth_b3[bank_offset + j];
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

        depth2_buffer2stream:
		for(int j = 0; j < BUF_SIZE; j++){
        #pragma HLS pipeline
			depth_inspect_stream << buffer2[j];
		}

        depth3_buffer2stream:
		for(int j = 0; j < BUF_SIZE; j++){
        #pragma HLS pipeline
			depth_inspect_stream << buffer3[j];
		}
	}
}

// inspect depth for frontier 
static void bfs_stage1(
		int *frontier_size,
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontier_stream,
		int vertex_num,
		const char level)
{
	int counter = 0;
	for (int i = 0; i < vertex_num; i++){
    #pragma HLS pipeline
		char d = depth_inspect_stream.read();
		if(d == level){
			frontier_stream << i;
			counter++;
		}

		if(i == vertex_num - 1){
			*frontier_size = counter;
		}
	}

	inspect_done_stream << 1;

}

// Read rpao of the frontier 
static void bfs_stage2(
		const int64_dt* rpao_extend,
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream)
{
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;
	int vidx;
	while((frontier_empty != 1) || (done != 1)){
#pragma HLS pipeline
		frontier_empty = frontier_stream.empty();
	    done_empty = inspect_done_stream.empty();

		if(frontier_empty != 1){
			vidx = frontier_stream.read();
			rpao_stream << rpao_extend[vidx];
		}

		if(done_empty != 1 && frontier_empty == 1){
			done = inspect_done_stream.read();
		}
	}
	rpao_done_stream << 1;
}

static void bfs_stage3(
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
	int buffer[BUF_SIZE];
	while((rpao_empty != 1) || (done != 1)){
		rpao_empty = rpao_stream.empty();
		done_empty = rpao_done_stream.empty();
		
		if(rpao_empty != 1){
			int64_dt rpitem = rpao_stream.read();
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
#ifdef DEBUG
					std::cout << buffer[j] << " ";
#endif
				}
			}
		}
		if(done_empty != 1 && rpao_empty == 1){
			done = rpao_done_stream.read();
		}
	}
	ciao_done_stream << 1;
#ifdef DEBUG
	printf("End of ngb stream.\n");
#endif
}


static void bfs_stage4(
		char *depth_b0,
		char *depth_b1,
		char *depth_b2,
		char *depth_b3,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int> &ngb0_stream,
		hls::stream<int> &ngb1_stream,
		hls::stream<int> &ngb2_stream,
		hls::stream<int> &ngb3_stream,
		hls::stream<uint1_dt> &ngb0_done_stream,
		hls::stream<uint1_dt> &ngb1_done_stream,
		hls::stream<uint1_dt> &ngb2_done_stream,
		hls::stream<uint1_dt> &ngb3_done_stream
		){
	uint1_dt done = 0;
	uint1_dt ciao_empty = 0;
	uint1_dt done_empty = 0;
	while((ciao_empty != 1) || (done != 1)){
#pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			uint32_dt bank_offset;
			uint32_dt vidx = ciao_stream.read();
			uint2_dt bank_idx = vidx.range(HB, LB); 
			bank_offset.range(LB - 1, 0) = vidx.range(LB - 1, 0);
			bank_offset.range(29, LB) = vidx.range(31, HB + 1);
			bank_offset.range(31, 30) = 0;
#ifdef DEBUG
			printf("vidx=%d, bank_idx=%d, bank_offset=%d\n", (int)vidx, (int)bank_idx, (int)bank_offset);
#endif
			if(bank_idx == 0) {
				char d = depth_b0[bank_offset]; 
				if(d == -1) ngb0_stream << bank_offset;
			}

			if(bank_idx == 1){
				char d = depth_b1[bank_offset];
				if(d == -1) ngb1_stream << bank_offset;
			}

			if(bank_idx == 2){
				char d = depth_b2[bank_offset];
				if(d == -1) ngb2_stream << bank_offset;
			}

			if(bank_idx == 3) {
				char d = depth_b3[bank_offset];
				if(d == -1) ngb3_stream << bank_offset;
			}
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();	
		}
	}

	ngb0_done_stream << 1;
	ngb1_done_stream << 1;
	ngb2_done_stream << 1;
	ngb3_done_stream << 1;
}

// load depth for inspection 
// Each instance handles only one bank of data
static void bfs_stage5(
		char *depth,
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		const char level_plus1)
{
	int done = 0;
	int ngb_empty = 0;
	int done_empty = 0;
    while(ngb_empty != 1 || done != 1){
    #pragma HLS pipeline
		ngb_empty = ngb_stream.empty();
		done_empty = ngb_done_stream.empty();
		if(ngb_empty != 1){
			uint32_dt vidx = ngb_stream.read();
			depth[vidx] = level_plus1;
		}

		if(done_empty != 1 && ngb_empty == 1){
			done = ngb_done_stream.read();
		}
	}
}

extern "C" {
void bfs(
		char *depth_for_inspect_b0,
		char *depth_for_inspect_b1,
		char *depth_for_inspect_b2,
		char *depth_for_inspect_b3,
		char *depth_for_expand_b0,
		char *depth_for_expand_b1,
		char *depth_for_expand_b2,
		char *depth_for_expand_b3,
		char *depth_for_update_b0,
		char *depth_for_update_b1,
		char *depth_for_update_b2,
		char *depth_for_update_b3,
		const int64_dt *rpao_extend, 
		const int *ciao,
		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect_b0 offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_for_inspect_b1 offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=depth_for_inspect_b2 offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=depth_for_inspect_b3 offset=slave bundle=gmem3

#pragma HLS INTERFACE m_axi port=depth_for_expand_b0 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=depth_for_expand_b1 offset=slave bundle=gmem5
#pragma HLS INTERFACE m_axi port=depth_for_expand_b2 offset=slave bundle=gmem6
#pragma HLS INTERFACE m_axi port=depth_for_expand_b3 offset=slave bundle=gmem7

#pragma HLS INTERFACE m_axi port=depth_for_update_b0 offset=slave bundle=gmem8
#pragma HLS INTERFACE m_axi port=depth_for_update_b1 offset=slave bundle=gmem9
#pragma HLS INTERFACE m_axi port=depth_for_update_b2 offset=slave bundle=gmem10
#pragma HLS INTERFACE m_axi port=depth_for_update_b3 offset=slave bundle=gmem11

#pragma HLS INTERFACE m_axi port=rpao_extend offset=slave bundle=gmem12
#pragma HLS INTERFACE m_axi port=ciao offset=slave bundle=gmem13
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem14
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

char level_plus1 = level + 1;

hls::stream<char> depth_inspect_stream;
hls::stream<uint1_dt> inspect_done_stream;
hls::stream<int> frontier_stream;
hls::stream<int64_dt> rpao_stream;
hls::stream<uint1_dt> rpao_done_stream;
hls::stream<int> ciao_stream;
hls::stream<uint1_dt> ciao_done_stream;
hls::stream<int> ngb_stream0;
hls::stream<int> ngb_stream1;
hls::stream<int> ngb_stream2;
hls::stream<int> ngb_stream3;
hls::stream<uint1_dt> ngb_done_stream0;
hls::stream<uint1_dt> ngb_done_stream1;
hls::stream<uint1_dt> ngb_done_stream2;
hls::stream<uint1_dt> ngb_done_stream3;

#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=frontier_stream depth=32
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=rpao_done_stream depth=4
#pragma HLS STREAM variable=rpao_stream depth=32
#pragma HLS STREAM variable=ciao_stream depth=32
#pragma HLS STREAM variable=ciao_done_stream depth=4
#pragma HLS STREAM variable=ngb_stream0 depth=32
#pragma HLS STREAM variable=ngb_stream1 depth=32
#pragma HLS STREAM variable=ngb_stream2 depth=32
#pragma HLS STREAM variable=ngb_stream3 depth=32
#pragma HLS STREAM variable=ngb_done_stream0 depth=4
#pragma HLS STREAM variable=ngb_done_stream1 depth=4
#pragma HLS STREAM variable=ngb_done_stream2 depth=4
#pragma HLS STREAM variable=ngb_done_stream3 depth=4

#pragma HLS dataflow
bfs_stage0(
		depth_for_inspect_b0, 
		depth_for_inspect_b1, 
		depth_for_inspect_b2, 
		depth_for_inspect_b3,
		depth_inspect_stream, 
		vertex_num);

#ifdef DEBUG
printf("stage0 is done\n");
#endif

bfs_stage1(
		frontier_size, 
		depth_inspect_stream, 
		inspect_done_stream, 
		frontier_stream, 
		vertex_num, 
		level);

#ifdef DEBUG
printf("stage1 is done\n");
#endif
bfs_stage2(
		rpao_extend, 
		frontier_stream, 
		inspect_done_stream, 
		rpao_stream, 
		rpao_done_stream);

#ifdef DEBUG
printf("stage2 is done\n");
#endif
bfs_stage3(
		ciao, 
		rpao_stream, 
		rpao_done_stream, 
		ciao_stream, 
		ciao_done_stream);

#ifdef DEBUG
printf("stage3 is done\n");
#endif
bfs_stage4(
		depth_for_expand_b0, 
		depth_for_expand_b1, 
		depth_for_expand_b2, 
		depth_for_expand_b3, 
		ciao_stream, 
		ciao_done_stream, 
		ngb_stream0, 
		ngb_stream1, 
		ngb_stream2, 
		ngb_stream3, 
		ngb_done_stream0, 
		ngb_done_stream1, 
		ngb_done_stream2, 
		ngb_done_stream3);

#ifdef DEBUG
printf("stage4 is done\n");
#endif
bfs_stage5(
		depth_for_update_b0, 
		ngb_stream0, 
		ngb_done_stream0, 
		level_plus1);

#ifdef DEBUG
printf("stage5-0 is done\n");
#endif
bfs_stage5(
		depth_for_update_b1, 
		ngb_stream1, 
		ngb_done_stream1, 
		level_plus1);

#ifdef DEBUG
printf("stage5-1 is done\n");
#endif
bfs_stage5(
		depth_for_update_b2, 
		ngb_stream2, 
		ngb_done_stream2, 
		level_plus1);

#ifdef DEBUG
printf("stage5-2 is done\n");
#endif
bfs_stage5(
		depth_for_update_b3, 
		ngb_stream3, 
		ngb_done_stream3, 
		level_plus1);

#ifdef DEBUG
printf("stage5-3 is done\n");
#endif
}
}

