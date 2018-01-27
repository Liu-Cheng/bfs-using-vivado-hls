#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

typedef ap_uint<1> uint1_dt;
typedef ap_int<16> int16_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_int<64> int64_dt;

//#define SW_EMU

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
		int *frontier_size,
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<uint1_dt> &inspect_done_stream0,
		hls::stream<uint1_dt> &inspect_done_stream1,
		hls::stream<uint1_dt> &inspect_done_stream2,
		hls::stream<uint1_dt> &inspect_done_stream3,
		hls::stream<int> &frontier_stream0,
		hls::stream<int> &frontier_stream1,
		hls::stream<int> &frontier_stream2,
		hls::stream<int> &frontier_stream3,
		int vertex_num,
		const int level)
{
	char d;
	int counter = 0;
	int cnt0 = 0;
	int cnt1 = 0;
	int cnt2 = 0;
	int cnt3 = 0;

    inspect: 
	for (int i = 0; i < vertex_num; i++){
#pragma HLS pipeline
		d = depth_inspect_stream.read();
		if(d == level){
			counter++;
			if(i < vertex_num/4){
				frontier_stream0 << i;
			}
			else if(i < vertex_num/2){
				frontier_stream1 << i;
			}
			else if(i < vertex_num*3/4){
				frontier_stream2 << i;
			}
			else{
				frontier_stream3 << i;
			}
		}

		if(i == vertex_num - 1){
			*frontier_size = counter;
		}
	}

	inspect_done_stream0 << 1;
	inspect_done_stream2 << 1;
	inspect_done_stream3 << 1;
	inspect_done_stream1 << 1;
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
	int buffer[16];
	while((rpao_empty != 1) || (done != 1)){
		rpao_empty = rpao_stream.empty();
		done_empty = rpao_done_stream.empty();
		
		if(rpao_empty != 1){
			int64_dt rpitem = rpao_stream.read();
			int start = rpitem.range(31, 0);
			int num = rpitem.range(63, 32);
			int len = 16;
			for(int i = 0; i < num; i += 16){
				if(i + 16 > num) len = num - i;
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
		hls::stream<uint1_dt> &merged_ciao_b0_done_stream
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
			merged_ciao_b0_stream << vidx;
		}

		// data path 1 processing
		if(ciao_b1_empty != 1){
			uint32_dt vidx = ciao_b1_stream.read();
			merged_ciao_b0_stream << vidx;
		}

		// data path 2 processing
		if(ciao_b2_empty != 1){
			uint32_dt vidx = ciao_b2_stream.read();
			merged_ciao_b0_stream << vidx;
		}

		// data path 3 processing
		if(ciao_b3_empty != 1){
			uint32_dt vidx = ciao_b3_stream.read();
			merged_ciao_b0_stream << vidx;
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
	
}


static void bfs_stage4(
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

		if(done_empty != 1 && ciao_empty == 1){
			delay_counter++;
			if(delay_counter == 100){
				done = ciao_done_stream.read();
			}
		}
	}

	ngb_done_stream << 1;
}
// load depth for inspection 
static void bfs_stage5(
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

		if(done_empty != 1 && ngb_empty == 1){
			done = ngb_done_stream.read();
		}
	}
}


extern "C" {
void bfs(
		const char *depth_for_inspect,
		const char *depth_for_expand,
		char *depth_for_update,
		const int64_dt *rpao_extend0, 
		const int64_dt *rpao_extend1, 
		const int64_dt *rpao_extend2, 
		const int64_dt *rpao_extend3, 
		const int *ciao_bank0,
		const int *ciao_bank1,
		const int *ciao_bank2,
		const int *ciao_bank3,
		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_for_expand offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem2

#pragma HLS INTERFACE m_axi port=rpao_extend0 offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=rpao_extend1 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=rpao_extend2 offset=slave bundle=gmem5
#pragma HLS INTERFACE m_axi port=rpao_extend3 offset=slave bundle=gmem6

#pragma HLS INTERFACE m_axi port=ciao_bank0 offset=slave bundle=gmem7
#pragma HLS INTERFACE m_axi port=ciao_bank1 offset=slave bundle=gmem8
#pragma HLS INTERFACE m_axi port=ciao_bank2 offset=slave bundle=gmem9
#pragma HLS INTERFACE m_axi port=ciao_bank3 offset=slave bundle=gmem10

#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem11
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

char level_plus1 = level + 1;

hls::stream<char> depth_inspect_stream;
hls::stream<uint1_dt> inspect_done_stream0;
hls::stream<uint1_dt> inspect_done_stream1;
hls::stream<uint1_dt> inspect_done_stream2;
hls::stream<uint1_dt> inspect_done_stream3;
hls::stream<int> frontier_stream0;
hls::stream<int> frontier_stream1;
hls::stream<int> frontier_stream2;
hls::stream<int> frontier_stream3;
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
hls::stream<int> merged_ciao_stream;

hls::stream<uint1_dt> ciao_done_stream0;
hls::stream<uint1_dt> ciao_done_stream1;
hls::stream<uint1_dt> ciao_done_stream2;
hls::stream<uint1_dt> ciao_done_stream3;
hls::stream<uint1_dt> merged_ciao_done_stream;
hls::stream<int> ngb_stream;
hls::stream<uint1_dt> ngb_done_stream;


#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=frontier_stream0 depth=32
#pragma HLS STREAM variable=frontier_stream1 depth=32
#pragma HLS STREAM variable=frontier_stream2 depth=32
#pragma HLS STREAM variable=frontier_stream3 depth=32
#pragma HLS STREAM variable=inspect_done_stream0 depth=4
#pragma HLS STREAM variable=inspect_done_stream1 depth=4
#pragma HLS STREAM variable=inspect_done_stream2 depth=4
#pragma HLS STREAM variable=inspect_done_stream3 depth=4
#pragma HLS STREAM variable=rpao_done_stream0 depth=4
#pragma HLS STREAM variable=rpao_done_stream1 depth=4
#pragma HLS STREAM variable=rpao_done_stream2 depth=4
#pragma HLS STREAM variable=rpao_done_stream3 depth=4
#pragma HLS STREAM variable=rpao_stream0 depth=32
#pragma HLS STREAM variable=rpao_stream1 depth=32
#pragma HLS STREAM variable=rpao_stream2 depth=32
#pragma HLS STREAM variable=rpao_stream3 depth=32
#pragma HLS STREAM variable=ciao_stream0 depth=32
#pragma HLS STREAM variable=ciao_stream1 depth=32
#pragma HLS STREAM variable=ciao_stream2 depth=32
#pragma HLS STREAM variable=ciao_stream3 depth=32
#pragma HLS STREAM variable=merged_ciao_stream depth=32
#pragma HLS STREAM variable=ciao_done_stream0 depth=4
#pragma HLS STREAM variable=ciao_done_stream1 depth=4
#pragma HLS STREAM variable=ciao_done_stream2 depth=4
#pragma HLS STREAM variable=ciao_done_stream3 depth=4
#pragma HLS STREAM variable=merged_ciao_done_stream depth=32
#pragma HLS STREAM variable=ngb_stream depth=32
#pragma HLS STREAM variable=ngb_done_stream depth=4

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, vertex_num);

bfs_stage1(frontier_size, depth_inspect_stream, 
		inspect_done_stream0, inspect_done_stream1, inspect_done_stream2, inspect_done_stream3,
	       	frontier_stream0,frontier_stream1,frontier_stream2,frontier_stream3,
	       	vertex_num, level);

bfs_stage2(rpao_extend0, frontier_stream0, inspect_done_stream0, rpao_stream0, rpao_done_stream0);
bfs_stage2(rpao_extend1, frontier_stream1, inspect_done_stream1, rpao_stream1, rpao_done_stream1);
bfs_stage2(rpao_extend2, frontier_stream2, inspect_done_stream2, rpao_stream2, rpao_done_stream2);
bfs_stage2(rpao_extend3, frontier_stream3, inspect_done_stream3, rpao_stream3, rpao_done_stream3);

bfs_stage3(ciao_bank0, rpao_stream0, rpao_done_stream0, ciao_stream0, ciao_done_stream0);
bfs_stage3(ciao_bank1, rpao_stream1, rpao_done_stream1, ciao_stream1, ciao_done_stream1);
bfs_stage3(ciao_bank2, rpao_stream2, rpao_done_stream2, ciao_stream2, ciao_done_stream2);
bfs_stage3(ciao_bank3, rpao_stream3, rpao_done_stream3, ciao_stream3, ciao_done_stream3);

merge_ciao(ciao_stream0,ciao_stream1,ciao_stream2,ciao_stream3,
		ciao_done_stream0,ciao_done_stream1,ciao_done_stream2,ciao_done_stream3,
		merged_ciao_stream, merged_ciao_done_stream);

bfs_stage4(depth_for_expand, merged_ciao_stream, merged_ciao_done_stream, ngb_stream, ngb_done_stream);
bfs_stage5(depth_for_update, ngb_stream, ngb_done_stream, level_plus1);
}
}

