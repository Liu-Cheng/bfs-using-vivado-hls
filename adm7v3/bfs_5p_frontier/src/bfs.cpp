/**
 * File              : src/bfs.cpp
 * Author            : Cheng Liu <st.liucheng@gmail.com>
 * Date              : 13.11.2017
 * Last Modified Date: 02.12.2017
 * Last Modified By  : Cheng Liu <st.liucheng@gmail.com>
 */

#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define BUFFER_SIZE 16

#define BITMAP_MEM_DEPTH (8*1024*1024) 

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

#define DEPTH_CACHE_SIZE 16384
#define DEPTH_OFFSET_WIDTH 6
#define DEPTH_INDEX_WIDTH 14
#define DEPTH_TAG_WIDTH 12
#define DEPTH_TAG_INIT_VAL 0xFFF
#define DEPTH_ADDR_WIDTH (DEPTH_INDEX_WIDTH + DEPTH_TAG_WIDTH)
typedef ap_uint<DEPTH_OFFSET_WIDTH> depth_offset_dt;
typedef ap_uint<DEPTH_INDEX_WIDTH> depth_index_dt;
typedef ap_uint<DEPTH_TAG_WIDTH> depth_tag_dt;
typedef ap_uint<DEPTH_ADDR_WIDTH> depth_addr_dt;


typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_uint<4> uint4_dt;
typedef ap_uint<6> uint6_dt;
typedef ap_uint<16> uint16_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_int<512> int512_dt;

// inspect depth for frontier 
static void read_frontier(
		const int frontier_size,
		int *frontier,
		hls::stream<int> &frontier_stream
		)
{
    read_frontier:
    for (int i = 0; i < frontier_size; i++){
    #pragma HLS pipeline II=1
		frontier_stream << frontier[i];
	}
}

// Read rpao of the frontier 
static void read_csr_rpa(
		const int frontier_size,
		const rpa_cache_dt *rpaoA,
		const rpa_cache_dt *rpaoB,
		hls::stream<int> &frontier_stream,
		hls::stream<uint32_dt> &start_stream,
		hls::stream<uint32_dt> &end_stream)
{
	rpa_tag_dt tag = RPA_TAG_INIT_VAL;
	static rpa_cache_dt cache_data;

#ifdef RPAO_ANALYSIS
	int read_num = 0;
	int hit_num = 0;
	static int hit_sum = 0;
	static int read_sum = 0;
#endif

    read_csr_rpa:
	for(int i = 0; i < frontier_size; i++){
    #pragma HLS pipeline II=1
		uint32_dt vidx = frontier_stream.read();
		uint32_dt vidx_plus1 = vidx + 1;
		rpa_tag_dt start_tag = vidx.range(31, RPA_OFFSET_WIDTH);
		rpa_tag_dt end_tag = vidx_plus1.range(31, RPA_OFFSET_WIDTH);
		rpa_offset_dt start_offset = vidx.range(RPA_OFFSET_WIDTH - 1, 0);
		rpa_offset_dt end_offset = vidx_plus1.range(RPA_OFFSET_WIDTH - 1, 0);
		rpa_cache_dt word_old = cache_data;
		rpa_tag_dt _tag = tag;

		if(start_tag == _tag && end_tag == _tag){
#ifdef RPAO_ANALYSIS
			hit_num++;
			hit_num++;
#endif
			start_stream << word_old.range((start_offset + 1)*32 - 1, start_offset * 32);
			end_stream << word_old.range((end_offset + 1)*32 - 1, end_offset * 32);
		}
		else if(start_tag == _tag && start_tag != end_tag){
#ifdef RPAO_ANALYSIS
			hit_num++;
#endif

			rpa_cache_dt word = rpaoA[end_tag];
			start_stream << word_old.range((start_offset + 1)*32 - 1, start_offset * 32);
			end_stream << word.range((end_offset + 1)*32 - 1, end_offset * 32);
			cache_data = word;
			tag = end_tag;
		}
		else if(start_tag != _tag && start_tag == end_tag){
			rpa_cache_dt word = rpaoA[start_tag];
			start_stream << word.range((start_offset + 1) * 32 - 1, start_offset * 32);
			end_stream << word.range((end_offset + 1) * 32 - 1, end_offset * 32);
			cache_data = word;
			tag = start_tag;
		}
		else{
			rpa_cache_dt word_start = rpaoA[start_tag];
			rpa_cache_dt word_end = rpaoB[end_tag];
			start_stream << word_start.range((start_offset + 1)*32 - 1, start_offset * 32);
			end_stream << word_end.range((end_offset + 1)*32 - 1, end_offset * 32);
			cache_data = word_end;
			tag = end_tag;
		}

#ifdef RPAO_ANALYSIS
		read_num++;
		read_num++;
#endif
	}

#ifdef RPAO_ANALYSIS
	read_sum += read_num;
	hit_sum += hit_num;
	if(read_num > 0 && read_sum > 0){
		printf("rpao hit rate: %f\n", hit_num * 1.0/read_num);
		printf("total rpao hit rate: %f\n", hit_sum * 1.0/read_sum);
	}
#endif
}

static void read_csr_cai(
		const int frontier_size,
		const cia_cache_dt *ciao,
		hls::stream<uint32_dt> &start_stream,
		hls::stream<uint32_dt> &end_stream,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream
		)
{
	static cia_cache_dt cache_data;
	static cia_tag_dt cache_tag = CIA_TAG_INIT_VAL;

#ifdef CIAO_ANALYSIS
	int hit_cnt = 0;
	int ciao_cnt = 0;
	static int hit_sum = 0;
	static int ciao_sum = 0;
#endif

    read_csr_cai_L1:
	for(int j = 0; j < frontier_size; j++){
		uint32_dt start = start_stream.read();
		uint32_dt end = end_stream.read();

        read_csr_cai_L0:
		for(uint32_dt i = start; i < end; i++){
		#pragma HLS LOOP_FLATTEN off
        #pragma HLS pipeline
#ifdef CIAO_ANALYSIS
			ciao_cnt++;
#endif
			cia_tag_dt addr = i.range(31, CIA_OFFSET_WIDTH);
			cia_offset_dt offset = i.range(CIA_OFFSET_WIDTH - 1 ,0);
			cia_tag_dt tag = i.range(31, CIA_OFFSET_WIDTH);
			cia_tag_dt _tag = cache_tag;
			cia_cache_dt word_old = cache_data;
			if(_tag == tag){
				ciao_stream << word_old.range((offset+1)*32 - 1, offset * 32);
#ifdef CIAO_ANALYSIS
				hit_cnt++;
#endif
			}
			else{
				cia_cache_dt word = ciao[addr];
				ciao_stream << word.range((offset+1)*32 - 1, offset * 32);
				cache_data = word;
				cache_tag = tag;
			}
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
static void squeeze_neighbor(
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int> &ciao_squeeze_stream,
		hls::stream<uint1_dt> &squeeze_done_stream,
		const char level
		)
{
	static uint1_dt bitmap[BITMAP_MEM_DEPTH];
#pragma HLS RESOURCE variable=bitmap core=RAM_S2P_BRAM latency=1
#pragma HLS DEPENDENCE variable=bitmap inter false
#pragma HLS DEPENDENCE variable=bitmap intra raw true

	uint32_dt root_vidx = 320872;
   	if(level == 0){
        squeeze_init:
		for (int i = 0; i < BITMAP_MEM_DEPTH; i++){
        #pragma HLS pipeline II=1
			bitmap[i] = 0;
		}

		bitmap[root_vidx] = 1;
	}

    squeeze_main:
	while(ciao_empty != 1 || done != 1){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();

		if(ciao_empty != 1){
			uint32_dt vidx = ciao_stream.read();	
			if(bitmap[vidx] == 0){
				ciao_squeeze_stream << vidx;
				bitmap[vidx] = 1;
			}
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();
		}
	}

	squeeze_done_stream << 1;
}

// load depth for inspection 
static void update_depth(
		int512_dt *depth,
		int *next_frontier,
		int *next_frontier_size,
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		const char level_plus1,
		const char level)
{
	int done = 0;
	int ngb_empty = 0;
	int done_empty = 0;
	int depth_empty = 0;
	int idx = 0;
	int counter = 0;
	//uint16_dt delay_counter = 0;

	static int512_dt cache_data[DEPTH_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_data core=RAM_2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_data inter false

	static depth_tag_dt cache_tag[DEPTH_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_tag core=RAM_2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_tag inter false

	static uint1_dt cache_dirty[DEPTH_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_dirty core=RAM_2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_dirty inter false

	if(level == 0){
        stage5_init:
		for(uint32_dt i = 0; i < DEPTH_CACHE_SIZE; i++){
        #pragma HLS PIPELINE II=1
			cache_tag[i] = DEPTH_TAG_INIT_VAL;
			cache_dirty[i] = 0;
		}
	}

    stage5_main:
    while(ngb_empty != 1 || done != 1){
    #pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		ngb_empty = ngb_stream.empty();
		done_empty = ngb_done_stream.empty();

		if(ngb_empty != 1){
			counter++;
			uint32_dt vidx = ngb_stream.read();
			next_frontier[idx++] = vidx;
			depth_offset_dt offset = vidx.range(DEPTH_OFFSET_WIDTH - 1, 0);
			depth_index_dt index = vidx.range(DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH - 1, DEPTH_OFFSET_WIDTH);
			depth_tag_dt tag = vidx.range(31, DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH);
			depth_tag_dt _tag = cache_tag[index];
			depth_addr_dt _addr = (_tag, index);
			depth_addr_dt addr = (tag, index);
			uint1_dt dirty = cache_dirty[index];
			int512_dt word_old = cache_data[index];
			cache_dirty[index] = 1; // Make sure this happens after dirty is read.

			// cache hit
			if(_tag == tag){
				cache_data[index].range((offset + 1) * 8 - 1, offset * 8) = level_plus1;
			}
			else{
				int512_dt word_new = depth[addr];
				word_new.range((offset + 1) * 8 - 1, offset * 8) = level_plus1;
				if(dirty == 1){
					depth[_addr] = word_old;

					// Short delay is added here to ensure the data is written back to DDR.
					//for(char i = 0; i < 256; i++){
					//	delay_counter++;
					//	if(delay_counter == 256) delay_counter = 0;
					//}
				}
				cache_data[index] = word_new;
				cache_tag[index] = tag;
			}

		}

		if(done_empty != 1 && ngb_empty == 1){
			done = ngb_done_stream.read();
		}
    }

	next_frontier_size = counter;

    stage5_wb:
	for(uint32_dt i = 0; i < DEPTH_CACHE_SIZE; i++){
    #pragma HLS PIPELINE
		if(cache_dirty[i] == 1){
			depth_tag_dt tag = cache_tag[i];
			depth_addr_dt addr = (tag, i.range(DEPTH_INDEX_WIDTH - 1, 0));
			depth[addr] = cache_data[i];
			cache_dirty[i] = 0;
		}
	}

}



extern "C" {
void bfs(
		const int *frontier,
		int *next_frontier,
		int512_dt *depth_for_update,
		const rpa_cache_dt *rpaoA, 
		const rpa_cache_dt *rpaoB, 
		const int512_dt *ciao,
		int *next_frontier_size,
		const int frontier_size,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=frontier offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=next_frontier offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=rpaoA offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=rpaoB offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=ciao offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size offset=slave bundle=gmem5
#pragma HLS INTERFACE s_axilite port=frontier_size bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

char level_plus1 = level + 1;

hls::stream<int> frontier_stream;
hls::stream<uint32_dt> start_stream;
hls::stream<uint32_dt> end_stream;
hls::stream<int> ciao_stream;
hls::stream<uint1_dt> ciao_done_stream;
hls::stream<int> ciao_squeeze_stream;
hls::stream<uint1_dt> squeeze_done_stream;

#pragma HLS STREAM variable=frontier_stream depth=64
#pragma HLS STREAM variable=start_stream depth=64
#pragma HLS STREAM variable=end_stream depth=64
#pragma HLS STREAM variable=ciao_stream depth=64
#pragma HLS STREAM variable=ciao_done_stream depth=4
#pragma HLS STREAM variable=ciao_squeeze_stream depth=64
#pragma HLS STREAM variable=squeeze_done_stream depth=4

#pragma HLS dataflow
read_frontier(frontier_size, frontier, frontier_stream);
read_csr_rpa(frontier_size, rpaoA, rpaoB, frontier_stream, start_stream, end_stream);
read_csr_cai(frontier_size, ciao, frontier_stream, start_stream, end_stream, 
		ciao_stream, end_stream, ciao_done_stream);
squeeze_neighbor(ciao_stream, ciao_done_stream, ciao_squeeze_stream, squeeze_done_stream, level);
update_depth(depth_for_update, next_frontier, next_frontier_size, ciao_squeeze_stream, 
		squeeze_done_stream, level_plus1, level);
}
}


