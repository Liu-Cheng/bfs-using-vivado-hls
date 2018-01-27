#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define SW_EMU
#define SW_ANALYSIS

#define HASH_SIZE (16384)
#define HASH_ID_WIDTH 14
#define HASH_TAG_WIDTH 18
typedef ap_uint<HASH_ID_WIDTH> hash_index_dt;
typedef ap_uint<HASH_TAG_WIDTH> hash_tag_dt;

#define LEN4_WIDTH 4
#define INDEX4_WIDTH 12
#define TAG4_WIDTH 16
#define INIT4_VAL 0xFFFF
#define CACHE4_SIZE 4096

#define LEN6_WIDTH 6
#define INDEX6_WIDTH 14
#define TAG6_WIDTH 12
#define INIT6_VAL 0xFFF
#define CACHE6_SIZE 16384

typedef ap_uint<LEN4_WIDTH> offset4_dt;
typedef ap_uint<INDEX4_WIDTH> index4_dt;
typedef ap_uint<TAG4_WIDTH> tag4_dt;

typedef ap_uint<LEN6_WIDTH> offset6_dt;
typedef ap_uint<INDEX6_WIDTH> index6_dt;
typedef ap_uint<TAG6_WIDTH> tag6_dt;

typedef ap_uint<1> uint1_dt;
typedef ap_uint<2> uint2_dt;
typedef ap_uint<4> uint4_dt;
typedef ap_uint<8> uint8_dt;
typedef ap_uint<14> uint14_dt;
typedef ap_int<16> int16_dt;
typedef ap_uint<26> uint26_dt;
typedef ap_uint<32> uint32_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<512> int512_dt;

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

		if(done_empty != 1 && frontier_empty == 1){
			done = inspect_done_stream.read();
			rpao_done_stream << 1;
		}
	}
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
#ifdef SW_ANALYSIS
	int hit_cnt = 0;
	int ciao_cnt = 0;
	static int hit_sum = 0;
	static int ciao_sum = 0;
#endif

	// offset 4, index 14, tag 14
	int512_dt cache_data_set0[CACHE4_SIZE];
	int512_dt cache_data_set1[CACHE4_SIZE];
	tag4_dt cache_tag_set0[CACHE4_SIZE];
	tag4_dt cache_tag_set1[CACHE4_SIZE];
	uint1_dt cache_pri[CACHE4_SIZE]; // determine which to replace

#ifndef SW_EMU
	if(level == 0){
#endif
		for(int i = 0; i < CACHE4_SIZE; i++){
        #pragma HLS pipeline
			cache_tag_set0[i] = INIT4_VAL;
			cache_tag_set1[i] = INIT4_VAL;
			cache_pri[i] = 0;
		}
#ifndef SW_EMU
	}
#endif

	while((rpao_empty != 1) || (done != 1)){
		rpao_empty = rpao_stream.empty();
		done_empty = rpao_done_stream.empty();
		
		if(rpao_empty != 1){
			int64_dt rpitem = rpao_stream.read();
			uint32_dt start = rpitem.range(31, 0);
			uint32_dt end = rpitem.range(63, 32);
			for(uint32_dt i = start; i < end; i++){
			#pragma HLS LOOP_FLATTEN off
            #pragma HLS pipeline
				uint32_dt addr = i.range(31, LEN4_WIDTH);
				offset4_dt offset = i.range(LEN4_WIDTH - 1 ,0);
				index4_dt index = i.range(INDEX4_WIDTH + LEN4_WIDTH - 1, LEN4_WIDTH);
				tag4_dt tag = i.range(31, INDEX4_WIDTH + LEN4_WIDTH);
				uint1_dt _pri = cache_pri[index];
				tag4_dt _tag_set0 = cache_tag_set0[index];
				uint14_dt _tag_set1 = cache_tag_set1[index];
#ifdef SW_ANALYSIS
				ciao_cnt++;
#endif

				// cache hit
				if(_tag_set0 == tag){
#ifdef SW_ANALYSIS
					hit_cnt++;
#endif
					ciao_stream << cache_data_set0[index].range((offset+1)*32 - 1, offset * 32);
				}
				else if(_tag_set1 == tag){
#ifdef SW_ANALYSIS
					hit_cnt++;
#endif
					ciao_stream << cache_data_set1[index].range((offset+1)*32 - 1, offset * 32);
				}

				// cache miss
				else{
					int512_dt word = ciao[addr];
					if(_pri == 0){
						cache_data_set0[index] = word;
						cache_tag_set0[index] = tag;
						cache_pri[index] = 1;
					}
					else{
						cache_data_set1[index] = word;
						cache_tag_set1[index] = tag;
						cache_pri[index] = 0;
					}
					ciao_stream << word.range((offset+1)*32 - 1, offset * 32);
				}
			}
		}

		if(done_empty != 1 && rpao_empty == 1){
			done = rpao_done_stream.read();
			ciao_done_stream << 1;
		}
	}

#ifdef SW_ANALYSIS
	hit_sum += hit_cnt;
	ciao_sum += ciao_cnt;
	if(ciao_cnt > 0 && ciao_sum > 0){
		printf("Level = %d, hit_cnt = %d, ciao cache hit rate = %f\n", (int)level, hit_cnt, hit_cnt*1.0/ciao_cnt);
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

//#ifdef DEBUG
//	static int hit_sum = 0;
//	static int ciao_sum = 0;
//	int hit_cnt = 0;
//	int ciao_cnt = 0;
//#endif

	hash_tag_dt hash_table0[HASH_SIZE];
	hash_tag_dt hash_table1[HASH_SIZE];
	hash_tag_dt hash_table2[HASH_SIZE];
	hash_tag_dt hash_table3[HASH_SIZE];
	uint2_dt hash_pri = 0;
#ifndef SW_EMU
   	if(level == 0){
#endif
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

	while(ciao_empty != 1 || done != 1){
    #pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();

		if(ciao_empty != 1){
//#ifdef DEBUG
//			ciao_cnt++;	
//#endif
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
//#ifdef DEBUG
//			else{
//				hit_cnt++;
//			}
//#endif
		}

		if(done_empty != 1 && ciao_empty == 1){
			done = ciao_done_stream.read();
			ciao_squeeze_done_stream << 1;
		}
	}
//#ifdef DEBUG
//	if(ciao_cnt > 0){
//		ciao_sum += ciao_cnt;
//		hit_sum += hit_cnt;
//		printf("hash table hit %d  %0.3f\n", hit_cnt, 100.0*hit_cnt/ciao_cnt);
//		printf("total hit %d, %0.3f\n", hit_sum, 100.0*hit_sum/ciao_sum);
//		printf("---------------------------\n");
//	}
//#endif
}



// load depth for inspection 
// Why should I invalidate the cache in each bfs iteration?
static void bfs_stage4(
		int512_dt *depth,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		const char level_plus1,
		const char level)
{
	// Cache data structure
	bool cache_dirty[CACHE6_SIZE];
	tag6_dt cache_tag[CACHE6_SIZE];
	int512_dt cache_data[CACHE6_SIZE];

#ifdef SW_ANALYSIS
	int read_hit = 0;
	int write_hit = 0;
	int read_num = 0;
	int write_num = 0;
	static int hit_sum = 0;
	static int read_sum = 0;
	static int write_sum = 0;
#endif

#ifndef SW_EMU
	if(level_plus1 == 1){
#endif
		for(int i = 0; i < CACHE6_SIZE; i++){
        #pragma HLS pipeline
			cache_tag[i] = INIT6_VAL;
			cache_dirty[i] = false;
		}
#ifndef SW_EMU
	}
#endif

	int done = 0;
	int ciao_empty = 0;
	int done_empty = 0;

    while(ciao_empty != 1 || done != 1){
    #pragma HLS pipeline

		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		if(ciao_empty != 1){
#ifdef SW_ANALYSIS
			read_num++;
#endif
			uint32_dt vidx = ciao_stream.read();
			offset6_dt req_offset = vidx.range(LEN6_WIDTH - 1, 0);
			index6_dt req_idx = vidx.range(LEN6_WIDTH + INDEX6_WIDTH - 1, LEN6_WIDTH);
			tag6_dt req_tag = vidx.range(31, LEN6_WIDTH + INDEX6_WIDTH);
			tag6_dt old_tag = cache_tag[req_idx];
			int512_dt cache_line = cache_data[req_idx];
			uint32_dt req_line_addr = vidx >> LEN6_WIDTH;
			uint26_dt cache_line_addr = (old_tag, req_idx);

			// cache hit
			if(old_tag == req_tag){
#ifdef SW_ANALYSIS
				read_hit++;
#endif
				char d = cache_line.range(((req_offset+1)*8) - 1, (req_offset*8));
				if(d == -1){
#ifdef SW_ANALYSIS
					write_num++;
					write_hit++;
#endif
					cache_data[req_idx].range(((req_offset+1)*8) - 1, (req_offset*8)) = level_plus1;
					cache_dirty[req_idx] = true;
				}
			}

			// cache miss
			else{
				bool dirty = cache_dirty[req_idx];
				if(dirty == true){	
					depth[cache_line_addr] = cache_line;
				}
				int512_dt new_cache_line = depth[req_line_addr];
				char d = new_cache_line.range(((req_offset+1)*8) - 1, (req_offset*8));
				if(d == -1){
#ifdef SW_ANALYSIS
					write_num++;
#endif
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

	for(int i = 0; i < CACHE6_SIZE; i++){
    #pragma HLS pipeline
		bool dirty = cache_dirty[i];
		if(dirty == true){
			uint26_dt cache_line_addr;
			cache_line_addr.range(INDEX6_WIDTH - 1, 0) = i;
			cache_line_addr.range(INDEX6_WIDTH + TAG6_WIDTH - 1, INDEX6_WIDTH) = cache_tag[i];
			depth[cache_line_addr] = cache_data[i];
			cache_dirty[i] = false;
		}
	}
#ifdef SW_ANALYSIS
	read_sum += read_num;
	write_sum += write_num;
	hit_sum += read_hit + write_hit;
	if(read_num > 0 && write_num > 0){
		printf("Level = %d, read_num: %d, read hit rate: %f\n", (int)level, read_num, read_hit*1.0/read_num);
		printf("Level = %d, write_num: %d, write hit rate = %f\n", (int)level, write_num, write_hit*1.0/write_num);
		printf("total cache hit rate: %f\n", hit_sum * 1.0 /(read_sum + write_sum));
	}
#endif
}


extern "C" {
	void bfs(
			const char *depth_for_inspect,
			int512_dt *depth_for_update,
			const int *rpao, 
			const int512_dt *ciao,
			int *frontier_size,
			const int vertex_num,
			const char level
		)
{
//#pragma HLS DATA_PACK variable=depth_for_update
#pragma HLS INTERFACE m_axi port=depth_for_inspect offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=depth_for_update offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=rpao offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=ciao offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=frontier_size offset=slave bundle=gmem4
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

#pragma HLS STREAM variable=depth_inspect_stream depth=32
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
bfs_stage4(depth_for_update, ciao_squeeze_stream, ciao_squeeze_done_stream, level_plus1, level);

}
}

