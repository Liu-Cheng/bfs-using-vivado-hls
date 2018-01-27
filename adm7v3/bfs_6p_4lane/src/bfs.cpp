#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

//===================================================
// sw_emu error is expected. The hash table can't be 
// declared as static the squeeze function has 
// multiple instances. Each instance will own an 
// independent hash table which is different from the 
// definition in sw. One possible way for sw_emu is 
// thus to clean the hash table in each bfs iteration.
//===================================================

//#define SW_EMU

#define HASH_SIZE (16384 * 4)
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

#define DEPTH_RD_CACHE_SIZE 4096
#define DEPTH_RD_OFFSET_WIDTH 6
#define DEPTH_RD_INDEX_WIDTH 12
#define DEPTH_RD_TAG_WIDTH 14
#define DEPTH_RD_TAG_INIT_VAL 0x3FFF
#define DEPTH_RD_ADDR_WIDTH (DEPTH_RD_INDEX_WIDTH + DEPTH_RD_TAG_WIDTH)
typedef ap_uint<DEPTH_RD_OFFSET_WIDTH> depth_rd_offset_dt;
typedef ap_uint<DEPTH_RD_INDEX_WIDTH> depth_rd_index_dt;
typedef ap_uint<DEPTH_RD_TAG_WIDTH> depth_rd_tag_dt;
typedef ap_uint<DEPTH_RD_ADDR_WIDTH> depth_rd_addr_dt;

#define DEPTH_WR_CACHE_SIZE 8192
#define DEPTH_WR_OFFSET_WIDTH 6
#define DEPTH_WR_INDEX_WIDTH 13
#define DEPTH_WR_TAG_WIDTH 13
#define DEPTH_WR_TAG_INIT_VAL 0x1FFF
#define DEPTH_WR_ADDR_WIDTH (DEPTH_WR_INDEX_WIDTH + DEPTH_WR_TAG_WIDTH)
typedef ap_uint<DEPTH_WR_OFFSET_WIDTH> depth_wr_offset_dt;
typedef ap_uint<DEPTH_WR_INDEX_WIDTH> depth_wr_index_dt;
typedef ap_uint<DEPTH_WR_TAG_WIDTH> depth_wr_tag_dt;
typedef ap_uint<DEPTH_WR_ADDR_WIDTH> depth_wr_addr_dt;

typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_uint<5> uint5_dt;
typedef ap_uint<6> uint6_dt;
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

static void frontier_splitter(
		hls::stream<int> &frontier_stream,
		hls::stream<uint1_dt> &inspect_done_stream,
		hls::stream<int> &frontierA_stream,
		hls::stream<int> &frontierB_stream,
		hls::stream<int> &frontierC_stream,
		hls::stream<int> &frontierD_stream,
		hls::stream<uint1_dt> &frontierA_done_stream,
		hls::stream<uint1_dt> &frontierB_done_stream,
		hls::stream<uint1_dt> &frontierC_done_stream,
		hls::stream<uint1_dt> &frontierD_done_stream
		){

	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;
	uint6_dt counter = 0;

	while(frontier_empty != 1 || done != 1){
    #pragma HLS PIPELINE
		frontier_empty = frontier_stream.empty();
		done_empty = inspect_done_stream.empty();
		if(frontier_empty != 1){
			uint32_dt vidx = frontier_stream.read();
			if(counter < 16){
				frontierA_stream << vidx;
			}
			else if(counter < 32){
				frontierB_stream << vidx;
			}
			else if(counter < 48){
				frontierC_stream << vidx;
			}
			else{
				frontierD_stream << vidx;
			}
			counter++;
		}

		if(done_empty != 1 && frontier_empty == 1){
			done = inspect_done_stream.read();
		}
	}

	frontierA_done_stream << 1;
	frontierB_done_stream << 1;
	frontierC_done_stream << 1;
	frontierD_done_stream << 1;
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

	cia_cache_dt cache_data[1];
   	cia_tag_dt cache_tag[1] = {CIA_TAG_INIT_VAL};

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

	hash_tag_dt hash_table0[HASH_SIZE];
//#pragma HLS RESOURCE variable=hash_table0 core=RAM_S2P_BRAM latency=1
//#pragma HLS DEPENDENCE variable=hash_table0 inter false
//#pragma HLS DEPENDENCE variable=hash_table0 intra raw true

	hash_tag_dt hash_table1[HASH_SIZE];
//#pragma HLS RESOURCE variable=hash_table1 core=RAM_S2P_BRAM latency=1
//#pragma HLS DEPENDENCE variable=hash_table1 inter false
//#pragma HLS DEPENDENCE variable=hash_table1 intra raw true

	hash_tag_dt hash_table2[HASH_SIZE];
//#pragma HLS RESOURCE variable=hash_table2 core=RAM_S2P_BRAM latency=1
//#pragma HLS DEPENDENCE variable=hash_table2 inter false
//#pragma HLS DEPENDENCE variable=hash_table2 intra raw true

	hash_tag_dt hash_table3[HASH_SIZE];
//#pragma HLS RESOURCE variable=hash_table3 core=RAM_S2P_BRAM latency=1
//#pragma HLS DEPENDENCE variable=hash_table3 inter false
//#pragma HLS DEPENDENCE variable=hash_table3 intra raw true
  

	uint2_dt hash_pri = 0;
	//uint2_dt hash_pri = 0;
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

	int512_dt cache_data[DEPTH_RD_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_data core=RAM_T2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_data inter false

	depth_rd_tag_dt cache_tag[DEPTH_RD_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_tag core=RAM_T2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_tag inter false


    stage4_init:
	for(int i = 0; i < DEPTH_RD_CACHE_SIZE; i++){
    #pragma HLS PIPELINE II=1
		cache_tag[i] = DEPTH_RD_TAG_INIT_VAL;
	}

    stage4_main:
	while((ciao_empty != 1) || (done != 1)){
    #pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
			uint32_dt vidx = ciao_stream.read();
			depth_rd_offset_dt offset = vidx.range(DEPTH_RD_OFFSET_WIDTH - 1, 0);
			depth_rd_index_dt index = vidx.range(DEPTH_RD_OFFSET_WIDTH + DEPTH_RD_INDEX_WIDTH - 1,
				   	DEPTH_RD_OFFSET_WIDTH);
			depth_rd_tag_dt tag = vidx.range(31, DEPTH_RD_OFFSET_WIDTH + DEPTH_RD_INDEX_WIDTH);
			uint32_dt addr  = vidx.range(31, DEPTH_RD_OFFSET_WIDTH);
			depth_rd_tag_dt _tag = cache_tag[index];

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

static void ngb_merger(
		hls::stream<int> &ngbA_stream,
		hls::stream<int> &ngbB_stream,
		hls::stream<int> &ngbC_stream,
		hls::stream<int> &ngbD_stream,
		hls::stream<uint1_dt> &ngbA_done_stream,
		hls::stream<uint1_dt> &ngbB_done_stream,
		hls::stream<uint1_dt> &ngbC_done_stream,
		hls::stream<uint1_dt> &ngbD_done_stream,
		hls::stream<int> &ngb_stream,
		hls::stream<uint1_dt> &ngb_done_stream
		){

	uint1_dt Adone = 0;
	uint1_dt Bdone = 0;
	uint1_dt Cdone = 0;
	uint1_dt Ddone = 0;

	uint1_dt ngbA_empty = 0;
	uint1_dt ngbB_empty = 0;
	uint1_dt ngbC_empty = 0;
	uint1_dt ngbD_empty = 0;

	uint1_dt doneA_empty = 0;
	uint1_dt doneB_empty = 0;
	uint1_dt doneC_empty = 0;
	uint1_dt doneD_empty = 0;

	while(ngbA_empty != 1 || ngbB_empty != 1 
			|| ngbC_empty != 1 || ngbD_empty != 1
			|| Adone != 1 || Bdone != 1 
			|| Cdone != 1 || Ddone != 1
			){
		ngbA_empty = ngbA_stream.empty();
		ngbB_empty = ngbB_stream.empty();
		ngbC_empty = ngbC_stream.empty();
		ngbD_empty = ngbD_stream.empty();

		doneA_empty = ngbA_done_stream.empty();
		doneB_empty = ngbB_done_stream.empty();
		doneC_empty = ngbC_done_stream.empty();
		doneD_empty = ngbD_done_stream.empty();

		if(ngbA_empty != 1){
			uint32_dt vidx = ngbA_stream.read();
			ngb_stream << vidx;
		}

		if(ngbB_empty != 1){
			uint32_dt vidx = ngbB_stream.read();
			ngb_stream << vidx;
		}

		if(ngbC_empty != 1){
			uint32_dt vidx = ngbC_stream.read();
			ngb_stream << vidx;
		}

		if(ngbD_empty != 1){
			uint32_dt vidx = ngbD_stream.read();
			ngb_stream << vidx;
		}

		if(doneA_empty != 1 && ngbA_empty == 1){
			Adone = ngbA_done_stream.read();
		}

		if(doneB_empty != 1 && ngbB_empty == 1){
			Bdone = ngbB_done_stream.read();
		}

		if(doneC_empty != 1 && ngbC_empty == 1){
			Cdone = ngbC_done_stream.read();
		}
		if(doneD_empty != 1 && ngbD_empty == 1){
			Ddone = ngbD_done_stream.read();
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
	uint1_dt done = 0;
	uint1_dt ngb_empty = 0;
	uint1_dt done_empty = 0;

	int512_dt cache_data[DEPTH_WR_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_data core=RAM_2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_data inter false

	depth_wr_tag_dt cache_tag[DEPTH_WR_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_tag core=RAM_2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_tag inter false

	uint1_dt cache_dirty[DEPTH_WR_CACHE_SIZE];
//#pragma HLS RESOURCE variable=cache_dirty core=RAM_2P_BRAM
//#pragma HLS DEPENDENCE variable=cache_dirty inter false
#ifndef SW_EMU
	if(level == 0){
#endif
        stage5_init:
		for(uint32_dt i = 0; i < DEPTH_WR_CACHE_SIZE; i++){
        #pragma HLS PIPELINE II=1
			cache_tag[i] = DEPTH_WR_TAG_INIT_VAL;
			cache_dirty[i] = 0;
		}
#ifndef SW_EMU
	}
#endif

    stage5_main:
    while(ngb_empty != 1 || done != 1){
    #pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		ngb_empty = ngb_stream.empty();
		done_empty = ngb_done_stream.empty();

		if(ngb_empty != 1){
			uint32_dt vidx = ngb_stream.read();
			depth_wr_offset_dt offset = vidx.range(DEPTH_WR_OFFSET_WIDTH - 1, 0);
			depth_wr_index_dt index = vidx.range(DEPTH_WR_OFFSET_WIDTH + DEPTH_WR_INDEX_WIDTH - 1, 
					DEPTH_WR_OFFSET_WIDTH);
			depth_wr_tag_dt tag = vidx.range(31, DEPTH_WR_OFFSET_WIDTH + DEPTH_WR_INDEX_WIDTH);
			depth_wr_tag_dt _tag = cache_tag[index];
			depth_wr_addr_dt _addr = (_tag, index);
			depth_wr_addr_dt addr = (tag, index);
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
	for(uint32_dt i = 0; i < DEPTH_WR_CACHE_SIZE; i++){
    #pragma HLS PIPELINE
		if(cache_dirty[i] == 1){
			depth_wr_tag_dt tag = cache_tag[i];
			depth_wr_addr_dt addr = (tag, i.range(DEPTH_WR_INDEX_WIDTH - 1, 0));
			depth[addr] = cache_data[i];
			cache_dirty[i] = 0;
		}
	}

}


extern "C" {
void bfs(
		const int512_dt *depth_for_inspect,

		const int512_dt *depthA_for_expand,
		const int512_dt *depthB_for_expand,
		const int512_dt *depthC_for_expand,
		const int512_dt *depthD_for_expand,

		int512_dt *depth_for_update,

		const rpa_cache_dt *rpaoAA, 
		const rpa_cache_dt *rpaoAB, 

		const rpa_cache_dt *rpaoBA, 
		const rpa_cache_dt *rpaoBB, 
		
		const rpa_cache_dt *rpaoCA, 
		const rpa_cache_dt *rpaoCB,
		
		const rpa_cache_dt *rpaoDA, 
		const rpa_cache_dt *rpaoDB, 

		const int512_dt *ciaoA,
		const int512_dt *ciaoB,
		const int512_dt *ciaoC,
		const int512_dt *ciaoD,

		int *frontier_size,
		const int vertex_num,
		const char level
		)
{
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0

#pragma HLS INTERFACE m_axi port=depthA_for_expand offset=slave bundle=gmem11
#pragma HLS INTERFACE m_axi port=depthB_for_expand offset=slave bundle=gmem12
#pragma HLS INTERFACE m_axi port=depthC_for_expand offset=slave bundle=gmem13
#pragma HLS INTERFACE m_axi port=depthD_for_expand offset=slave bundle=gmem14

#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem2

#pragma HLS INTERFACE m_axi port=rpaoAA offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=rpaoAB offset=slave bundle=gmem31

#pragma HLS INTERFACE m_axi port=rpaoBA offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=rpaoBB offset=slave bundle=gmem32

#pragma HLS INTERFACE m_axi port=rpaoCA offset=slave bundle=gmem33
#pragma HLS INTERFACE m_axi port=rpaoCB offset=slave bundle=gmem33

#pragma HLS INTERFACE m_axi port=rpaoDA offset=slave bundle=gmem34
#pragma HLS INTERFACE m_axi port=rpaoDB offset=slave bundle=gmem34

#pragma HLS INTERFACE m_axi port=ciaoA offset=slave bundle=gmem41
#pragma HLS INTERFACE m_axi port=ciaoB offset=slave bundle=gmem42
#pragma HLS INTERFACE m_axi port=ciaoC offset=slave bundle=gmem43
#pragma HLS INTERFACE m_axi port=ciaoD offset=slave bundle=gmem44

#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem5
#pragma HLS INTERFACE s_axilite port=vertex_num bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

char level_plus1 = level + 1;
int word_num = vertex_num >> 6;

hls::stream<int512_dt> depth_inspect_stream;
hls::stream<uint1_dt> inspect_done_stream;
hls::stream<int> frontier_stream;

hls::stream<uint1_dt> frontierA_done_stream;
hls::stream<uint1_dt> frontierB_done_stream;
hls::stream<uint1_dt> frontierC_done_stream;
hls::stream<uint1_dt> frontierD_done_stream;

hls::stream<int> frontierA_stream;
hls::stream<int> frontierB_stream;
hls::stream<int> frontierC_stream;
hls::stream<int> frontierD_stream;

hls::stream<uint32_dt> startA_stream;
hls::stream<uint32_dt> startB_stream;
hls::stream<uint32_dt> startC_stream;
hls::stream<uint32_dt> startD_stream;

hls::stream<uint32_dt> endA_stream;
hls::stream<uint32_dt> endB_stream;
hls::stream<uint32_dt> endC_stream;
hls::stream<uint32_dt> endD_stream;

hls::stream<uint1_dt> rpaoA_done_stream;
hls::stream<uint1_dt> rpaoB_done_stream;
hls::stream<uint1_dt> rpaoC_done_stream;
hls::stream<uint1_dt> rpaoD_done_stream;

hls::stream<int> ciaoA_stream;
hls::stream<int> ciaoB_stream;
hls::stream<int> ciaoC_stream;
hls::stream<int> ciaoD_stream;

hls::stream<uint1_dt> ciaoA_done_stream;
hls::stream<uint1_dt> ciaoB_done_stream;
hls::stream<uint1_dt> ciaoC_done_stream;
hls::stream<uint1_dt> ciaoD_done_stream;

hls::stream<int> ngbA_stream;
hls::stream<int> ngbB_stream;
hls::stream<int> ngbC_stream;
hls::stream<int> ngbD_stream;

hls::stream<uint1_dt> ngbA_done_stream;
hls::stream<uint1_dt> ngbB_done_stream;
hls::stream<uint1_dt> ngbC_done_stream;
hls::stream<uint1_dt> ngbD_done_stream;

hls::stream<int> ciaoA_squeeze_stream;
hls::stream<int> ciaoB_squeeze_stream;
hls::stream<int> ciaoC_squeeze_stream;
hls::stream<int> ciaoD_squeeze_stream;

hls::stream<uint1_dt> ciaoA_squeeze_done_stream;
hls::stream<uint1_dt> ciaoB_squeeze_done_stream;
hls::stream<uint1_dt> ciaoC_squeeze_done_stream;
hls::stream<uint1_dt> ciaoD_squeeze_done_stream;

hls::stream<int> ngb_stream;
hls::stream<uint1_dt> ngb_done_stream;

#pragma HLS STREAM variable=depth_inspect_stream depth=64
#pragma HLS STREAM variable=frontier_stream depth=64
#pragma HLS STREAM variable=inspect_done_stream depth=4

#pragma HLS STREAM variable=frontierA_stream depth=64
#pragma HLS STREAM variable=frontierB_stream depth=64
#pragma HLS STREAM variable=frontierC_stream depth=64
#pragma HLS STREAM variable=frontierD_stream depth=64

#pragma HLS STREAM variable=frontierA_done_stream depth=4
#pragma HLS STREAM variable=frontierB_done_stream depth=4
#pragma HLS STREAM variable=frontierC_done_stream depth=4
#pragma HLS STREAM variable=frontierD_done_stream depth=4

#pragma HLS STREAM variable=rpaoA_done_stream depth=4
#pragma HLS STREAM variable=rpaoB_done_stream depth=4
#pragma HLS STREAM variable=rpaoC_done_stream depth=4
#pragma HLS STREAM variable=rpaoD_done_stream depth=4

#pragma HLS STREAM variable=startA_stream depth=64
#pragma HLS STREAM variable=startB_stream depth=64
#pragma HLS STREAM variable=startC_stream depth=64
#pragma HLS STREAM variable=startD_stream depth=64

#pragma HLS STREAM variable=endA_stream depth=64
#pragma HLS STREAM variable=endB_stream depth=64
#pragma HLS STREAM variable=endC_stream depth=64
#pragma HLS STREAM variable=endD_stream depth=64

#pragma HLS STREAM variable=ciaoA_stream depth=64
#pragma HLS STREAM variable=ciaoB_stream depth=64
#pragma HLS STREAM variable=ciaoC_stream depth=64
#pragma HLS STREAM variable=ciaoD_stream depth=64

#pragma HLS STREAM variable=ciaoA_done_stream depth=4
#pragma HLS STREAM variable=ciaoB_done_stream depth=4
#pragma HLS STREAM variable=ciaoC_done_stream depth=4
#pragma HLS STREAM variable=ciaoD_done_stream depth=4

#pragma HLS STREAM variable=ngbA_stream depth=64
#pragma HLS STREAM variable=ngbB_stream depth=64
#pragma HLS STREAM variable=ngbC_stream depth=64
#pragma HLS STREAM variable=ngbD_stream depth=64

#pragma HLS STREAM variable=ngb_stream depth=64
#pragma HLS STREAM variable=ngb_done_stream depth=4

#pragma HLS STREAM variable=ngbA_done_stream depth=4
#pragma HLS STREAM variable=ngbB_done_stream depth=4
#pragma HLS STREAM variable=ngbC_done_stream depth=4
#pragma HLS STREAM variable=ngbD_done_stream depth=4

#pragma HLS STREAM variable=ciaoA_squeeze_stream depth=64
#pragma HLS STREAM variable=ciaoB_squeeze_stream depth=64
#pragma HLS STREAM variable=ciaoC_squeeze_stream depth=64
#pragma HLS STREAM variable=ciaoD_squeeze_stream depth=64

#pragma HLS STREAM variable=ciaoA_squeeze_done_stream depth=4
#pragma HLS STREAM variable=ciaoB_squeeze_done_stream depth=4
#pragma HLS STREAM variable=ciaoC_squeeze_done_stream depth=4
#pragma HLS STREAM variable=ciaoD_squeeze_done_stream depth=4

#pragma HLS dataflow
bfs_stage0(depth_for_inspect, depth_inspect_stream, word_num);
bfs_stage1(frontier_size, depth_inspect_stream, inspect_done_stream, frontier_stream, word_num, level);

// frontier splitter
frontier_splitter(frontier_stream, inspect_done_stream, 
		frontierA_stream, frontierB_stream, 
		frontierC_stream, frontierD_stream, 
		frontierA_done_stream, frontierB_done_stream,
		frontierC_done_stream, frontierD_done_stream
		);

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

// branch C
bfs_stage2(rpaoCA, rpaoCB, frontierC_stream, frontierC_done_stream, 
		startC_stream, endC_stream, rpaoC_done_stream);
bfs_stage3(ciaoC, startC_stream, endC_stream, rpaoC_done_stream, 
		ciaoC_stream, ciaoC_done_stream, level);
bfs_squeeze_ciao(ciaoC_stream, ciaoC_done_stream, 
		ciaoC_squeeze_stream, ciaoC_squeeze_done_stream, level);
bfs_stage4(depthC_for_expand, ciaoC_squeeze_stream, ciaoC_squeeze_done_stream, 
		ngbC_stream, ngbC_done_stream, level);

// branch D

bfs_stage2(rpaoDA, rpaoDB, frontierD_stream, frontierD_done_stream, 
		startD_stream, endD_stream, rpaoD_done_stream);
bfs_stage3(ciaoD, startD_stream, endD_stream, rpaoD_done_stream, 
		ciaoD_stream, ciaoD_done_stream, level);
bfs_squeeze_ciao(ciaoD_stream, ciaoD_done_stream, 
		ciaoD_squeeze_stream, ciaoD_squeeze_done_stream, level);
bfs_stage4(depthD_for_expand, ciaoD_squeeze_stream, ciaoD_squeeze_done_stream, 
		ngbD_stream, ngbD_done_stream, level);


//ngb stream merger
ngb_merger(ngbA_stream, ngbB_stream, ngbC_stream, ngbD_stream, 
		ngbA_done_stream, ngbB_done_stream, ngbC_done_stream, ngbD_done_stream,
		ngb_stream, ngb_done_stream);

bfs_stage5(depth_for_update, ngb_stream, ngb_done_stream, level_plus1, level);
}
}



