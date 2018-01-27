#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_int<16> int16_dt;
typedef ap_int<32> int32_dt;
typedef ap_int<64> int64_dt;

#define BUF_SIZE 64
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
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontier_stream,
		int vertex_num,
		const int level)
{
	char d;
	int counter = 0;
#ifdef SW_EMU
	int cnt0 = 0;
	int cnt1 = 0;
	int cnt2 = 0;
	int cnt3 = 0;
#endif

    inspect: for (int i = 0; i < vertex_num; i++){
#pragma HLS pipeline
		d = depth_inspect_stream.read();
		if(d == level){
			frontier_stream << i;
			counter++;
#ifdef SW_EMU
			if(i < vertex_num/4){
				cnt0++;
			}
			else if(i < vertex_num/2){
				cnt1++;
			}
			else if(i < vertex_num*3/4){
				cnt2++;
			}
			else{
				cnt3++;
			}
#endif
		}

		if(i == vertex_num - 1){
			inspect_done_stream << 1;
			*frontier_size = counter;
		}
	}

#ifdef SW_EMU
	printf("frontier = %d, work load = %f, %f, %f, %f\n", 
			counter, cnt0*1.0/counter, cnt1*1.0/counter, cnt2*1.0/counter, cnt3*1.0/counter);
#endif

}

// Read rpao of the frontier 
static void bfs_stage2(
		const int64_dt* rpao_extended,
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
			rpao_stream << rpao_extended[vidx];
		}
		else{
			if(done_empty != 1){
				done = inspect_done_stream.read();
				rpao_done_stream << 1;
			}
		}
	}
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
				}
			}
		}
		else{
			if(done_empty != 1){
				done = rpao_done_stream.read();
				ciao_done_stream << 1;
			}
		}
	}
}



static void bfs_stage4(
		const char *depth,
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
	int32_dt vidx;
	char d;
	int16_dt delay_counter = 0;
	while((ciao_empty != 1) || (done != 1)){
#pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			vidx = ciao_stream.read();
			uint2_dt sel = vidx.range(11, 10); 
			d = depth[vidx];
			if(d == -1 && sel == 0){
				ngb0_stream << vidx;
			}
			else if(d == -1 && sel == 1){
				ngb1_stream << vidx;
			}
			else if(d == -1 && sel == 2){
				ngb2_stream << vidx;
			}
			else if(d == -1 && sel == 3){
				ngb3_stream << vidx;
			}
		}

		else{
			if(done_empty != 1){
				delay_counter++;
				if(delay_counter == 100){
					done = ciao_done_stream.read();
					ngb0_done_stream << 1;
					ngb1_done_stream << 1;
					ngb2_done_stream << 1;
					ngb3_done_stream << 1;
				}
			}
		}
	}
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
		else{
			if(done_empty != 1){
				done = ngb_done_stream.read();
			}
		}
    }
}


extern "C" {
void bfs(
		const char *depth_for_inspect,
		const char *depth_for_expand,
		char *depth0_for_update,
		char *depth1_for_update,
		char *depth2_for_update,
		char *depth3_for_update,
		const int64_dt *rpao_extended, 
		const int *ciao,
		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_for_expand offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=depth0_for_update offset=slave bundle=gmem20
#pragma HLS INTERFACE m_axi port=depth1_for_update offset=slave bundle=gmem21
#pragma HLS INTERFACE m_axi port=depth2_for_update offset=slave bundle=gmem22
#pragma HLS INTERFACE m_axi port=depth3_for_update offset=slave bundle=gmem23
#pragma HLS INTERFACE m_axi port=rpao_extended offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=ciao offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem5
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
hls::stream<int> ngb0_stream;
hls::stream<int> ngb1_stream;
hls::stream<int> ngb2_stream;
hls::stream<int> ngb3_stream;
hls::stream<uint1_dt> ngb0_done_stream;
hls::stream<uint1_dt> ngb1_done_stream;
hls::stream<uint1_dt> ngb2_done_stream;
hls::stream<uint1_dt> ngb3_done_stream;


#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=frontier_stream depth=32
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=rpao_done_stream depth=4
#pragma HLS STREAM variable=rpao_stream depth=32
#pragma HLS STREAM variable=ciao_stream depth=32
#pragma HLS STREAM variable=ciao_done_stream depth=4
#pragma HLS STREAM variable=ngb0_stream depth=4096
#pragma HLS STREAM variable=ngb1_stream depth=4096
#pragma HLS STREAM variable=ngb2_stream depth=4096
#pragma HLS STREAM variable=ngb3_stream depth=4096
#pragma HLS STREAM variable=ngb0_done_stream depth=4
#pragma HLS STREAM variable=ngb1_done_stream depth=4
#pragma HLS STREAM variable=ngb2_done_stream depth=4
#pragma HLS STREAM variable=ngb3_done_stream depth=4

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, vertex_num);
bfs_stage1(frontier_size, depth_inspect_stream, inspect_done_stream, frontier_stream, vertex_num, level);
bfs_stage2(rpao_extended, frontier_stream, inspect_done_stream, rpao_stream, rpao_done_stream);
bfs_stage3(ciao, rpao_stream, rpao_done_stream, ciao_stream, ciao_done_stream);
bfs_stage4(depth_for_expand, ciao_stream, ciao_done_stream, 
		ngb0_stream, ngb1_stream, ngb2_stream, ngb3_stream, 
		ngb0_done_stream, ngb1_done_stream, ngb2_done_stream, ngb3_done_stream);
bfs_stage5(depth0_for_update, ngb0_stream, ngb0_done_stream, level_plus1);
bfs_stage5(depth1_for_update, ngb1_stream, ngb1_done_stream, level_plus1);
bfs_stage5(depth2_for_update, ngb2_stream, ngb2_done_stream, level_plus1);
bfs_stage5(depth3_for_update, ngb3_stream, ngb3_done_stream, level_plus1);
}
}

