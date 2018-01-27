/**
 * File              : src/bfs.cpp
 * Author            : Cheng Liu <st.liucheng@gmail.com>
 * Date              : 13.11.2017
 * Last Modified Date: 04.12.2017
 * Last Modified By  : Cheng Liu <st.liucheng@gmail.com>
 */

#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define BUFFER_SIZE 16

// 16 x 32K x 16bit bitmap memory
#define BITMAP_MEM_DEPTH (32*1024) 

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

#define DEPTH_CACHE_SIZE 8192
#define DEPTH_OFFSET_WIDTH 6
#define DEPTH_INDEX_WIDTH 13
#define DEPTH_TAG_WIDTH 13
#define DEPTH_TAG_INIT_VAL 0x1FFF
#define DEPTH_ADDR_WIDTH (DEPTH_INDEX_WIDTH + DEPTH_TAG_WIDTH)
typedef ap_uint<DEPTH_OFFSET_WIDTH> depth_offset_dt;
typedef ap_uint<DEPTH_INDEX_WIDTH> depth_index_dt;
typedef ap_uint<DEPTH_TAG_WIDTH> depth_tag_dt;
typedef ap_uint<DEPTH_ADDR_WIDTH> depth_addr_dt;


typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_uint<4> uint4_dt;
typedef ap_uint<6> uint6_dt;
typedef ap_uint<15> uint15_dt;
typedef ap_uint<16> uint16_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_int<512> int512_dt;

uint16_dt get_mask(
		uint4_dt n
		){
#pragma HLS INLINE 
	uint16_dt mask;
	switch (n){
		case 0:	
			mask = 0x1;
			break;
		case 1:
			mask = 0x2;
			break;
		case 2:
			mask = 0x4;
			break;
		case 3:
			mask = 0x8;
			break;
		case 4:
			mask = 0x10;
			break;
		case 5:
			mask = 0x20;
			break;
		case 6:
			mask = 0x40;
			break;
		case 7:
			mask = 0x80;
			break;
		case 8:
			mask = 0x100;
			break;
		case 9:
			mask = 0x200;
			break;
		case 10:
			mask = 0x400;
			break;
		case 11:
			mask = 0x800;
			break;
		case 12:
			mask = 0x1000;
			break;
		case 13:
			mask = 0x2000;
			break;
		case 14:
			mask = 0x4000;
			break;
		case 15:
			mask = 0x8000;
			break;
	}

	return mask;
}

// load depth for inspection 
static void bfs_stage0(
		const int512_dt *depth, 
		hls::stream<int512_dt> &depth_inspect_stream,
        int word_num)
{
	int512_dt buffer[BUFFER_SIZE];
	//int len = BUFFER_SIZE;

    stage0_main:
    for (uint32_dt i = 0; i < word_num; i += BUFFER_SIZE){
		//if(i + BUFFER_SIZE > word_num) len = word_num - i;
		for(int j = 0; j < BUFFER_SIZE; j++){
        #pragma HLS pipeline II=1
			buffer[j] = depth[i + j];
		}

		for(int j = 0; j < BUFFER_SIZE; j++){
        #pragma HLS pipeline II=1
			depth_inspect_stream << buffer[j];
		}
	}
}

static void bfs_stage0_1(
		hls::stream<int512_dt> &depth_inspect_stream,
		hls::stream<char> &depth_inspect_char_stream,
		const int vertex_num
		)
{
	int512_dt word;
	for(int i = 0; i < vertex_num; i++){
		int idx = i%64;
		if(idx == 0) word = depth_inspect_stream.read();
		depth_inspect_char_stream << word.range((idx + 1)*8 - 1, idx * 8);
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
	int counter = 0;
    stage1_main:
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

// Read rpao of the frontier 
static void bfs_stage2(
		const rpa_cache_dt *rpaoA,
		const rpa_cache_dt *rpaoB,
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<uint32_dt> &start_stream,
		hls::stream<uint32_dt> &end_stream,
		hls::stream<uint1_dt> &rpao_done_stream)
{
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;

	// 4 bit offset, 28bit tag
	rpa_tag_dt tag = RPA_TAG_INIT_VAL;
	static rpa_cache_dt cache_data;
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
		const cia_cache_dt *ciao,
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

	static cia_cache_dt cache_data;
	static cia_tag_dt cache_tag = CIA_TAG_INIT_VAL;

#ifdef CIAO_ANALYSIS
	int hit_cnt = 0;
	int ciao_cnt = 0;
	static int hit_sum = 0;
	static int ciao_sum = 0;
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

		if(done_empty != 1 && start_empty == 1 && end_empty == 1){
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
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int> &ciao_squeeze_stream0,
		hls::stream<int> &ciao_squeeze_stream1,
		hls::stream<int> &ciao_squeeze_stream2,
		hls::stream<int> &ciao_squeeze_stream3,
		hls::stream<uint1_dt> &ciao_squeeze_done_stream0,
		hls::stream<uint1_dt> &ciao_squeeze_done_stream1,
		hls::stream<uint1_dt> &ciao_squeeze_done_stream2,
		hls::stream<uint1_dt> &ciao_squeeze_done_stream3,
		const uint32_dt root_vidx,
		const char level
		)
{
	int done = 0;
	int ciao_empty = 0;
	int done_empty = 0;

	static uint16_dt bitmap[16][BITMAP_MEM_DEPTH];
#pragma HLS array_partition variable=bitmap cyclic factor=16 dim=1
//#pragma HLS RESOURCE variable=bitmap core=RAM_S2P_BRAM latency=1
//#pragma HLS DEPENDENCE variable=bitmap inter false
//#pragma HLS DEPENDENCE variable=bitmap intra raw true

   	if(level == 0){
        squeeze_init:
		for(int j = 0; j < 16; j++){
			for (int i = 0; i < BITMAP_MEM_DEPTH; i++){
            #pragma HLS pipeline II=1
				bitmap[j][i] = 0;
			}
		}

		uint4_dt n = root_vidx.range(3, 0);
		uint15_dt offset = root_vidx.range(18, 4);
		uint4_dt dim = root_vidx.range(22, 19);
		uint16_dt mask = get_mask(n);
		bitmap[dim][offset] = mask;
	}

squeeze_main:
	while(ciao_empty != 1 || done != 1){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();

		if(ciao_empty != 1){
			uint32_dt vidx = ciao_stream.read();	
			uint4_dt n = vidx.range(3, 0);
			uint15_dt offset = vidx.range(18, 4);
			uint4_dt dim = vidx.range(22, 19);
			uint16_dt mask = get_mask(n);

			if((bitmap[dim][offset] & mask) == 0){
				uint2_dt channel = vidx.range(14, 13);
				if(channel == 0){
					ciao_squeeze_stream0 << vidx;
				}
				else if(channel == 1){
					ciao_squeeze_stream1 << vidx;
				}
				else if(channel == 2){
					ciao_squeeze_stream2 << vidx;
				}
				else{
					ciao_squeeze_stream3 << vidx;
				}
				bitmap[dim][offset] |= mask;
			}
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();
		}
	}

	ciao_squeeze_done_stream0 << 1;
	ciao_squeeze_done_stream1 << 1;
	ciao_squeeze_done_stream2 << 1;
	ciao_squeeze_done_stream3 << 1;
}

// load depth for inspection 
static void bfs_stage5(
		int512_dt *depth_for_read,
		int512_dt *depth_for_write,
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		const char level_plus1,
		const char level)
{
	int done = 0;
	int ngb_empty = 0;
	int done_empty = 0;
	int depth_empty = 0;
	//uint16_dt delay_counter = 0;

	int512_dt cache_data[DEPTH_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_data core=RAM_2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_data inter false

	depth_tag_dt cache_tag[DEPTH_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_tag core=RAM_2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_tag inter false

	uint1_dt cache_dirty[DEPTH_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_dirty core=RAM_2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_dirty inter false

	//int512_dt data_buffer[16];
	//depth_tag_dt tag_buffer[16];
    //#pragma HLS array_partition variable=tag_buffer complete

	int delay_counter = 0;

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
		ngb_empty = ngb_stream.empty();
		done_empty = ngb_done_stream.empty();

		if(ngb_empty != 1){
			uint32_dt vidx = ngb_stream.read();
			depth_offset_dt offset = vidx.range(DEPTH_OFFSET_WIDTH - 1, 0);
			depth_index_dt index = vidx.range(DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH - 1, DEPTH_OFFSET_WIDTH);
			depth_tag_dt tag = vidx.range(31, DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH);
			depth_tag_dt _tag = cache_tag[index];
			depth_addr_dt _addr = (_tag, index);
			depth_addr_dt addr = (tag, index);
			uint1_dt dirty = cache_dirty[index];
			int512_dt word_old = cache_data[index];
			cache_dirty[index] = 1; // Make sure this happens after dirty is read.

			//uint1_dt recent_write = 0;
			//for(int i = 0; i < 16; i++){
			//	if(tag_buffer[i] == tag){
			//		recent_write = 1;
			//		break;
			//	}
			//}

			// cache hit
			if(_tag == tag){
				cache_data[index].range((offset + 1) * 8 - 1, offset * 8) = level_plus1;
			}
			else{	
				if(dirty == 1){
					depth_for_write[_addr] = word_old;
				}
				int512_dt word_new = depth_for_read[addr];
				word_new.range((offset + 1) * 8 - 1, offset * 8) = level_plus1;
				cache_data[index] = word_new;
				cache_tag[index] = tag;
			}
		}

		if(done_empty != 1 && ngb_empty == 1){
			done = ngb_done_stream.read();
		}
    }

    stage5_wb:
	for(uint32_dt i = 0; i < DEPTH_CACHE_SIZE; i++){
    #pragma HLS PIPELINE
		if(cache_dirty[i] == 1){
			depth_tag_dt tag = cache_tag[i];
			depth_addr_dt addr = (tag, i.range(DEPTH_INDEX_WIDTH - 1, 0));
			depth_for_write[addr] = cache_data[i];
			cache_dirty[i] = 0;
		}
	}

}



extern "C" {
void bfs(
		const int512_dt *depth_for_inspect,
		int512_dt *depth_for_read0,
		int512_dt *depth_for_read1,
		int512_dt *depth_for_read2,
		int512_dt *depth_for_read3,
		int512_dt *depth_for_write0,
		int512_dt *depth_for_write1,
		int512_dt *depth_for_write2,
		int512_dt *depth_for_write3,
		const rpa_cache_dt *rpaoA, 
		const rpa_cache_dt *rpaoB, 
		const int512_dt *ciao,
		int *frontier_size,
		const int vertex_num,
		const int root_vidx,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_for_read0 offset=slave bundle=gmem10
#pragma HLS INTERFACE m_axi port=depth_for_read1 offset=slave bundle=gmem11
#pragma HLS INTERFACE m_axi port=depth_for_read2 offset=slave bundle=gmem12
#pragma HLS INTERFACE m_axi port=depth_for_read3 offset=slave bundle=gmem13
#pragma HLS INTERFACE m_axi port=depth_for_write0 offset=slave bundle=gmem20
#pragma HLS INTERFACE m_axi port=depth_for_write1 offset=slave bundle=gmem21
#pragma HLS INTERFACE m_axi port=depth_for_write2 offset=slave bundle=gmem22
#pragma HLS INTERFACE m_axi port=depth_for_write3 offset=slave bundle=gmem23
#pragma HLS INTERFACE m_axi port=rpaoA offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=rpaoB offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=ciao offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem5
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=root_vidx bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

char level_plus1 = level + 1;
int word_num = vertex_num >> 6;

hls::stream<int512_dt> depth_inspect_stream;
hls::stream<char> depth_inspect_char_stream;
hls::stream<uint1_dt> inspect_done_stream;
hls::stream<int> frontier_stream;
hls::stream<uint32_dt> start_stream;
hls::stream<uint32_dt> end_stream;
hls::stream<uint1_dt> rpao_done_stream;
hls::stream<int> ciao_stream;
hls::stream<uint1_dt> ciao_done_stream;
hls::stream<int> ciao_squeeze_stream0;
hls::stream<int> ciao_squeeze_stream1;
hls::stream<int> ciao_squeeze_stream2;
hls::stream<int> ciao_squeeze_stream3;
hls::stream<uint1_dt> squeeze_done_stream0;
hls::stream<uint1_dt> squeeze_done_stream1;
hls::stream<uint1_dt> squeeze_done_stream2;
hls::stream<uint1_dt> squeeze_done_stream3;

#pragma HLS STREAM variable=depth_inspect_stream depth=32
#pragma HLS STREAM variable=depth_inspect_char_stream depth=32
#pragma HLS STREAM variable=frontier_stream depth=32
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=rpao_done_stream depth=4
#pragma HLS STREAM variable=start_stream depth=32
#pragma HLS STREAM variable=end_stream depth=32
#pragma HLS STREAM variable=ciao_stream depth=32
#pragma HLS STREAM variable=ciao_done_stream depth=4
#pragma HLS STREAM variable=ciao_squeeze_stream0 depth=32
#pragma HLS STREAM variable=ciao_squeeze_stream1 depth=32
#pragma HLS STREAM variable=ciao_squeeze_stream2 depth=32
#pragma HLS STREAM variable=ciao_squeeze_stream3 depth=32
#pragma HLS STREAM variable=squeeze_done_stream0 depth=4
#pragma HLS STREAM variable=squeeze_done_stream1 depth=4
#pragma HLS STREAM variable=squeeze_done_stream2 depth=4
#pragma HLS STREAM variable=squeeze_done_stream3 depth=4

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, word_num);
bfs_stage0_1(depth_inspect_stream, depth_inspect_char_stream, vertex_num);
bfs_stage1(frontier_size, depth_inspect_char_stream, inspect_done_stream, frontier_stream, vertex_num, level);
bfs_stage2(rpaoA, rpaoB, frontier_stream, inspect_done_stream, start_stream, end_stream, rpao_done_stream);
bfs_stage3(ciao, start_stream, end_stream, rpao_done_stream, ciao_stream, ciao_done_stream, level);
bfs_squeeze_ciao(ciao_stream, ciao_done_stream, ciao_squeeze_stream0, ciao_squeeze_stream1, 
		ciao_squeeze_stream2, ciao_squeeze_stream3, squeeze_done_stream0, squeeze_done_stream1, 
		squeeze_done_stream2, squeeze_done_stream3, root_vidx, level);
bfs_stage5(depth_for_read0, depth_for_write0, ciao_squeeze_stream0, squeeze_done_stream0, level_plus1, level);
bfs_stage5(depth_for_read1, depth_for_write1, ciao_squeeze_stream1, squeeze_done_stream1, level_plus1, level);
bfs_stage5(depth_for_read2, depth_for_write2, ciao_squeeze_stream2, squeeze_done_stream2, level_plus1, level);
bfs_stage5(depth_for_read3, depth_for_write3, ciao_squeeze_stream3, squeeze_done_stream3, level_plus1, level);
}
}



