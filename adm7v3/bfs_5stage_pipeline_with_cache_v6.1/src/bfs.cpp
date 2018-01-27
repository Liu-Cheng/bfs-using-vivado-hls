#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define SW_EMU
#define RPAO_ANALYSIS
//#define SW_ANALYSIS

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
typedef ap_uint<20> uint20_dt;
typedef ap_uint<26> uint26_dt;
typedef ap_uint<28> uint28_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<512> int512_dt;
typedef ap_uint<LOG2_LEN> cache_offset_dt;
typedef ap_uint<INDEX_WIDTH> cache_index_dt;
typedef ap_uint<TAG_WIDTH> cache_tag_dt;

// load depth for inspection 
static void bfs_stage0(
		const int512_dt *depth, 
		hls::stream<int512_dt> &depth_inspect_stream,
        int word_num)
{
    stage0_main:
    for (uint32_dt i = 0; i < word_num; i++){
        #pragma HLS pipeline II=1
		depth_inspect_stream << depth[i];
	}
}


// inspect depth for frontier 
static void bfs_stage1(
		int *frontier_size,
		hls::stream<int512_dt> &depth_inspect_stream, 
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontier_stream,
		int word_num,
		const int level)
{
	int counter = 0;
    stage1_main:
    for (int i = 0; i < word_num; i++){
		int512_dt word = depth_inspect_stream.read();
		for(int j = 0; j < 64; j++){
        #pragma HLS pipeline II=1
			char d = word.range((j+1) * 8 - 1, j * 8);
			if(d == level){
				frontier_stream << ((i << 6) + j);
				counter++;
			}
		}
	}

	inspect_done_stream << 1;
	*frontier_size = counter;
}

// Read rpao of the frontier 
static void bfs_stage2(
		const int *rpao,
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<uint32_dt> &start_stream,
		hls::stream<uint32_dt> &end_stream,
		hls::stream<uint1_dt> &rpao_done_stream)
{
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;
	int last_vidx[1] = {-1};
	int last_rpao[1] = {-1};
#ifdef RPAO_ANALYSIS
	int read_num = 0;
	int hit_num = 0;
	static int hit_sum = 0;
	static int read_sum = 0;
#endif

    stage2_main:
	while((frontier_empty != 1) || (done != 1)){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		frontier_empty = frontier_stream.empty();
	    done_empty = inspect_done_stream.empty();

		if(frontier_empty != 1){
			int vidx = frontier_stream.read();
			int vidx_plus1 = vidx + 1;
			if(vidx == last_vidx[0]){
#ifdef RPAO_ANALYSIS
				hit_num++;
#endif
				start_stream << last_rpao[0];
			}
			else{
				start_stream << rpao[vidx];
			}

			uint32_dt val = rpao[vidx_plus1];
#ifdef RPAO_ANALYSIS
			read_num++;
			read_num++;
#endif
			end_stream << val;
			last_vidx[0] = vidx_plus1;
			last_rpao[0] = val;
		}

		if(done_empty != 1 && frontier_empty == 1){
			done = inspect_done_stream.read();
		}
	}
#ifdef RPAO_ANALYSIS
	read_sum += read_num;
	hit_sum += hit_num;
	if(read_num > 0 && read_sum > 0){
		printf("rpao hit rate: %f\n", hit_num * 1.0/read_num);
		printf("total rpao hit rate: %f\n", hit_sum * 1.0/read_sum);
	}
#endif

	rpao_done_stream << 1;
}


static void bfs_stage3(
		const int512_dt *ciao,
		hls::stream<uint32_dt> &start_stream,
		hls::stream<uint32_dt> &end_stream,
		hls::stream<uint1_dt> &rpao_done_stream,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		const char level
		)
{
	uint1_dt start_empty = 0;
	uint1_dt end_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;

	// 4 bit offset, 14 bit index, 14 bit tag
	int512_dt cache_data[CACHE_SIZE];
	uint14_dt cache_tag[CACHE_SIZE];
#ifdef SW_ANALYSIS
	int hit_cnt = 0;
	int ciao_cnt = 0;
	static int hit_sum = 0;
	static int ciao_sum = 0;
#endif

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
	while((start_empty != 1 || end_empty != 1) || (done != 1)){
	#pragma HLS LOOP_FLATTEN off
		start_empty = start_stream.empty();
		end_empty = end_stream.empty();
		done_empty = rpao_done_stream.empty();
		
		if(start_empty != 1 && end_empty != 1){
			uint32_dt start = start_stream.read();
			uint32_dt end = end_stream.read();

            stage3_main_L0:
			for(uint32_dt i = start; i < end; i++){
			#pragma HLS LOOP_FLATTEN off
            #pragma HLS pipeline
#ifdef SW_ANALYSIS
				ciao_cnt++;
#endif
				uint28_dt burst_addr = i.range(31, 4);
				uint4_dt offset = i.range(3 ,0);
				uint14_dt index = i.range(17, 4);
				uint14_dt tag = i.range(31, 18);
				uint14_dt _tag = cache_tag[index];
				if(_tag == tag){
					ciao_stream << cache_data[index].range((offset+1)*32 - 1, offset * 32);
#ifdef SW_ANALYSIS
					hit_cnt++;
#endif
				}
				else{
					int512_dt word = ciao[burst_addr];
					ciao_stream << word.range((offset+1)*32 - 1, offset * 32);
					cache_data[index] = word;
					cache_tag[index] = tag;
				}

			}
		}

		if(done_empty != 1 && start_empty == 1 && end_empty == 1){
			done = rpao_done_stream.read();
		}
	}

	ciao_done_stream << 1;
#ifdef SW_ANALYSIS
	hit_sum += hit_cnt;
	ciao_sum += ciao_cnt;
	if(ciao_cnt > 0 && ciao_sum > 0){
		printf("Level = %d, hit_cnt = %d, ciao cache hit rate = %f\n", (int)level, hit_cnt, hit_cnt*1.0/ciao_cnt);
		printf("Total ciao cache hit rate = %f\n", hit_sum * 1.0/ciao_sum);
	}
#endif

}

/*
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
	#pragma HLS LOOP_FLATTEN off
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
#ifdef SW_ANALYSIS
	int hit_cnt = 0;
	int ciao_cnt = 0;
	static int hit_sum = 0;
	static int ciao_sum = 0;
#endif

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
	#pragma HLS LOOP_FLATTEN off
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
#ifdef SW_ANALYSIS
				ciao_cnt++;
#endif
				uint28_dt burst_addr = i.range(31, 4);
				uint4_dt offset = i.range(3 ,0);
				uint14_dt index = i.range(17, 4);
				uint14_dt tag = i.range(31, 18);
				uint14_dt _tag = cache_tag[index];
				if(_tag == tag){
					ciao_stream << cache_data[index].range((offset+1)*32 - 1, offset * 32);
#ifdef SW_ANALYSIS
					hit_cnt++;
#endif
				}
				else{
					int512_dt word = ciao[burst_addr];
					ciao_stream << word.range((offset+1)*32 - 1, offset * 32);
					cache_data[index] = word;
					cache_tag[index] = tag;
				}

			}
		}

		if(done_empty != 1 && rpao_empty == 1){
			done = rpao_done_stream.read();
		}
	}

	ciao_done_stream << 1;
#ifdef SW_ANALYSIS
	hit_sum += hit_cnt;
	ciao_sum += ciao_cnt;
	if(ciao_cnt > 0 && ciao_sum > 0){
		printf("Level = %d, hit_cnt = %d, ciao cache hit rate = %f\n", (int)level, hit_cnt, hit_cnt*1.0/ciao_cnt);
		printf("Total ciao cache hit rate = %f\n", hit_sum * 1.0/ciao_sum);
	}
#endif

}
*/

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

#ifdef SW_ANALYSIS
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
        #pragma HLS pipeline II=1
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
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();

		if(ciao_empty != 1){
#ifdef SW_ANALYSIS
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
				}
				else if(hash_pri == 1){
					hash_table1[hash_idx] = hash_tag;
				}
				else if(hash_pri == 2){
					hash_table2[hash_idx] = hash_tag;
				}
				else if(hash_pri == 3){
					hash_table3[hash_idx] = hash_tag;
				}
				hash_pri++;
			}
#ifdef SW_ANALYSIS
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
#ifdef SW_ANALYSIS
	if(ciao_cnt > 0){
		ciao_sum += ciao_cnt;
		hit_sum += hit_cnt;
		printf("hash table hit %d  %0.3f\n", hit_cnt, 100.0*hit_cnt/ciao_cnt);
		printf("total hash table hit %d, %0.3f\n", hit_sum, 100.0*hit_sum/ciao_sum);
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
		for(int i = 0; i < CACHE_SIZE; i++){
        #pragma HLS pipeline II=1
			cache_tag[i] = 0xFFF;
			cache_dirty[i] = false;
		}
#ifndef SW_EMU
	}
#endif

	int done = 0;
	int ciao_empty = 0;
	int done_empty = 0;

    while(ciao_empty != 1 || done != 1){
	#pragma HLS LOOP_FLATTEN off
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
int word_num = vertex_num >> 6;

hls::stream<int512_dt> depth_inspect_stream;
hls::stream<uint1_dt> inspect_done_stream;
hls::stream<int> frontier_stream;
//hls::stream<int64_dt> rpao_stream;
hls::stream<uint32_dt> start_stream;
hls::stream<uint32_dt> end_stream;
hls::stream<uint1_dt> rpao_done_stream;
hls::stream<int> ciao_stream;
hls::stream<uint1_dt> ciao_done_stream;
hls::stream<int> ciao_squeeze_stream;
hls::stream<uint1_dt> ciao_squeeze_done_stream;

#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=frontier_stream depth=32
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=rpao_done_stream depth=4
//#pragma HLS STREAM variable=rpao_stream depth=32
#pragma HLS STREAM variable=start_stream depth=32
#pragma HLS STREAM variable=end_stream depth=32
#pragma HLS STREAM variable=ciao_stream depth=64
#pragma HLS STREAM variable=ciao_done_stream depth=4
#pragma HLS STREAM variable=ciao_squeeze_stream depth=64
#pragma HLS STREAM variable=ciao_squeeze_done_stream depth=4

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, word_num);
bfs_stage1(frontier_size, depth_inspect_stream, inspect_done_stream, frontier_stream, word_num, level);
bfs_stage2(rpao, frontier_stream, inspect_done_stream, start_stream, end_stream, rpao_done_stream);
bfs_stage3(ciao, start_stream, end_stream, rpao_done_stream, ciao_stream, ciao_done_stream, level);
bfs_squeeze_ciao(ciao_stream, ciao_done_stream, 
		ciao_squeeze_stream, ciao_squeeze_done_stream, level);
bfs_stage4(depth_for_expand, depth_for_update, ciao_squeeze_stream, ciao_squeeze_done_stream, level_plus1);

}
}



