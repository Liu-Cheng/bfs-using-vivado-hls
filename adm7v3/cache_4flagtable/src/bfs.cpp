#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>
#define HASH_SIZE 16384/2
#define HASH_MASK (HASH_SIZE - 1) //0x3FFF

typedef ap_uint<1> uint1_dt;
typedef ap_int<16> int16_dt;
typedef ap_int<64> int64_dt;

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
    inspect: for (int i = 0; i < vertex_num; i++){
#pragma HLS pipeline
		d = depth_inspect_stream.read();
		if(d == level){
			frontier_stream << i;
			counter++;
		}

		if(i == vertex_num - 1){
			inspect_done_stream << 1;
			*frontier_size = counter;
		}
	}
}

// Read rpao of the frontier 
static void bfs_stage2(
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

static void bfs_cache_ciao(
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		hls::stream<int> &ngb_cached_stream,
		hls::stream<uint1_dt> &ngb_cached_done_stream,
		int level
		)
{
	int vidx;
	int done = 0;
	int hash_idx, hash_val0, hash_val1,hash_val2,hash_val3;
	int ngb_empty = 0;
	int done_empty = 0;
	//for debug
	int cache0_hit_cnt = 0;
	int cache2_hit_cnt = 0;
	int cache3_hit_cnt = 0;
	int cache1_hit_cnt = 0;
	int ngb_cnt = 0;
	//
	char cache_flag = 0;
	int16_dt delay_counter = 0;
	int written_table0[HASH_SIZE];
	int written_table1[HASH_SIZE];
	int written_table2[HASH_SIZE];
	int written_table3[HASH_SIZE];
   	if(!level){
		for (int i=0; i < HASH_SIZE; i ++){
			written_table0[i] = 0;
			written_table1[i] = 0;
			written_table2[i] = 0;
			written_table3[i] = 0;
		}
	}
	while(ngb_empty != 1 || done != 1){
#pragma HLS pipeline
		ngb_empty = ngb_stream.empty();
		done_empty = ngb_done_stream.empty();
		if(ngb_empty != 1){
			vidx = ngb_stream.read();
		       	ngb_cnt ++;	
			hash_idx = vidx & HASH_MASK;
			//cache_flag = vidx;//printf ("%d",cache_flag);
			hash_val0 = written_table0[hash_idx];
			hash_val1 = written_table1[hash_idx];
			hash_val2 = written_table2[hash_idx];
			hash_val3 = written_table3[hash_idx];
			if(hash_val0 != vidx && hash_val1 != vidx && hash_val2 != vidx && hash_val3 != vidx){
			if(cache_flag == 0){
					written_table0[hash_idx] = vidx;
					ngb_cached_stream << vidx;
					cache_flag = 1;
			}
			else if(cache_flag == 1){
					written_table1[hash_idx] = vidx;
					ngb_cached_stream << vidx;
					cache_flag = 2;
			}
			else if(cache_flag == 2){
					written_table2[hash_idx] = vidx;
					ngb_cached_stream << vidx;
					cache_flag = 3;
			}
			else if(cache_flag == 3){
					written_table3[hash_idx] = vidx;
					ngb_cached_stream << vidx;
					cache_flag = 0;
			}

				cache0_hit_cnt ++;
		}
		}
		
		else{
			if(done_empty != 1){
				done = ngb_done_stream.read();
				ngb_cached_done_stream << 1;
			}
		}
	}
	printf("cache0 hit %d  %0.3f\n",(ngb_cnt - cache0_hit_cnt),100*(double)(ngb_cnt - cache0_hit_cnt)/(double)ngb_cnt);
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
		char *depth_for_update,
		const int *rpao, 
		const int *ciao,
		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_for_expand offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=rpao offset=slave bundle=gmem3
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
hls::stream<int> ngb_cached_stream;
hls::stream<int> ngb_stream;
hls::stream<uint1_dt> ngb_cached_done_stream;
hls::stream<uint1_dt> ngb_done_stream;


#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=frontier_stream depth=32
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=rpao_done_stream depth=4
#pragma HLS STREAM variable=rpao_stream depth=32
#pragma HLS STREAM variable=ciao_stream depth=64
#pragma HLS STREAM variable=ciao_done_stream depth=4
#pragma HLS STREAM variable=ngb_stream depth=64
#pragma HLS STREAM variable=ngb_done_stream depth=4

#pragma HLS STREAM variable=ngb_cached_stream depth=64
#pragma HLS STREAM variable=ngb_cached_done_stream depth=4
#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, vertex_num);
bfs_stage1(frontier_size, depth_inspect_stream, inspect_done_stream, frontier_stream, vertex_num, level);
bfs_stage2(rpao, frontier_stream, inspect_done_stream, rpao_stream, rpao_done_stream);
bfs_stage3(ciao, rpao_stream, rpao_done_stream, ciao_stream, ciao_done_stream);
bfs_stage4(depth_for_expand, ciao_stream, ciao_done_stream, ngb_stream, ngb_done_stream);
bfs_cache_ciao(ngb_stream, ngb_done_stream,ngb_cached_stream, ngb_cached_done_stream,level);
bfs_stage5(depth_for_update, ngb_cached_stream, ngb_cached_done_stream, level_plus1);
}
}

