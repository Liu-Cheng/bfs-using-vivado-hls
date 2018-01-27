#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>
#define HASH_SIZE 4096
#define HASH_MASK 0x7FF
#define HUB_THRESHOLD 128

#define THREAD_NUM 4
#define BUFFER_SIZE 32
typedef ap_int<16> int16_dt;
typedef ap_int<12> int12_dt;
typedef ap_int<40> int40_dt;
typedef ap_int<8> int8_dt;

typedef ap_uint<1> uint1_dt;
typedef ap_int<64> int64_dt;
typedef ap_int<32> int32_dt;

// load depth for inspection 
static void read_depth(
		int32_dt *depth, 
		hls::stream<int32_dt> &depth_inspect_stream,
        int inspect_num)
{
	int len;
	int32_dt buffer[BUFFER_SIZE];
	for (int i = 0; i < inspect_num; i += BUFFER_SIZE){
		if(i + BUFFER_SIZE <= inspect_num){
			len = BUFFER_SIZE;
		}
		else{
			len = inspect_num - i;
		}
		for(int j = 0; j < len; j++){
#pragma HLS pipeline
			buffer[j] = depth[i+j];
		}
		for(int j = 0; j < len; j++){
#pragma HLS pipeline
			depth_inspect_stream << buffer[j];
		}
	}
}

// inspect depth for frontier 
static void frontier_inspect(
		hls::stream<int32_dt> &depth_inspect_stream, 
		hls::stream<int> &frontier_stream0,
		hls::stream<int> &frontier_stream1,
		hls::stream<int> &frontier_stream2,
		hls::stream<int> &frontier_stream3,
		int *frontier_size,
		int inspect_num,
		char level)
{
	int32_dt data;
	char d0;
	char d1;
	char d2;
	char d3;
	int vidx0;
	int vidx1;
	int vidx2;
	int vidx3;
	int count0 = 0;
	int count1 = 0;
	int count2 = 0;
	int count3 = 0;
    inspect: for (int i = 0; i < inspect_num; i++){
#pragma HLS pipeline
		data = depth_inspect_stream.read();

		d0 = data.range(7, 0);
		d1 = data.range(15, 8);
		d2 = data.range(23, 16);
		d3 = data.range(31, 24);

		if(d0 == level){
			vidx0 = (4*i);
			count0++;
		}
		else{
			vidx0 = -1;
		}

		if(d1 == level){
			vidx1 = (4*i + 1);
			count1++;
		}
		else{
			vidx1 = -1;
		}

		if(d2 == level){
			vidx2 = (4*i + 2);
			count2++;
		}
		else{
			vidx2 = -1;
		}

		if(d3 == level){
			vidx3 = (4*i + 3);
			count3++;
		}
		else{
			vidx3 = -1;
		}

		frontier_stream0 << vidx0;
		frontier_stream1 << vidx1;
		frontier_stream2 << vidx2;
		frontier_stream3 << vidx3;
		if(i == inspect_num - 1){
			*frontier_size = count0 + count1 + count2 + count3;
		}
	}
}

// Read rpao of the frontier 
static void read_rpao(
		int *rpao,
		hls::stream<int> &frontier_stream,
		hls::stream<int64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream,
		hls::stream<int> &hub_stream,
		hls::stream<int> &frontier_bypass_stream,
		int inspect_num)
{
	int vidx,start,end;
	int rpidx;
	int64_dt buffer[1];
	int64_dt rpitem;
	for(int i = 0; i < inspect_num; i++){
#pragma HLS pipeline
		vidx = frontier_stream.read();
#pragma HLS pipeline
		frontier_bypass_stream << vidx;
		if(vidx >= 0){
		if(buffer[0].range(31,0) == vidx){
			start = buffer[0].range(63,32);
		}
		else {
			start = rpao[vidx];
		}
		end = rpao[vidx+1];
		buffer[0].range(31, 0) = vidx + 1;
		buffer[0].range(63, 32) = end;
		rpitem.range(31, 0) = start;
		rpitem.range(63, 32) = end;
		if(end - start >= HUB_THRESHOLD){
			hub_stream << vidx;
		}
		rpao_stream << rpitem;
		}
		if(i == inspect_num - 1){
			rpao_done_stream << 1;
		}
	}
}

// read ciao
static void read_ciao(
		int *ciao,
		hls::stream<int64_dt> &rpao_stream,
		hls::stream<uint1_dt> &rpao_done_stream,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		hls::stream<int> &hub_stream,
		const int level
		)
{
	uint1_dt rpao_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;
	int start, end;
	int64_dt rpitem;
	int frontier_table0[HASH_SIZE];
	int frontier_table1[HASH_SIZE];
	uint1_dt flag_table[HASH_SIZE];
	while((rpao_empty != 1) || (done != 1)){
		rpao_empty = rpao_stream.empty();
		done_empty = rpao_done_stream.empty();
	 if(hub_stream.empty() != 1){
	  	int hub_vidx = hub_stream.read();
		int12_dt hash_idx = hub_vidx & HASH_MASK;
		uint1_dt flag = flag_table[hash_idx];
   		if(flag == 0){
	    		frontier_table0[hash_idx] = hub_vidx; 
	    		flag_table[hash_idx] = 1; 
	 	}
		else {
			frontier_table1[hash_idx] = hub_vidx;
			flag_table[hash_idx] = 0;
		}
	 }
		if(rpao_empty != 1){
			rpitem = rpao_stream.read();
			start = rpitem.range(31, 0);
			end = rpitem.range(63, 32);
			if(end - start >= 2){
				for(int i = start; i < end; i++){
		#pragma HLS pipeline
					  ciao_stream << ciao[i];
				}
			}
			else if(end - start == 1){
				int12_dt hash_idx = start & HASH_MASK;
				int hash_val0 = frontier_table0[hash_idx];
				int hash_val1 = frontier_table1[hash_idx];
				if(hash_val0 != start && hash_val1 != start){
					ciao_stream << ciao[start];
				}
			}
		}
	
		else{
			if(done_empty != 1){
			done = rpao_done_stream.read();
			ciao_done_stream << 1;
			}
		}  	
	}
}

// update depth
static void update_depth(
		char *depth,
		hls::stream<int> &frontier_stream,
		hls::stream<int> &ciao_stream,
		hls::stream<uint1_dt> &ciao_done_stream,
		int level
		)
{
	int written_table0[HASH_SIZE];
	int written_table1[HASH_SIZE];
	uint1_dt flag_table[HASH_SIZE];

	int frontier_table[HASH_SIZE];

	uint1_dt done = 0;
	uint1_dt ciao_empty = 0;
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
//	int vidx;
	
	if(!level){	
		for (int i=0; i < HASH_SIZE; i ++){	
			written_table0[i] = 0;
			written_table1[i] = 0;
			flag_table[i] = 0;
		}
		for (int i=0; i < HASH_SIZE; i ++){
			frontier_table[i] = 0;
		}
	}

	while((ciao_empty != 1) ||(frontier_empty !=1) || (done != 1)){
#pragma HLS pipeline
		ciao_empty = ciao_stream.empty();
		done_empty = ciao_done_stream.empty();
		frontier_empty = frontier_stream.empty();
		if(frontier_empty != 1){
			int frontier_vidx = frontier_stream.read();
			int frontier_hash_idx = frontier_vidx & HASH_MASK;
			frontier_table[frontier_hash_idx] = frontier_vidx;
		}
		if(ciao_empty != 1){
			int vidx = ciao_stream.read();
			//**********look up the HASH table to see whether written to avoid write redundently ************//
			int hash_idx = vidx & HASH_MASK;
			int hash_val0 = written_table0[hash_idx];
			int hash_val1 = written_table1[hash_idx];
			int hash_frontier_val = frontier_table[hash_idx];
		
			if(hash_val0 != vidx && hash_val1 != vidx &&  hash_frontier_val!= vidx){
				if(depth[vidx] == -1){
					depth[vidx] = level + 1;	
				}
			}

				//*********************add written level vertex to the HASH table to record**********************//

				//-------- neighbor part-------------//
				//hash_idx = vidx & HASH_MASK;
				uint1_dt flag = flag_table[hash_idx];
				if(!flag){
					written_table0[hash_idx] = vidx;
					flag_table[hash_idx] = 1;
				}
				else{
					written_table1[hash_idx] = vidx;
				 	flag_table[hash_idx] = 0;
				}	
			//********************************* end record ****************I**********************************// 
		}

		if(done_empty != 1){
			done = ciao_done_stream.read();
		}
	}
}

extern "C" {
void bfs(
		int *rpao0, 
		int *rpao1,
		int *rpao2,
		int *rpao3,
		int *ciao0,
		int *ciao1,
		int *ciao2,
		int *ciao3,
		int32_dt *depth_for_inspect,
		char *depth_for_expand0,	
		char *depth_for_expand1,
		char *depth_for_expand2,
		char *depth_for_expand3,
		int *frontier_size,
		int vertexNum,
		char level
		)
{
#pragma HLS INTERFACE m_axi port=rpao0 offset=slave bundle=gmem00
#pragma HLS INTERFACE m_axi port=rpao1 offset=slave bundle=gmem01
#pragma HLS INTERFACE m_axi port=rpao2 offset=slave bundle=gmem02
#pragma HLS INTERFACE m_axi port=rpao3 offset=slave bundle=gmem03

#pragma HLS INTERFACE m_axi port=ciao0  offset=slave bundle=gmem10
#pragma HLS INTERFACE m_axi port=ciao1  offset=slave bundle=gmem11
#pragma HLS INTERFACE m_axi port=ciao2  offset=slave bundle=gmem12
#pragma HLS INTERFACE m_axi port=ciao3  offset=slave bundle=gmem13

#pragma HLS INTERFACE m_axi port=depth_for_inspect  offset=slave bundle=gmem20

#pragma HLS INTERFACE m_axi port=depth_for_expand0  offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=depth_for_expand1  offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=depth_for_expand2  offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=depth_for_expand3  offset=slave bundle=gmem33

#pragma HLS INTERFACE m_axi port=frontier_size  offset=slave bundle=gmem4

#pragma HLS INTERFACE s_axilite port=vertexNum bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    hls::stream<int32_dt> depth_inspect_stream;

	hls::stream<int> frontier_stream0;
	hls::stream<int> frontier_stream1;
	hls::stream<int> frontier_stream2;
	hls::stream<int> frontier_stream3;

	hls::stream<int> frontier_bypass_stream0;
	hls::stream<int> frontier_bypass_stream1;
	hls::stream<int> frontier_bypass_stream2;
	hls::stream<int> frontier_bypass_stream3;

	hls::stream<uint1_dt> inspect_done_stream0;
	hls::stream<uint1_dt> inspect_done_stream1;
	hls::stream<uint1_dt> inspect_done_stream2;
	hls::stream<uint1_dt> inspect_done_stream3;

	hls::stream<int64_dt> rpao_stream0;
	hls::stream<int64_dt> rpao_stream1;
	hls::stream<int64_dt> rpao_stream2;
	hls::stream<int64_dt> rpao_stream3;

	hls::stream<uint1_dt> rpao_done_stream0;
	hls::stream<uint1_dt> rpao_done_stream1;
	hls::stream<uint1_dt> rpao_done_stream2;
	hls::stream<uint1_dt> rpao_done_stream3;

	hls::stream<int> ciao_stream0;
	hls::stream<int> ciao_stream1;
	hls::stream<int> ciao_stream2;
	hls::stream<int> ciao_stream3;

	hls::stream<int> hub_stream0;	
	hls::stream<int> hub_stream1;
        hls::stream<int> hub_stream2;
        hls::stream<int> hub_stream3;

	hls::stream<uint1_dt> ciao_done_stream0;
	hls::stream<uint1_dt> ciao_done_stream1;
	hls::stream<uint1_dt> ciao_done_stream2;
	hls::stream<uint1_dt> ciao_done_stream3;

#pragma HLS STREAM variable=depth_inspect_stream depth=64

#pragma HLS STREAM variable=frontier_stream0 depth=64
#pragma HLS STREAM variable=frontier_stream1 depth=64
#pragma HLS STREAM variable=frontier_stream2 depth=64
#pragma HLS STREAM variable=frontier_stream3 depth=64

#pragma HLS STREAM variable=frontier_bypass_stream0 depth=64
#pragma HLS STREAM variable=frontier_bypass_stream1 depth=64
#pragma HLS STREAM variable=frontier_bypass_stream2 depth=64
#pragma HLS STREAM variable=frontier_bypass_stream3 depth=64

#pragma HLS STREAM variable=rpao_stream0 depth=64
#pragma HLS STREAM variable=rpao_stream1 depth=64
#pragma HLS STREAM variable=rpao_stream2 depth=64
#pragma HLS STREAM variable=rpao_stream3 depth=64

#pragma HLS STREAM variable=rpao_done_stream0 depth=4
#pragma HLS STREAM variable=rpao_done_stream1 depth=4
#pragma HLS STREAM variable=rpao_done_stream2 depth=4
#pragma HLS STREAM variable=rpao_done_stream3 depth=4

#pragma HLS STREAM variable=ciao_stream0 depth=64
#pragma HLS STREAM variable=ciao_stream1 depth=64
#pragma HLS STREAM variable=ciao_stream2 depth=64
#pragma HLS STREAM variable=ciao_stream3 depth=64

#pragma HLS STREAM variable=ciao_done_stream0 depth=4
#pragma HLS STREAM variable=ciao_done_stream1 depth=4
#pragma HLS STREAM variable=ciao_done_stream2 depth=4
#pragma HLS STREAM variable=ciao_done_stream3 depth=4
#pragma HLS STREAM variable=hub_stream0 depth=64
#pragma HLS STREAM variable=hub_stream1 depth=64
#pragma HLS STREAM variable=hub_stream2 depth=64
#pragma HLS STREAM variable=hub_stream3 depth=64


	int block = vertexNum/THREAD_NUM;
#pragma HLS dataflow
    read_depth(depth_for_inspect, depth_inspect_stream, block);
	frontier_inspect(depth_inspect_stream, 
			frontier_stream0, 
			frontier_stream1,
			frontier_stream2,
			frontier_stream3,
			frontier_size,
			block, 
			level);

	read_rpao(rpao0, frontier_stream0, rpao_stream0, rpao_done_stream0, hub_stream0,frontier_bypass_stream0,block);
	read_rpao(rpao1, frontier_stream1, rpao_stream1, rpao_done_stream1, hub_stream1,frontier_bypass_stream1,block);
	read_rpao(rpao2, frontier_stream2, rpao_stream2, rpao_done_stream2, hub_stream2,frontier_bypass_stream2,block);
	read_rpao(rpao3, frontier_stream3, rpao_stream3, rpao_done_stream3, hub_stream3,frontier_bypass_stream3,block);

	read_ciao(ciao0, rpao_stream0, rpao_done_stream0, ciao_stream0, ciao_done_stream0,hub_stream0,level);
	read_ciao(ciao1, rpao_stream1, rpao_done_stream1, ciao_stream1, ciao_done_stream1,hub_stream1,level);
	read_ciao(ciao2, rpao_stream2, rpao_done_stream2, ciao_stream2, ciao_done_stream2,hub_stream2,level);
	read_ciao(ciao3, rpao_stream3, rpao_done_stream3, ciao_stream3, ciao_done_stream3,hub_stream3,level);

    update_depth(depth_for_expand0,frontier_bypass_stream0, ciao_stream0, ciao_done_stream0, level);
    update_depth(depth_for_expand1,frontier_bypass_stream1, ciao_stream1, ciao_done_stream1, level);
    update_depth(depth_for_expand2,frontier_bypass_stream2, ciao_stream2, ciao_done_stream2, level);
    update_depth(depth_for_expand3,frontier_bypass_stream3, ciao_stream3, ciao_done_stream3, level);
}
}

