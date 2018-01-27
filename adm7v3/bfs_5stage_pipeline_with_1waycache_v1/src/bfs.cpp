#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

//#define SW_EMU
//#define RPAO_ANALYSIS
//#define CIAO_ANALYSIS
//#define DEPTH_ANALYSIS
//#define HASH_ANALYSIS

#define HASH_SIZE (16384)
#define HASH_ID_WIDTH 14
#define HASH_TAG_WIDTH 18
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
typedef ap_int<1024> int1024_dt;

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
		const rpa_cache_dt *rpao,
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
	rpa_tag_dt tag[1] = {RPA_TAG_INIT_VAL};
	rpa_cache_dt cache_data[1];
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
			rpa_cache_dt word_old = cache_data[0];
			rpa_tag_dt _tag = tag[0];

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

				rpa_cache_dt word = rpao[end_tag];
				start_stream << word_old.range((start_offset + 1)*32 - 1, start_offset * 32);
				end_stream << word.range((end_offset + 1)*32 - 1, end_offset * 32);
				cache_data[0] = word;
				tag[0] = end_tag;
			}
			else if(start_tag != _tag && start_tag == end_tag){
				rpa_cache_dt word = rpao[start_tag];
				start_stream << word.range((start_offset + 1) * 32 - 1, start_offset * 32);
				end_stream << word.range((end_offset + 1) * 32 - 1, end_offset * 32);
				cache_data[0] = word;
				tag[0] = start_tag;
			}
			else{
				rpa_cache_dt word_start = rpao[start_tag];
				rpa_cache_dt word_end = rpao[end_tag];
				start_stream << word_start.range((start_offset + 1)*32 - 1, start_offset * 32);
				end_stream << word_end.range((end_offset + 1)*32 - 1, end_offset * 32);
				cache_data[0] = word_end;
				tag[0] = end_tag;
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

	// 4 bit offset, 14 bit index, 14 bit tag
	cia_cache_dt cache_data[1];
	cia_tag_dt cache_tag[1] = {0xFFFFFFF};

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

		if(done_empty != 1 && start_empty == 1 && end_empty == 1){
			done = rpao_done_stream.read();
		}
	}

	ciao_done_stream << 1;
#ifdef CIAO_ANALYSIS
	hit_sum += hit_cnt;
	ciao_sum += ciao_cnt;
	if(ciao_cnt > 0 && ciao_sum > 0){
		printf("Level = %d, hit_cnt = %d, ciao_sum =%d, ciao cache hit rate = %f\n", (int)level, hit_cnt, ciao_cnt, hit_cnt*1.0/ciao_cnt);
		printf("Total ciao cache hit rate = %f\n", hit_sum * 1.0/ciao_sum);
	}
#endif

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

#ifdef HASH_ANALYSIS
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
#ifdef HASH_ANALYSIS
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
#ifdef HASH_ANALYSIS
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
		const int512_dt *depth_for_expand,
		int512_dt *depth_for_update,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		const char level_plus1)
{
	// Cache data structure 6bit offset, 14bit index, 12bit tag
	uint1_dt cache_dirty[DEPTH_CACHE_SIZE];
	int512_dt cache_data[DEPTH_CACHE_SIZE];
	depth_tag_dt cache_tag[DEPTH_CACHE_SIZE];

#ifdef DEPTH_ANALYSIS
	int read_num = 0;
	int write_num = 0;
	int read_hit_num = 0;
	int write_hit_num = 0;
	static int read_sum = 0;
	static int write_sum = 0;
	static int hit_sum = 0;
#endif

#ifndef SW_EMU
	if(level_plus1 == 1){
#endif
		for(int i = 0; i < DEPTH_CACHE_SIZE; i++){
        #pragma HLS pipeline II=1
			cache_tag[i] = DEPTH_TAG_INIT_VAL;
			cache_dirty[i] = 0;
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
			depth_offset_dt offset = vidx.range(DEPTH_OFFSET_WIDTH - 1, 0);
			depth_index_dt index = vidx.range(DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH - 1, DEPTH_OFFSET_WIDTH);
			depth_tag_dt tag = vidx.range(31, DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH);
			depth_addr_dt addr = vidx.range(31, DEPTH_OFFSET_WIDTH);
			depth_tag_dt _tag = cache_tag[index];
			int512_dt _word = cache_data[index];
			depth_addr_dt _addr = (_tag, index);

#ifdef DEPTH_ANALYSIS
			read_num++;
#endif

			// cache hit
			if(_tag == tag){
#ifdef DEPTH_ANALYSIS
				read_hit_num++;
#endif
				char d = _word.range(((offset+1)*8) - 1, (offset*8));
				if(d == -1){
#ifdef DEPTH_ANALYSIS
					write_hit_num++;
					write_num++;
#endif
					cache_data[index].range(((offset+1)*8) - 1, (offset*8)) = level_plus1;
					cache_dirty[index] = 1;
				}
			}

			// cache miss
			else{
				int512_dt word = depth_for_expand[addr];
				uint1_dt dirty = cache_dirty[index];
				if(dirty){	
					depth_for_update[_addr] = _word;
				}
				char d = word.range(((offset+1)*8) - 1, (offset*8));
				if(d == -1) word.range(((offset+1)*8) - 1, (offset*8)) = level_plus1;
#ifdef DEPTH_ANALYSIS
				if(d == -1) write_num++;
#endif
				cache_dirty[index] = (d == -1) ? 1 : 0;
				cache_tag[index] = tag;
				cache_data[index] = word;
			}
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();		
		}
	}

	for(uint32_dt i = 0; i < DEPTH_CACHE_SIZE; i++){
    #pragma HLS pipeline
	   uint1_dt dirty = cache_dirty[i];
		if(dirty){
			depth_addr_dt line_addr = (cache_tag[i], i.range(DEPTH_INDEX_WIDTH - 1, 0));
			depth_for_update[line_addr] = cache_data[i];
			cache_dirty[i] = 0;
		}
	}

#ifdef DEPTH_ANALYSIS
	read_sum += read_num;
	write_sum += write_num;
	hit_sum += read_hit_num + write_hit_num;
	if(read_num > 0 && write_num > 0){
		printf("Depth read hit rate: %f\n", read_hit_num * 1.0/read_num);
		printf("Depth write hit rate: %f\n", write_hit_num * 1.0/write_num);
		printf("Total depth cache hit rate: %f\n", hit_sum * 1.0/(read_sum + write_sum));
	}
#endif
}

extern "C" {
void bfs(
		const int512_dt *depth_for_inspect,
		const int512_dt *depth_for_expand,
		int512_dt *depth_for_update,
		const rpa_cache_dt *rpao, 
		const cia_cache_dt *ciao,
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



