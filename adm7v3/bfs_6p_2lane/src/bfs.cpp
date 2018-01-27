/**
 * File              : src/bfs.cpp
 * Author            : Cheng Liu <st.liucheng@gmail.com>
 * Date              : 13.11.2017
 * Last Modified Date: 13.11.2017
 * Last Modified By  : Cheng Liu <st.liucheng@gmail.com>
 */
#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define SW_EMU

#define HASH_SIZE (16384*4)
#define HASH_ID_WIDTH 16
#define HASH_TAG_WIDTH 16
#define HASH_TAG_INIT_VAL 0xFFFF
typedef ap_uint<HASH_ID_WIDTH> hash_index_dt;
typedef ap_uint<HASH_TAG_WIDTH> hash_tag_dt;

#define RPA_CACHE_SIZE 16
#define RPA_OFFSET_WIDTH 4
#define RPA_TAG_INIT_VAL 0xFFFFFFF
#define RPA_CACHE_LINE_WIDTH (RPA_CACHE_SIZE * 32)
#define RPA_TAG_WIDTH (32 - RPA_OFFSET_WIDTH)
typedef ap_int<RPA_CACHE_LINE_WIDTH> rpa_cache_dt;
typedef ap_uint<RPA_TAG_WIDTH> rpa_tag_dt;
typedef ap_uint<RPA_OFFSET_WIDTH> rpa_offset_dt;

#define CIA_CACHE_SIZE 16
#define CIA_OFFSET_WIDTH 4
#define CIA_TAG_INIT_VAL 0xFFFFFFF
#define CIA_TAG_WIDTH (32 - CIA_OFFSET_WIDTH)
#define CIA_CACHE_LINE_WIDTH (CIA_CACHE_SIZE * 32)
typedef ap_uint<CIA_OFFSET_WIDTH> cia_offset_dt;
typedef ap_uint<CIA_TAG_WIDTH> cia_tag_dt;
typedef ap_int<CIA_CACHE_LINE_WIDTH> cia_cache_dt;

#define DEPTH_RD_CACHE_SIZE 8192
#define DEPTH_RD_OFFSET_WIDTH 6
#define DEPTH_RD_INDEX_WIDTH 13
#define DEPTH_RD_TAG_WIDTH 13
#define DEPTH_RD_TAG_INIT_VAL 0x1FFF
#define DEPTH_RD_ADDR_WIDTH (DEPTH_RD_INDEX_WIDTH + DEPTH_RD_TAG_WIDTH)
typedef ap_uint<DEPTH_RD_OFFSET_WIDTH> depth_rd_offset_dt;
typedef ap_uint<DEPTH_RD_INDEX_WIDTH> depth_rd_index_dt;
typedef ap_uint<DEPTH_RD_TAG_WIDTH> depth_rd_tag_dt;
typedef ap_uint<DEPTH_RD_ADDR_WIDTH> depth_rd_addr_dt;

#define DEPTH_WR_CACHE_SIZE 16384
#define DEPTH_WR_OFFSET_WIDTH 6
#define DEPTH_WR_INDEX_WIDTH 14
#define DEPTH_WR_TAG_WIDTH 12
#define DEPTH_WR_TAG_INIT_VAL 0xFFF
#define DEPTH_WR_ADDR_WIDTH (DEPTH_WR_INDEX_WIDTH + DEPTH_WR_TAG_WIDTH)
typedef ap_uint<DEPTH_WR_OFFSET_WIDTH> depth_wr_offset_dt;
typedef ap_uint<DEPTH_WR_INDEX_WIDTH> depth_wr_index_dt;
typedef ap_uint<DEPTH_WR_TAG_WIDTH> depth_wr_tag_dt;
typedef ap_uint<DEPTH_WR_ADDR_WIDTH> depth_wr_addr_dt;

typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_uint<4> uint4_dt;
typedef ap_uint<5> uint5_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_int<32> int32_dt;
typedef ap_int<512> int512_dt;

// load depth for inspection 
static void bfs_stage0(
		const int32_dt *depth, 
		hls::stream<int32_dt> &depth_inspect_stream,
        int word_num)
{
	int32_dt buffer[16];
	int len = 16;
    stage0_main:
    for (uint32_dt i = 0; i < word_num; i += 16){
    #pragma HLS LOOP_FLATTEN off
		if(i + 16 > word_num) len = word_num - i;
		for(int j = 0; j < len; j++){
        #pragma HLS pipeline II=1
			buffer[j] = depth[i + j];
			depth_inspect_stream << buffer[j];
		}
	}
}

static void bfs_stage1(
		hls::stream<int32_dt> &depth_inspect_stream,
		hls::stream<int> frontier_stream0,
		hls::stream<int> frontier_stream1,
		hls::stream<int> frontier_stream2,
		hls::stream<int> frontier_stream3,
		hls::stream<uint1_dt> &inspect_done_stream0,
		hls::stream<uint1_dt> &inspect_done_stream1,
		hls::stream<uint1_dt> &inspect_done_stream2,
		hls::stream<uint1_dt> &inspect_done_stream3,
		const int word_num,
		const char level)
		)
{
	for(int i = 0; i < word_num; i++){
		int32_dt word = depth_inspect_stream.read();
		if(word.range(7, 0) == level) frontier_stream0 << (i << 2);
		if(word.range(15, 8) == level) frontier_stream0 << (i << 2 + 1);
		if(word.range(23, 16) == level) frontier_stream0 << (i << 2 + 2);
		if(word.range(31, 24) == level) frontier_stream0 << (i << 2 + 3);
	}
	inspect_done_stream0 << 1;
	inspect_done_stream1 << 1;
	inspect_done_stream2 << 1;
	inspect_done_stream3 << 1;
}

// Read rpao of the frontier 
static void bfs_stage2(
		const int64_dt *rpao_extend,
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int64_dt> &rpaoInfo_stream,
		hls::stream<uint1_dt> &rpao_done_stream
		)
{
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;

	while((frontier_empty != 1) || (done != 1)){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		frontier_empty = frontier_stream.empty();
	    done_empty = inspect_done_stream.empty();

		if(frontier_empty != 1){
			uint32_dt vidx = frontier_stream.read();
			int64_dt rpaoInfo = rpao_extend[vidx];
			rpaoInfo_stream << rpaoInfo;
		}

		if(done_empty != 1 && frontier_empty == 1){
			done = inspect_done_stream.read();
		}
	}
	rpao_done_stream << 1;
}



static void bfs_stage3(
		const cia_cache_dt *ciao,
		hls::stream<int64_dt> &rpaoInfo_stream,
		hls::stream<uint1_dt> &rpao_done_stream,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		const char level
		)
{
	uint1_dt rpao_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;

	cia_cache_dt cache_data[1];
   	cia_tag_dt cache_tag[1] = {CIA_TAG_INIT_VAL};

#ifdef CIAO_ANALYSIS
	int hit_cnt = 0;
	int ciao_cnt = 0;
	static int hit_sum = 0;
	static int ciao_sum = 0;
#endif

    stage3_main_L1:
	while((rpao_empty != 1) || (done != 1)){
	#pragma HLS LOOP_FLATTEN off
		rpao_empty = rpaoInfo_stream.empty();
		done_empty = rpao_done_stream.empty();
		
		if(rpao_empty != 1){
			int64_dt rpaoInfo = rpaoInfo_stream.read();
			uint32_dt start = rpaoInfo.range(31, 0);
			uint32_dt num = rpaoInfo.range(63, 32);

			for(uint32_dt i = 0; i < num; i++){
			#pragma HLS LOOP_FLATTEN off
            #pragma HLS pipeline
#ifdef CIAO_ANALYSIS
				ciao_cnt++;
#endif
				uint32_dt cidx = start + i; 
				cia_tag_dt addr = cidx.range(31, CIA_OFFSET_WIDTH);
				cia_offset_dt offset = cidx.range(CIA_OFFSET_WIDTH - 1 ,0);
				cia_tag_dt tag = cidx.range(31, CIA_OFFSET_WIDTH);
				cia_tag_dt _tag = cache_tag[0];
				cia_cache_dt word_old = cache_data[0];
				if(_tag == tag){
					ciao_stream << word_old.range((offset+1)*32 - 1, offset * 32);
#ifdef CIAO_ANALYSIS
					hit_cnt++;
#endif
				}
				else{
					cia_cache_dt word = ciao[addr];
					ciao_stream << word.range((offset+1)*32 - 1, offset * 32);
					cache_data[0] = word;
					cache_tag[0] = tag;
				}
			}
		}

		if(done_empty != 1 && rpao_empty == ){
			done = rpao_done_stream.read();
		}
	}

	ciao_done_stream << 1;
#ifdef CIAO_ANALYSIS
	hit_sum += hit_cnt;
	ciao_sum += ciao_cnt;
	if(ciao_cnt > 0 && ciao_sum > 0){
		printf("Level = %d, hit_cnt = %d, ciao_sum =%d, ciao cache hit rate = %f\n", 
				(int)level, hit_cnt, ciao_cnt, hit_cnt*1.0/ciao_cnt);
		printf("Total ciao cache hit rate = %f\n", hit_sum * 1.0/ciao_sum);
	}
#endif

}


// Remove redundant vertices in ciao
static void bfs_squeeze_ciao(
		hls::stream<int> &ciao_stream0,
		hls::stream<int> &ciao_stream1,
		hls::stream<int> &ciao_stream2,
		hls::stream<int> &ciao_stream3,
		hls::stream<uint1_dt> &ciao_done_stream0,
		hls::stream<uint1_dt> &ciao_done_stream1,
		hls::stream<uint1_dt> &ciao_done_stream2,
		hls::stream<uint1_dt> &ciao_done_stream3,
		hls::stream<int> &ciao_squeeze_stream0,
		hls::stream<int> &ciao_squeeze_stream1,
		hls::stream<int> &ciao_squeeze_stream2,
		hls::stream<int> &ciao_squeeze_stream3,
		hls::stream<uint1_dt> &ciao_squeeze_done_stream0,
		hls::stream<uint1_dt> &ciao_squeeze_done_stream1,
		hls::stream<uint1_dt> &ciao_squeeze_done_stream2,
		hls::stream<uint1_dt> &ciao_squeeze_done_stream3,
		const char level
		)
{
	int done0 = 0;
	int done1 = 0;
	int done2 = 0;
	int done3 = 0;
	int ciao_empty0 = 0;
	int ciao_empty1 = 0;
	int ciao_empty2 = 0;
	int ciao_empty3 = 0;
	int done_empty0 = 0;
	int done_empty1 = 0;
	int done_empty2 = 0;
	int done_empty3 = 0;

#ifdef HASH_ANALYSIS
	static int hit_sum = 0;
	static int ciao_sum = 0;
	int hit_cnt = 0;
	int ciao_cnt = 0;
#endif

	hash_tag_dt hash_table0[HASH_SIZE];
//#pragma HLS RESOURCE variable=hash_table0 core=RAM_2P_BRAM latency=1
//#pragma HLS DEPENDENCE variable=hash_table0 inter false
//#pragma HLS DEPENDENCE variable=hash_table0 intra raw true

	hash_tag_dt hash_table1[HASH_SIZE];
//#pragma HLS RESOURCE variable=hash_table1 core=RAM_2P_BRAM latency=1
//#pragma HLS DEPENDENCE variable=hash_table1 inter false
//#pragma HLS DEPENDENCE variable=hash_table1 intra raw true

	hash_tag_dt hash_table2[HASH_SIZE];
//#pragma HLS RESOURCE variable=hash_table2 core=RAM_2P_BRAM latency=1
//#pragma HLS DEPENDENCE variable=hash_table2 inter false
//#pragma HLS DEPENDENCE variable=hash_table2 intra raw true

	hash_tag_dt hash_table3[HASH_SIZE];
//#pragma HLS RESOURCE variable=hash_table3 core=RAM_2P_BRAM latency=1
//#pragma HLS DEPENDENCE variable=hash_table3 inter false
//#pragma HLS DEPENDENCE variable=hash_table3 intra true

#ifndef SW_EMU
   	if(level == 0){
#endif
        squeeze_init:
		for (int i = 0; i < HASH_SIZE; i++){
        #pragma HLS pipeline II=1
			hash_table0[i] = HASH_TAG_INIT_VAL;
			hash_table1[i] = HASH_TAG_INIT_VAL;
			hash_table2[i] = HASH_TAG_INIT_VAL;
			hash_table3[i] = HASH_TAG_INIT_VAL;
		}
#ifndef SW_EMU
	}
#endif

	while(ciao_empty0 != 1 || done0 != 1 
			|| ciao_empty1 != 1 || done1 != 1 
			|| ciao_empty2 != 1 || done2 != 1 
			|| ciao_empty3 != 1 || done3 != 1)
	{
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		ciao_empty0 = ciao_stream0.empty();
		ciao_empty1 = ciao_stream1.empty();
		ciao_empty2 = ciao_stream2.empty();
		ciao_empty3 = ciao_stream3.empty();
		done_empty0 = ciao_done_stream0.empty();
		done_empty1 = ciao_done_stream1.empty();
		done_empty2 = ciao_done_stream2.empty();
		done_empty3 = ciao_done_stream3.empty();

		// stream0 squeezing
		if(ciao_empty0 != 1){
#ifdef HASH_ANALYSIS
			ciao_cnt++;	
#endif
			uint32_dt vidx = ciao_stream0.read();	
			hash_index_dt hash_idx = vidx.range(HASH_ID_WIDTH-1, 0);
			hash_tag_dt hash_tag = vidx.range(31, HASH_ID_WIDTH);
			hash_tag_dt hash_val0 = hash_table0[hash_idx];
			hash_tag_dt hash_val1 = hash_table1[hash_idx];
			hash_tag_dt hash_val2 = hash_table2[hash_idx];
			hash_tag_dt hash_val3 = hash_table3[hash_idx];

			if(hash_val0 != hash_tag && hash_val1 != hash_tag
			   && hash_val2 != hash_tag && hash_val3 != hash_tag)
			{
				ciao_squeeze_stream0 << vidx;
				hash_table0[hash_idx] = hash_tag;
			}
#ifdef HASH_ANALYSIS
			else{
				hit_cnt++;
			}
#endif
		}

		// stream1 squeezing
		if(ciao_empty1 != 1){
#ifdef HASH_ANALYSIS
			ciao_cnt++;	
#endif
			uint32_dt vidx = ciao_stream1.read();	
			hash_index_dt hash_idx = vidx.range(HASH_ID_WIDTH-1, 0);
			hash_tag_dt hash_tag = vidx.range(31, HASH_ID_WIDTH);
			hash_tag_dt hash_val0 = hash_table0[hash_idx];
			hash_tag_dt hash_val1 = hash_table1[hash_idx];
			hash_tag_dt hash_val2 = hash_table2[hash_idx];
			hash_tag_dt hash_val3 = hash_table3[hash_idx];

			if(hash_val0 != hash_tag && hash_val1 != hash_tag
					&& hash_val2 != hash_tag && hash_val3 != hash_tag)
			{
				ciao_squeeze_stream1 << vidx;
				hash_table1[hash_idx] = hash_tag;
			}
#ifdef HASH_ANALYSIS
			else{
				hit_cnt++;
			}
#endif
		}

		// stream2 squeezing
		if(ciao_empty2 != 1){
#ifdef HASH_ANALYSIS
			ciao_cnt++;	
#endif
			uint32_dt vidx = ciao_stream2.read();	
			hash_index_dt hash_idx = vidx.range(HASH_ID_WIDTH-1, 0);
			hash_tag_dt hash_tag = vidx.range(31, HASH_ID_WIDTH);
			hash_tag_dt hash_val0 = hash_table0[hash_idx];
			hash_tag_dt hash_val1 = hash_table1[hash_idx];
			hash_tag_dt hash_val2 = hash_table2[hash_idx];
			hash_tag_dt hash_val3 = hash_table3[hash_idx];

			if(hash_val0 != hash_tag && hash_val1 != hash_tag
					&& hash_val2 != hash_tag && hash_val3 != hash_tag)
			{
				ciao_squeeze_stream2 << vidx;
				hash_table2[hash_idx] = hash_tag;
			}
#ifdef HASH_ANALYSIS
			else{
				hit_cnt++;
			}
#endif
		}



		// stream2 squeezing
		if(ciao_empty3 != 1){
#ifdef HASH_ANALYSIS
			ciao_cnt++;	
#endif
			uint32_dt vidx = ciao_stream3.read();	
			hash_index_dt hash_idx = vidx.range(HASH_ID_WIDTH-1, 0);
			hash_tag_dt hash_tag = vidx.range(31, HASH_ID_WIDTH);
			hash_tag_dt hash_val0 = hash_table0[hash_idx];
			hash_tag_dt hash_val1 = hash_table1[hash_idx];
			hash_tag_dt hash_val2 = hash_table2[hash_idx];
			hash_tag_dt hash_val3 = hash_table3[hash_idx];

			if(hash_val0 != hash_tag && hash_val1 != hash_tag
			   && hash_val2 != hash_tag && hash_val3 != hash_tag)
			{
				ciao_squeeze_stream3 << vidx;
				hash_table3[hash_idx] = hash_tag;
			}
#ifdef HASH_ANALYSIS
			else{
				hit_cnt++;
			}
#endif
		}


		if(done_empty0 != 1 && ciao_empty0 == 1){
			done0 = ciao_done_stream0.read();
		}
		if(done_empty1 != 1 && ciao_empty1 == 1){
			done1 = ciao_done_stream1.read();
		}
		if(done_empty2 != 1 && ciao_empty2 == 1){
			done2 = ciao_done_stream2.read();
		}
		if(done_empty3 != 1 && ciao_empty3 == 1){
			done3 = ciao_done_stream3.read();
		}
	}

	ciao_squeeze_done_stream0 << 1;
	ciao_squeeze_done_stream1 << 1;
	ciao_squeeze_done_stream2 << 1;
	ciao_squeeze_done_stream3 << 1;
#ifdef HASH_ANALYSIS
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
		char *depth,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		const char level_plus1)
{
	int vidx;
	char ngb_depth;

	int done = 0;
	int ciao_empty = 0;
	int done_empty = 0;
    while(ciao_empty != 1 || done != 1){
        #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			vidx = ciao_stream.read();
			ngb_depth = depth[vidx];
			if(ngb_depth == -1){
				depth[vidx] = level_plus1;
			}
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();
		}
	}
}




extern "C" {
void bfs(
		const int512_dt *depth_for_inspect,
		const int512_dt *depthA_for_expand,
		const int512_dt *depthB_for_expand,
		int512_dt *depth_for_update,
		const rpa_cache_dt *rpaoAA, 
		const rpa_cache_dt *rpaoAB, 
		const rpa_cache_dt *rpaoBA, 
		const rpa_cache_dt *rpaoBB,
		const int512_dt *ciaoA,
		const int512_dt *ciaoB,
		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depthA_for_expand offset=slave bundle=gmem11
#pragma HLS INTERFACE m_axi port=depthB_for_expand offset=slave bundle=gmem12
#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=rpaoAA offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=rpaoAB offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=rpaoBA offset=slave bundle=gmem33
#pragma HLS INTERFACE m_axi port=rpaoBB offset=slave bundle=gmem34

#pragma HLS INTERFACE m_axi port=ciaoA offset=slave bundle=gmem41
#pragma HLS INTERFACE m_axi port=ciaoB offset=slave bundle=gmem42
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem5
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

char level_plus1 = level + 1;
int word_num = vertex_num >> 2;

hls::stream<int512_dt> depth_inspect_stream;
hls::stream<char> depth_inspect_char_stream;
hls::stream<uint1_dt> inspect_done_stream;
hls::stream<int> frontier_stream;

hls::stream<uint1_dt> frontierA_done_stream;
hls::stream<uint1_dt> frontierB_done_stream;
hls::stream<int> frontierA_stream;
hls::stream<int> frontierB_stream;
hls::stream<uint32_dt> startA_stream;
hls::stream<uint32_dt> startB_stream;
hls::stream<uint32_dt> endA_stream;
hls::stream<uint32_dt> endB_stream;
hls::stream<uint1_dt> rpaoA_done_stream;
hls::stream<uint1_dt> rpaoB_done_stream;
hls::stream<int> ciaoA_stream;
hls::stream<int> ciaoB_stream;
hls::stream<uint1_dt> ciaoA_done_stream;
hls::stream<uint1_dt> ciaoB_done_stream;
hls::stream<int> ngbA_stream;
hls::stream<int> ngbB_stream;
hls::stream<uint1_dt> ngbA_done_stream;
hls::stream<uint1_dt> ngbB_done_stream;
hls::stream<int> ciaoA_squeeze_stream;
hls::stream<uint1_dt> ciaoA_squeeze_done_stream;
hls::stream<int> ciaoB_squeeze_stream;
hls::stream<uint1_dt> ciaoB_squeeze_done_stream;

hls::stream<int> ngb_stream;
hls::stream<uint1_dt> ngb_done_stream;

#pragma HLS STREAM variable=depth_inspect_stream depth=64
#pragma HLS STREAM variable=depth_inspect_char_stream depth=64
#pragma HLS STREAM variable=frontier_stream depth=64
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=frontierA_stream depth=64
#pragma HLS STREAM variable=frontierB_stream depth=64
#pragma HLS STREAM variable=frontierA_done_stream depth=4
#pragma HLS STREAM variable=frontierB_done_stream depth=4
#pragma HLS STREAM variable=rpaoA_done_stream depth=4
#pragma HLS STREAM variable=rpaoB_done_stream depth=4
#pragma HLS STREAM variable=startA_stream depth=64
#pragma HLS STREAM variable=startB_stream depth=64
#pragma HLS STREAM variable=endA_stream depth=64
#pragma HLS STREAM variable=endB_stream depth=64
#pragma HLS STREAM variable=ciaoA_stream depth=64
#pragma HLS STREAM variable=ciaoB_stream depth=64
#pragma HLS STREAM variable=ciaoA_done_stream depth=4
#pragma HLS STREAM variable=ciaoB_done_stream depth=4
#pragma HLS STREAM variable=ngbA_stream depth=64
#pragma HLS STREAM variable=ngbB_stream depth=64
#pragma HLS STREAM variable=ngb_stream depth=64
#pragma HLS STREAM variable=ngbA_done_stream depth=4
#pragma HLS STREAM variable=ngbB_done_stream depth=4
#pragma HLS STREAM variable=ngb_done_stream depth=4
#pragma HLS STREAM variable=ciaoA_squeeze_stream depth=64
#pragma HLS STREAM variable=ciaoB_squeeze_stream depth=64
#pragma HLS STREAM variable=ciaoA_squeeze_done_stream depth=4
#pragma HLS STREAM variable=ciaoB_squeeze_done_stream depth=4

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, word_num);
bfs_stage1(
		hls::stream<int32_dt> &depth_inspect_stream,
		hls::stream<int> frontier_stream0,
		hls::stream<int> frontier_stream1,
		hls::stream<int> frontier_stream2,
		hls::stream<int> frontier_stream3,
		hls::stream<uint1_dt> &inspect_done_stream0,
		hls::stream<uint1_dt> &inspect_done_stream1,
		hls::stream<uint1_dt> &inspect_done_stream2,
		hls::stream<uint1_dt> &inspect_done_stream3,
		const int word_num,
		const char level)

bfs_stage0_1(depth_inspect_stream, depth_inspect_char_stream, vertex_num);
bfs_stage1(frontier_size, depth_inspect_char_stream, inspect_done_stream, frontier_stream, vertex_num, level);
//bfs_stage0(depth_for_inspect, depth_inspect_stream, word_num);
//bfs_stage1(frontier_size, depth_inspect_stream, inspect_done_stream, frontier_stream, word_num, level);


// frontier splitter
frontier_splitter(frontier_stream, inspect_done_stream, 
		frontierA_stream, frontierB_stream, frontierA_done_stream, frontierB_done_stream);

// branch A
bfs_stage2(rpaoAA, rpaoAB, frontierA_stream, frontierA_done_stream, 
		startA_stream, endA_stream, rpaoA_done_stream);
bfs_stage3(ciaoA, startA_stream, endA_stream, rpaoA_done_stream, 
		ciaoA_stream, ciaoA_done_stream, level);
bfs_squeeze_ciao(ciaoA_stream, ciaoA_done_stream, 
		ciaoA_squeeze_stream, ciaoA_squeeze_done_stream, level);
bfs_stage4(depthA_for_expand, ciaoA_squeeze_stream, ciaoA_squeeze_done_stream, 
		ngbA_stream, ngbA_done_stream, level);

// branch B
bfs_stage2(rpaoBA, rpaoBB, frontierB_stream, frontierB_done_stream, 
		startB_stream, endB_stream, rpaoB_done_stream);
bfs_stage3(ciaoB, startB_stream, endB_stream, rpaoB_done_stream, 
		ciaoB_stream, ciaoB_done_stream, level);
bfs_squeeze_ciao(ciaoB_stream, ciaoB_done_stream, 
		ciaoB_squeeze_stream, ciaoB_squeeze_done_stream, level);
bfs_stage4(depthB_for_expand, ciaoB_squeeze_stream, ciaoB_squeeze_done_stream, 
		ngbB_stream, ngbB_done_stream, level);


//ngb stream merger
ngb_merger(ngbA_stream, ngbB_stream, ngbA_done_stream, 
		ngbB_done_stream, ngb_stream, ngb_done_stream);

bfs_stage5(depth_for_update, ngb_stream, ngb_done_stream, level_plus1, level);
}
}



