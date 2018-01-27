#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define HASH_SIZE (16384)
#define HASH_ID_WIDTH 14
#define HASH_TAG_WIDTH 18
#define HASH_TAG_INIT_VAL 0x3FFFF
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
typedef ap_uint<22> uint22_dt;
typedef ap_uint<26> uint26_dt;
typedef ap_uint<28> uint28_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<512> int512_dt;


// load depth for inspection 
static void bfs_stage0(
		const int512_dt *depth, 
		hls::stream<int512_dt> &depth_inspect_stream,
        int word_num)
{
	int512_dt buffer[16];
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

				rpa_cache_dt word = rpaoA[end_tag];
				start_stream << word_old.range((start_offset + 1)*32 - 1, start_offset * 32);
				end_stream << word.range((end_offset + 1)*32 - 1, end_offset * 32);
				cache_data[0] = word;
				tag[0] = end_tag;
			}
			else if(start_tag != _tag && start_tag == end_tag){
				rpa_cache_dt word = rpaoA[start_tag];
				start_stream << word.range((start_offset + 1) * 32 - 1, start_offset * 32);
				end_stream << word.range((end_offset + 1) * 32 - 1, end_offset * 32);
				cache_data[0] = word;
				tag[0] = start_tag;
			}
			else{
				rpa_cache_dt word_start = rpaoA[start_tag];
				rpa_cache_dt word_end = rpaoB[end_tag];
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

	static hash_tag_dt hash_table0[HASH_SIZE];
#pragma HLS RESOURCE variable=hash_table0 core=RAM_2P_BRAM
#pragma HLS DEPENDENCE variable=hash_table0 inter false

	static hash_tag_dt hash_table1[HASH_SIZE];
#pragma HLS RESOURCE variable=hash_table1 core=RAM_2P_BRAM
#pragma HLS DEPENDENCE variable=hash_table1 inter false

	static hash_tag_dt hash_table2[HASH_SIZE];
#pragma HLS RESOURCE variable=hash_table2 core=RAM_2P_BRAM
#pragma HLS DEPENDENCE variable=hash_table2 inter false

	static hash_tag_dt hash_table3[HASH_SIZE];
#pragma HLS RESOURCE variable=hash_table3 core=RAM_2P_BRAM
#pragma HLS DEPENDENCE variable=hash_table3 inter false

	uint2_dt hash_pri = 0;
   	if(level == 0){
        squeeze_init:
		for (int i = 0; i < HASH_SIZE; i++){
        #pragma HLS pipeline II=1
			hash_table0[i] = HASH_TAG_INIT_VAL;
			hash_table1[i] = HASH_TAG_INIT_VAL;
			hash_table2[i] = HASH_TAG_INIT_VAL;
			hash_table3[i] = HASH_TAG_INIT_VAL;
		}
	}

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


static void bfs_stage4(
		const int512_dt *depth,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		const char level
		){
	uint1_dt done = 0;
	uint1_dt ciao_empty = 0;
	uint1_dt done_empty = 0;

	int512_dt cache_data[DEPTH_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_data core=RAM_T2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_data inter false

	depth_tag_dt cache_tag[DEPTH_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_tag core=RAM_T2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_tag inter false


    stage4_init:
	for(int i = 0; i < DEPTH_CACHE_SIZE; i++){
    #pragma HLS PIPELINE II=1
		cache_tag[i] = DEPTH_TAG_INIT_VAL;
	}

    stage4_main:
	while((ciao_empty != 1) || (done != 1)){
    #pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			uint32_dt vidx = ciao_stream.read();
			depth_offset_dt offset = vidx.range(DEPTH_OFFSET_WIDTH - 1, 0);
			depth_index_dt index = vidx.range(DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH - 1, DEPTH_OFFSET_WIDTH);
			depth_tag_dt tag = vidx.range(31, DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH);
			uint32_dt addr  = vidx.range(31, DEPTH_OFFSET_WIDTH);
			depth_tag_dt _tag = cache_tag[index];

			if(_tag == tag){
				int512_dt word_old = cache_data[index];
				char d = word_old.range((offset + 1) * 8 - 1, offset * 8); 
				if(d == -1) ngb_stream << vidx;
			}
			
			else if(_tag != tag){ // cache miss
				int512_dt word_new = depth[addr];
				char d = word_new.range((offset + 1) * 8 - 1, offset * 8); 
				if(d == -1) ngb_stream << vidx;
				cache_data[index] = word_new;
				cache_tag[index] = tag;
			}
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();
		}
	}

	ngb_done_stream << 1;
}

// load depth for inspection 
static void bfs_stage5(
		int512_dt *depth,
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream,
		const char level_plus1,
		const char level)
{
	int done = 0;
	int ngb_empty = 0;
	int done_empty = 0;
	int depth_empty = 0;

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
			uint32_dt vidx = ngb_stream.read();
			depth_offset_dt offset = vidx.range(DEPTH_OFFSET_WIDTH - 1, 0);
			depth_index_dt index = vidx.range(DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH - 1, DEPTH_OFFSET_WIDTH);
			depth_tag_dt tag = vidx.range(31, DEPTH_OFFSET_WIDTH + DEPTH_INDEX_WIDTH);
			uint12_dt _tag = cache_tag[index];
			uint26_dt _addr = (_tag, index);
			uint26_dt addr = (tag, index);
			uint1_dt dirty = cache_dirty[index];
			int512_dt word_old = cache_data[index];
			cache_dirty[index] = 1; // Make sure this happens after dirty is read.

			// cache miss
			if(_tag == tag){
				cache_data[index].range((offset + 1) * 8 - 1, offset * 8) = level_plus1;
			}
			else{
				int512_dt word_new = depth[addr];
				word_new.range((offset + 1) * 8 - 1, offset * 8) = level_plus1;
				if(dirty == 1){
					depth[_addr] = word_old;
				}
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
			depth[addr] = cache_data[i];
			cache_dirty[i] = 0;
		}
	}

}



extern "C" {
void bfs(
		const int512_dt *depth_for_inspect,
		const int512_dt *depth_for_expand,
		int512_dt *depth_for_update,
		const rpa_cache_dt *rpaoA, 
		const rpa_cache_dt *rpaoB, 
		const int512_dt *ciao,
		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_for_expand offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=rpaoA offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=rpaoB offset=slave bundle=gmem32
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
hls::stream<int> ngb_stream;
hls::stream<uint1_dt> ngb_done_stream;
hls::stream<int> ciao_squeeze_stream;
hls::stream<uint1_dt> ciao_squeeze_done_stream;

#pragma HLS STREAM variable=depth_inspect_stream depth=64
#pragma HLS STREAM variable=frontier_stream depth=64
#pragma HLS STREAM variable=inspect_done_stream depth=4
#pragma HLS STREAM variable=rpao_done_stream depth=4
#pragma HLS STREAM variable=start_stream depth=64
#pragma HLS STREAM variable=end_stream depth=64
#pragma HLS STREAM variable=ciao_stream depth=64
#pragma HLS STREAM variable=ciao_done_stream depth=4
#pragma HLS STREAM variable=ngb_stream depth=64
#pragma HLS STREAM variable=ngb_done_stream depth=4
#pragma HLS STREAM variable=ciao_squeeze_stream depth=64
#pragma HLS STREAM variable=ciao_squeeze_done_stream depth=4

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, word_num);
bfs_stage1(frontier_size, depth_inspect_stream, inspect_done_stream, frontier_stream, word_num, level);
bfs_stage2(rpaoA, rpaoB, frontier_stream, inspect_done_stream, start_stream, end_stream, rpao_done_stream);
bfs_stage3(ciao, start_stream, end_stream, rpao_done_stream, ciao_stream, ciao_done_stream, level);
bfs_squeeze_ciao(ciao_stream, ciao_done_stream, 
		ciao_squeeze_stream, ciao_squeeze_done_stream, level);
bfs_stage4(depth_for_expand, ciao_squeeze_stream, ciao_squeeze_done_stream, 
		ngb_stream, ngb_done_stream, level);
bfs_stage5(depth_for_update, ngb_stream, ngb_done_stream, level_plus1, level);
}
}



