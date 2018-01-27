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

//#define SW_EMU
#define BUF_SIZE 64
#define SML_BUF_SIZE 16

#define RPAO_CACHE_SIZE 8
#define RPAO_OFFSET_WIDTH 3
#define RPAO_TAG_INIT_VAL 0x1FFFFFFF
#define RPAO_CACHE_LINE_WIDTH (RPAO_CACHE_SIZE * 64)
#define RPAO_TAG_WIDTH (32 - RPAO_OFFSET_WIDTH)
typedef ap_int<RPAO_CACHE_LINE_WIDTH> rpao_cache_dt;
typedef ap_uint<RPAO_TAG_WIDTH> rpao_tag_dt;
typedef ap_uint<RPAO_OFFSET_WIDTH> rpao_offset_dt;

#define CIAO_CACHE_SIZE 16
#define CIAO_OFFSET_WIDTH 4
#define CIAO_TAG_INIT_VAL 0xFFFFFFF
#define CIAO_TAG_WIDTH (32 - CIAO_OFFSET_WIDTH)
#define CIAO_CACHE_LINE_WIDTH (CIAO_CACHE_SIZE * 32)
typedef ap_uint<CIAO_OFFSET_WIDTH> ciao_offset_dt;
typedef ap_uint<CIAO_TAG_WIDTH> ciao_tag_dt;
typedef ap_int<CIAO_CACHE_LINE_WIDTH> ciao_cache_dt;

#define DEPTH_CACHE_SIZE 16384
#define DEPTH_OFFSET_WIDTH 6
#define DEPTH_INDEX_WIDTH 14
#define DEPTH_ADDR_WIDTH (32 - DEPTH_OFFSET_WIDTH)
#define DEPTH_TAG_INIT_VAL 0xFFF
#define DEPTH_TAG_WIDTH (32 - DEPTH_OFFSET_WIDTH - DEPTH_INDEX_WIDTH)
typedef ap_uint<DEPTH_OFFSET_WIDTH> depth_offset_dt;
typedef ap_uint<DEPTH_TAG_WIDTH> depth_tag_dt;
typedef ap_uint<DEPTH_INDEX_WIDTH> depth_index_dt;
typedef ap_uint<DEPTH_ADDR_WIDTH> depth_addr_dt;

#define HASH_SIZE (16384)
#define HASH_ID_WIDTH 14
#define HASH_TAG_WIDTH (32 - HASH_ID_WIDTH)
#define HASH_TAG_INIT_VAL 0x3FFFF
typedef ap_uint<HASH_ID_WIDTH> hash_index_dt;
typedef ap_uint<HASH_TAG_WIDTH> hash_tag_dt;

typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_uint<4> uint4_dt;
typedef ap_uint<5> uint5_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_uint<64> uint64_dt;
typedef ap_int<512> int512_dt;

static void read_depth(
		int512_dt *depth_b0, 
		int512_dt *depth_b1, 
		hls::stream<int512_dt> &depth_inspect_stream,
        int vertex_num)
{
	int512_dt buffer0[SML_BUF_SIZE];
	int512_dt buffer1[SML_BUF_SIZE];

	int word_num = vertex_num >> 6;
    read_depth: 
	for (uint32_dt i = 0; i < word_num; i += 2 * SML_BUF_SIZE){
		uint32_dt bank_idx = i >> 1;
		int len = SML_BUF_SIZE;
		if(i + 2 * SML_BUF_SIZE > word_num) {
			len = (word_num - i) >> 1; 
		}

        mem2depth_buffer:
		for(int j = 0; j < len; j++){
        #pragma HLS pipeline
			buffer0[j] = depth_b0[bank_idx + j];
			buffer1[j] = depth_b1[bank_idx + j];
		}

        depth0_buffer2stream:
		for(int j = 0; j < len; j++){
        #pragma HLS pipeline
			depth_inspect_stream << buffer0[j];
			depth_inspect_stream << buffer1[j];
		}

	}
}

static void reshape_depth(
		hls::stream<int512_dt> &depth_inspect_stream,
		hls::stream<char> &depth_inspect_char_stream,
		const int vertex_num
		)
{
	int512_dt word;
	for(int i = 0; i < vertex_num; i++){
    #pragma HLS pipeline
		int idx = i & 0x3F;
		if(idx == 0) word = depth_inspect_stream.read();
		depth_inspect_char_stream << word.range((idx + 1)*8 - 1, idx * 8);
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
		const int512_dt *rpao,
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &frontier_done_stream,
		hls::stream<uint64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream)
{
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;

	rpao_tag_dt cache_tag[1] = {RPAO_TAG_INIT_VAL};
	rpao_cache_dt cache_data[1];

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
			rpao_tag_dt tag = bank_vidx.range(31, RPAO_OFFSET_WIDTH);
			rpao_offset_dt offset = bank_vidx.range(RPAO_OFFSET_WIDTH - 1, 0);
			rpao_cache_dt _word = cache_data[0];
			rpao_tag_dt _tag = cache_tag[0];
			if(tag == _tag){
				rpao_stream << _word.range((offset + 1)*64 - 1, offset * 64);
			}
			else{
				rpao_cache_dt word = rpao[tag];
				rpao_stream << word.range((offset + 1)*64 - 1, offset * 64);
				cache_data[0] = word;
				cache_tag[0] = tag;
			}
		}

		if(frontier_empty == 1 && done_empty != 1){
			done = frontier_done_stream.read();
		}
	}
	rpao_done_stream << 1;
}

static void read_ngb(
		const int512_dt *ciao,
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

	ciao_cache_dt cache_data[1];
   	ciao_tag_dt cache_tag[1] = {CIAO_TAG_INIT_VAL};

    read_ngb_L0:
	while((rpao_empty != 1) || (done != 1)){
	#pragma HLS LOOP_FLATTEN off
		rpao_empty = rpao_stream.empty();
		done_empty = rpao_done_stream.empty();
		if(rpao_empty != 1){
			uint64_dt rpitem = rpao_stream.read();
			int start = rpitem.range(31, 0);
			int num = rpitem.range(63, 32);
			for(int i = 0; i < num; i++){
            #pragma HLS pipeline
				uint32_dt idx = start + i;
				ciao_tag_dt addr = idx.range(31, CIAO_OFFSET_WIDTH);
				ciao_offset_dt offset = idx.range(CIAO_OFFSET_WIDTH - 1 ,0);
				ciao_tag_dt tag = idx.range(31, CIAO_OFFSET_WIDTH);
				ciao_tag_dt _tag = cache_tag[0];
				ciao_cache_dt _word = cache_data[0];
				if(_tag == tag){
					ciao_stream << _word.range((offset+1)*32 - 1, offset * 32);
				}
				else{
					ciao_cache_dt word = ciao[addr];
					ciao_stream << word.range((offset+1)*32 - 1, offset * 32);
					cache_data[0] = word;
					cache_tag[0] = tag;
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

static void squeeze_ciao(
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int> &squeezed_ciao_stream,
		hls::stream<uint1_dt> &squeezed_ciao_done_stream,
		const char level
		)
{
	hash_tag_dt hash_table0[HASH_SIZE];
	hash_tag_dt hash_table1[HASH_SIZE];
	hash_tag_dt hash_table2[HASH_SIZE];
	hash_tag_dt hash_table3[HASH_SIZE];

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

	int done = 0;
	int ciao_empty = 0;
	int done_empty = 0;
	uint2_dt hash_pri = 0;

    squeeze_main:
	while(ciao_empty != 1 || done != 1){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();

		if(ciao_empty != 1){
			uint32_dt vidx = ciao_stream.read();	

			// Use this as the index for redundancy removal
			// This should change with the depth bank partition
			uint32_dt bank_vidx;
			bank_vidx.range(5, 0) = vidx.range(5, 0);
			bank_vidx.range(30, 6) = vidx.range(31, 7);
			bank_vidx.range(31, 31) = 0;

			hash_index_dt hash_idx = bank_vidx.range(HASH_ID_WIDTH - 1, 0);
			hash_tag_dt hash_tag = bank_vidx.range(31, HASH_ID_WIDTH);
			hash_tag_dt hash_val0 = hash_table0[hash_idx];
			hash_tag_dt hash_val1 = hash_table1[hash_idx];
			hash_tag_dt hash_val2 = hash_table2[hash_idx];
			hash_tag_dt hash_val3 = hash_table3[hash_idx];

			if(hash_val0 != hash_tag && hash_val1 != hash_tag
			   && hash_val2 != hash_tag && hash_val3 != hash_tag)
			{
				squeezed_ciao_stream << bank_vidx;
				if(hash_pri == 0){
					hash_table0[hash_idx] = hash_tag;
				}

				if(hash_pri == 1){
					hash_table1[hash_idx] = hash_tag;
				}

				if(hash_pri == 2){
					hash_table2[hash_idx] = hash_tag;
				}

				if(hash_pri == 3){
					hash_table3[hash_idx] = hash_tag;
				}

				hash_pri++;
			}
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();
		}
	}

	squeezed_ciao_done_stream << 1;
}


static void update_depth_s0(
		int512_dt *depth,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int> &vidx_update_stream,
		hls::stream<uint1_dt> &update_done_stream,
		char level
		){
	uint1_dt done = 0;
	uint1_dt ciao_empty = 0;
	uint1_dt done_empty = 0;

	int512_dt cache_data[DEPTH_CACHE_SIZE];
	depth_tag_dt cache_tag[DEPTH_CACHE_SIZE];
#ifndef SW_EMU
	if(level == 0){
#endif
        read_cache_init:
		for(int i = 0; i < DEPTH_CACHE_SIZE; i++){
        #pragma HLS PIPELINE II=1
			cache_tag[i] = DEPTH_TAG_INIT_VAL;
		}
#ifndef SW_EMU
	}
#endif

    update_depth:
	while((ciao_empty != 1) || (done != 1)){
    #pragma HLS LOOP FLATTEN OFF
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			uint32_dt vidx = ciao_stream.read();
			depth_offset_dt offset = vidx.range(DEPTH_OFFSET_WIDTH - 1, 0);
			depth_index_dt index = vidx.range(DEPTH_INDEX_WIDTH + DEPTH_OFFSET_WIDTH - 1, DEPTH_OFFSET_WIDTH);
			depth_addr_dt addr = vidx.range(31, DEPTH_OFFSET_WIDTH);
			depth_tag_dt tag = vidx.range(31, DEPTH_INDEX_WIDTH + DEPTH_OFFSET_WIDTH);
			depth_tag_dt _tag = cache_tag[index];

			// Note that this has been done in squeezing stage
			//bank_vidx.range(5, 0) = vidx.range(5, 0);
			//bank_vidx.range(30, 6) = vidx.range(31, 7);
			//bank_vidx.range(31, 31) = 0;
			char d;
			if(tag == _tag){
				int512_dt _word = cache_data[index];
				d = _word.range((offset + 1) * 8 - 1, offset * 8);
			}
			else{
				int512_dt word = depth[addr]; 
				d = word.range((offset + 1) * 8 - 1, offset * 8);
				cache_tag[index] = tag;
				cache_data[index] = word;
			}
			if(d == -1) vidx_update_stream << vidx;
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();	
		}
	}
	update_done_stream << 1;
}

static void update_depth_s1(
		int512_dt *depth,
		hls::stream<int> &vidx_update_stream,
		hls::stream<uint1_dt> &update_done_stream,
		char level
		){
	uint1_dt done = 0;
	uint1_dt update_empty = 0;
	uint1_dt done_empty = 0;

	int512_dt cache_data[DEPTH_CACHE_SIZE];
	depth_tag_dt cache_tag[DEPTH_CACHE_SIZE];
	uint1_dt cache_dirty[DEPTH_CACHE_SIZE];
#ifndef SW_EMU
	if(level == 0){
#endif
        write_cache_init:
		for(int i = 0; i < DEPTH_CACHE_SIZE; i++){
        #pragma HLS PIPELINE II=1
			cache_tag[i] = DEPTH_TAG_INIT_VAL;
			cache_dirty[i] = 0;
		}
#ifndef SW_EMU
	}
#endif

    update_depth:
	while((update_empty != 1) || (done != 1)){
    #pragma HLS LOOP FLATTEN OFF
    #pragma HLS pipeline
		update_empty = vidx_update_stream.empty();
		done_empty = update_done_stream.empty();
		if(update_empty != 1){
			uint32_dt vidx = vidx_update_stream.read();
			depth_offset_dt offset = vidx.range(DEPTH_OFFSET_WIDTH - 1, 0);
			depth_index_dt index = vidx.range(DEPTH_INDEX_WIDTH + DEPTH_OFFSET_WIDTH - 1, DEPTH_OFFSET_WIDTH);
			uint1_dt dirty = cache_dirty[index];
			int512_dt _word = depth[index];
			depth_tag_dt tag = vidx.range(31, DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH);
			depth_tag_dt _tag = cache_tag[index];
			depth_addr_dt addr = vidx.range(31, DEPTH_OFFSET_WIDTH);
			depth_addr_dt _addr;
			_addr.range(DEPTH_INDEX_WIDTH + DEPTH_TAG_WIDTH - 1, DEPTH_INDEX_WIDTH) = _tag;
			_addr.range(DEPTH_INDEX_WIDTH - 1, 0) = index;
			cache_dirty[index] = 1;

			if(tag == _tag){
				cache_data[index].range((offset + 1) * 8 - 1, offset*8) = level + 1;
			}
			else{
				int512_dt word = depth[addr];
				word.range((offset + 1)*8 - 1, offset * 8) = level + 1;
				if(cache_dirty[index] == 1){
					depth[_addr] = _word;
				}
				else{
				}
				cache_data[index] = word;
				cache_tag[index] = tag;
			}
		}

		if(done_empty != 1 && update_empty == 1){
			done = update_done_stream.read();	
		}
	}

	//write back the cache line in case it is not updated in memory
	depth_wb:
	for(uint32_dt i = 0; i < DEPTH_CACHE_SIZE; i++){
    #pragma HLS PIPELINE
		if(cache_dirty[i] == 1){
			depth_tag_dt _tag = cache_tag[i];
			depth_addr_dt _addr = (_tag, i.range(DEPTH_INDEX_WIDTH - 1, 0));
			depth[_addr] = cache_data[i];
			cache_dirty[i] = 0;
		}
	}
}

/*
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
*/

extern "C" {
void bfs(
		int512_dt *depth_inspect_b0,
		int512_dt *depth_inspect_b1,

		int512_dt *depth_update_b0,
		int512_dt *depth_update_b1,

		const int512_dt *rpao_b0,	
		const int512_dt *rpao_b1,	
		const int512_dt *rpao_b2,	
		const int512_dt *rpao_b3,	

		const int512_dt *ciao_b0,
		const int512_dt *ciao_b1,
		const int512_dt *ciao_b2,
		const int512_dt *ciao_b3,

		int *frontier_size,
		int512_dt *depth_read_b0,
		int512_dt *depth_read_b1,
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

hls::stream<int512_dt> depth_inspect_stream;
hls::stream<char> depth_inspect_char_stream;
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

hls::stream<int> squeezed_ciao_b0_stream;
hls::stream<int> squeezed_ciao_b1_stream;

hls::stream<uint1_dt> squeezed_ciao_b0_done_stream;
hls::stream<uint1_dt> squeezed_ciao_b1_done_stream;

hls::stream<int> vidx_update_b0_stream;
hls::stream<int> vidx_update_b1_stream;

hls::stream<uint1_dt> update_done_b0_stream;
hls::stream<uint1_dt> update_done_b1_stream;


#pragma HLS STREAM variable=depth_inspect_stream depth=16
#pragma HLS STREAM variable=depth_inspect_char_stream depth=32
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

#pragma HLS STREAM variable=squeezed_ciao_b0_stream depth=32
#pragma HLS STREAM variable=squeezed_ciao_b1_stream depth=32

#pragma HLS STREAM variable=squeezed_ciao_b0_done_stream depth=4
#pragma HLS STREAM variable=squeezed_ciao_b1_done_stream depth=4

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

reshape_depth(
		depth_inspect_stream,
		depth_inspect_char_stream,
		vertex_num
		);

frontier_analysis(
		frontier_size, 
		depth_inspect_char_stream, 
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

squeeze_ciao(
		merged_ciao_b0_stream,
		merged_ciao_b0_done_stream,
		squeezed_ciao_b0_stream,
		squeezed_ciao_b0_done_stream,
		level
		);

squeeze_ciao(
		merged_ciao_b1_stream,
		merged_ciao_b1_done_stream,
		squeezed_ciao_b1_stream,
		squeezed_ciao_b1_done_stream,
		level
		);

update_depth_s0(
		depth_read_b0,
		squeezed_ciao_b0_stream,
		squeezed_ciao_b0_done_stream,
		vidx_update_b0_stream,
		update_done_b0_stream,
		level
		);

update_depth_s0(
		depth_read_b1,
		squeezed_ciao_b1_stream,
		squeezed_ciao_b1_done_stream,
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

