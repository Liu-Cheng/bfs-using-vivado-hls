#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

//#define SW_EMU

#define HASH_SIZE (16384)
#define HASH_ID_WIDTH 14
#define HASH_TAG_WIDTH 18
typedef ap_uint<HASH_ID_WIDTH> hash_index_dt;
typedef ap_uint<HASH_TAG_WIDTH> hash_tag_dt;

#define CACHE_SIZE 16384
#define LOG2_LEN 4
#define LINE_LEN 16
#define INDEX_WIDTH 14
#define TAG_WIDTH 14

typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_uint<4> uint4_dt;
typedef ap_uint<6> uint6_dt;
typedef ap_uint<8> uint8_dt;
typedef ap_uint<12> uint12_dt;
typedef ap_uint<14> uint14_dt;
typedef ap_uint<16> uint16_dt;
typedef ap_uint<22> uint22_dt;
typedef ap_uint<26> uint26_dt;
typedef ap_uint<28> uint28_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<512> int512_dt;
typedef ap_uint<LOG2_LEN> cache_offset_dt;
typedef ap_uint<INDEX_WIDTH> cache_index_dt;
typedef ap_uint<TAG_WIDTH> cache_tag_dt;

/*
// load depth for inspection 
static void bfs_stage0(
		const char *depth_for_inspect, 
		hls::stream<char> &depth_inspect_stream,
        int vertex_num)
{
    stage0_main:
    for (int i = 0; i < vertex_num; i++){
    #pragma HLS pipeline
        depth_inspect_stream << depth_for_inspect[i];
    }
}
*/



// inspect depth for frontier 
static void bfs_stage1(
		int *frontier_size,
		hls::stream<char> &depth_inspect_stream, 
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontier_stream,
		int vertex_num,
		const int level)
{
	int counter = 0;
    stage1_main:
    for (int i = 0; i < vertex_num; i++){
#pragma HLS pipeline
		char d = depth_inspect_stream.read();
		if(d == level){
			frontier_stream << i;
			counter++;
		}
	}

	inspect_done_stream << 1;
	*frontier_size = counter;
}

/*
// Read rpao of the frontier 
static void bfs_stage2(
		const int512_dt *rpao,
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream)
{
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;
	
	// 4 bit offset, 28 bit tag
	int512_dt cache_data[1];
	uint28_dt cache_tag[1] = {0xFFFFFFF};

    stage2_main:
	while((frontier_empty != 1) || (done != 1)){
    #pragma HLS pipeline
		frontier_empty = frontier_stream.empty();
	    done_empty = inspect_done_stream.empty();

		if(frontier_empty != 1){
			uint32_dt start_vidx = frontier_stream.read();
			uint32_dt end_vidx = start_vidx + 1;
			uint28_dt start_tag = start_vidx.range(31, 4);
			uint28_dt end_tag = end_vidx.range(31, 4);
			uint4_dt start_offset = start_vidx.range(3, 0);
			uint4_dt end_offset = end_vidx.range(3, 0);
			uint28_dt _tag = cache_tag[0];
			int64_dt rpitem;
			
			if(start_tag == _tag && start_tag == end_tag){
				rpitem = cache_data[0].range((start_offset + 2) * 32 - 1, start_offset * 32);
			}
			else if(start_tag != _tag && start_tag == end_tag){
				int512_dt word = rpao[start_tag];
				rpitem = word.range((start_offset + 2) * 32 - 1, start_offset * 32);
				cache_data[0] = word;
				cache_tag[0] = start_tag;

			}
			else if(start_tag == _tag && start_tag != end_tag){
				rpitem.range(31, 0) = cache_data[0].range(511, 480);
				int512_dt word = rpao[end_tag];
				rpitem.range(63, 32) = word.range(31, 0);
				cache_data[0] = word;
				cache_tag[0] = end_tag;
			}
			else{
				int512_dt word_start = rpao[start_tag];
				int512_dt word_end = rpao[end_tag];
				rpitem = (word_end.range(31, 0), word_start.range(511, 480));
				cache_data[0] = word_end;
				cache_tag[0] = end_tag;
			}

			rpao_stream << rpitem;
			
		}

		if(done_empty != 1 && frontier_empty == 1){
			done = inspect_done_stream.read();
			rpao_done_stream << 1;
		}
	}
}
*/


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

    stage2_main:
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
		}
	}

	rpao_done_stream << 1;
}

static void bfs_stage3(
		const int512_dt *ciao,
		hls::stream<int64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		const char level
		)
{
	uint1_dt rpao_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;

	// 4 bit offset, 14 bit index, 14 bit tag
	int512_dt cache_data[CACHE_SIZE];
	uint14_dt cache_tag[CACHE_SIZE];

#ifndef SW_EMU
	if(level == 0){
#endif
        stage3_init:
		for(size_t i = 0; i < CACHE_SIZE; i++){
        #pragma HLS pipeline
			cache_tag[i] = 0x3FFF;
		}
#ifndef SW_EMU
	}
#endif

    stage3_main_L1:
	while((rpao_empty != 1) || (done != 1)){
		rpao_empty = rpao_stream.empty();
		done_empty = rpao_done_stream.empty();
		
		if(rpao_empty != 1){
			int64_dt rpitem = rpao_stream.read();
			uint32_dt start = rpitem.range(31, 0);
			uint32_dt end = rpitem.range(63, 32);

            stage3_main_L0:
			for(uint32_dt i = start; i < end; i++){
			#pragma HLS LOOP_FLATTEN off
            #pragma HLS pipeline
				uint28_dt burst_addr = i.range(31, 4);
				uint4_dt offset = i.range(3 ,0);
				uint14_dt index = i.range(17, 4);
				uint14_dt tag = i.range(31, 18);
				uint14_dt _tag = cache_tag[index];
				int512_dt word = (_tag == tag) ? cache_data[index] : ciao[burst_addr];
				if(_tag != tag){
					cache_data[index] = word;
					cache_tag[index] = tag;
				}

				ciao_stream << word.range((offset+1)*32 - 1, offset * 32);
			}
		}

		if(done_empty != 1 && rpao_empty == 1){
			done = rpao_done_stream.read();
		}
	}

	ciao_done_stream << 1;
}


// Remove redundant vertices in ciao
static void bfs_squeeze_ciao(
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int> &ciao_squeeze_stream,
		hls::stream<uint1_dt> &ciao_squeeze_done_stream,
		const char level
		)
{
	int done = 0;
	int ciao_empty = 0;
	int done_empty = 0;

#ifdef SW_DEBUG 
	static int hit_sum = 0;
	static int ciao_sum = 0;
	int hit_cnt = 0;
	int ciao_cnt = 0;
#endif

	hash_tag_dt hash_table0[HASH_SIZE];
	hash_tag_dt hash_table1[HASH_SIZE];
	hash_tag_dt hash_table2[HASH_SIZE];
	hash_tag_dt hash_table3[HASH_SIZE];
	uint2_dt hash_pri = 0;
#ifndef SW_EMU
   	if(level == 0){
#endif
        squeeze_init:
		for (int i = 0; i < HASH_SIZE; i++){
        #pragma HLS pipeline
			hash_table0[i] = 0x3FFFF;
			hash_table1[i] = 0x3FFFF;
			hash_table2[i] = 0x3FFFF;
			hash_table3[i] = 0x3FFFF;
		}

#ifndef SW_EMU
	}
#endif

    squeeze_main:
	while(ciao_empty != 1 || done != 1){
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();

		if(ciao_empty != 1){
#ifdef SW_DEBUG
			ciao_cnt++;	
#endif
			uint32_dt vidx = ciao_stream.read();	
			hash_index_dt hash_idx = vidx.range(HASH_ID_WIDTH-1, 0);
			hash_tag_dt hash_tag = vidx.range(31, HASH_ID_WIDTH);
			hash_tag_dt hash_val0 = hash_table0[hash_idx];
			hash_tag_dt hash_val1 = hash_table1[hash_idx];
			hash_tag_dt hash_val2 = hash_table2[hash_idx];
			hash_tag_dt hash_val3 = hash_table3[hash_idx];

			if(hash_val0 != hash_tag && hash_val1 != hash_tag
			   && hash_val2 != hash_tag && hash_val3 != hash_tag)
			{
				ciao_squeeze_stream << vidx;
				if(hash_pri == 0){
					hash_table0[hash_idx] = hash_tag;
					hash_pri = 1;
				}
				else if(hash_pri == 1){
					hash_table1[hash_idx] = hash_tag;
					hash_pri = 2;
				}
				else if(hash_pri == 2){
					hash_table2[hash_idx] = hash_tag;
					hash_pri = 3;
				}
				else if(hash_pri == 3){
					hash_table3[hash_idx] = hash_tag;
					hash_pri = 0;
				}
			}
#ifdef SW_DEBUG
			else{
				hit_cnt++;
			}
#endif
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();
		}
	}

	ciao_squeeze_done_stream << 1;
#ifdef SW_DEBUG
	if(ciao_cnt > 0){
		ciao_sum += ciao_cnt;
		hit_sum += hit_cnt;
		printf("hash table hit %d  %0.3f\n", hit_cnt, 100.0*hit_cnt/ciao_cnt);
		printf("total hit %d, %0.3f\n", hit_sum, 100.0*hit_sum/ciao_sum);
		printf("---------------------------\n");
	}
#endif
}

// load depth for inspection 
static void bfs_stage4(
		const int512_dt *depth_for_expand,
		int512_dt *depth_for_update,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		const char level_plus1)
{
	// Cache data structure 6bit offset, 14bit index, 12bit tag
	bool cache_dirty[CACHE_SIZE];
	uint12_dt cache_tag[CACHE_SIZE];
	int512_dt cache_data[CACHE_SIZE];

#ifndef SW_EMU
	if(level_plus1 == 1){
#endif
        stage4_init:
		for(int i = 0; i < CACHE_SIZE; i++){
        #pragma HLS pipeline
			cache_tag[i] = 0xFFF;
			cache_dirty[i] = false;
		}
#ifndef SW_EMU
	}
#endif

	int done = 0;
	int ciao_empty = 0;
	int done_empty = 0;

    stage4_main:
    while(ciao_empty != 1 || done != 1){
    #pragma HLS pipeline

		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			uint32_dt vidx = ciao_stream.read();
			uint6_dt req_offset = vidx.range(5, 0);
			uint14_dt req_idx = vidx.range(19, 6);
			uint12_dt req_tag = vidx.range(31, 20);
			uint12_dt old_tag = cache_tag[req_idx];
			int512_dt cache_line = cache_data[req_idx];
			uint26_dt req_line_addr = vidx.range(31, 6);
			uint26_dt cache_line_addr = (old_tag, req_idx);

			// cache hit
			if(old_tag == req_tag){
				char d = cache_line.range(((req_offset+1)*8) - 1, (req_offset*8));
				if(d == -1){
					cache_data[req_idx].range(((req_offset+1)*8) - 1, (req_offset*8)) = level_plus1;
					cache_dirty[req_idx] = true;
				}
			}

			// cache miss
			else{
				bool dirty = cache_dirty[req_idx];
				if(dirty == true){	
					depth_for_update[cache_line_addr] = cache_line;
				}
				int512_dt new_cache_line = depth_for_expand[req_line_addr];
				char d = new_cache_line.range(((req_offset+1)*8) - 1, (req_offset*8));
				if(d == -1){
					new_cache_line.range(((req_offset+1)*8) - 1, (req_offset*8)) = level_plus1;
					cache_dirty[req_idx] = true;
				}
				else{
					cache_dirty[req_idx] = false;
				}

				cache_data[req_idx] = new_cache_line;
				cache_tag[req_idx] = req_tag;
			}
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();		
		}
	}

    stage4_wb:
	for(uint16_dt i = 0; i < CACHE_SIZE; i++){
    #pragma HLS pipeline
		bool dirty = cache_dirty[i];
		if(dirty == true){
			uint26_dt cache_line_addr = (cache_tag[i], i.range(13, 0));
			depth_for_update[cache_line_addr] = cache_data[i];
			cache_dirty[i] = false;
		}
	}
}

extern "C" {
void bfs(
		const int512_dt *depth_for_inspect,
		const int512_dt *depth_for_expand,
		int512_dt *depth_for_update,
		const int *rpao, 
		const int512_dt *ciao,
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
hls::stream<int> ciao_squeeze_stream;
hls::stream<uint1_dt> ciao_squeeze_done_stream;

//#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=frontier_stream depth=32
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=rpao_done_stream depth=4
#pragma HLS STREAM variable=rpao_stream depth=32
#pragma HLS STREAM variable=ciao_stream depth=64
#pragma HLS STREAM variable=ciao_done_stream depth=4
#pragma HLS STREAM variable=ciao_squeeze_stream depth=64
#pragma HLS STREAM variable=ciao_squeeze_done_stream depth=4

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, vertex_num);
bfs_stage1(frontier_size, depth_inspect_stream, inspect_done_stream, frontier_stream, vertex_num, level);
bfs_stage2(rpao, frontier_stream, inspect_done_stream, rpao_stream, rpao_done_stream);
bfs_stage3(ciao, rpao_stream, rpao_done_stream, ciao_stream, ciao_done_stream, level);
bfs_squeeze_ciao(ciao_stream, ciao_done_stream, 
		ciao_squeeze_stream, ciao_squeeze_done_stream, level);
bfs_stage4(depth_for_expand, depth_for_update, ciao_squeeze_stream, ciao_squeeze_done_stream, level_plus1);

}
}



