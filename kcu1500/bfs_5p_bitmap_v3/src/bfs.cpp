/**
 * File              : src/bfs.cpp
 * Author            : Cheng Liu <st.liucheng@gmail.com>
 * Date              : Aug 15, 2018
 * Last Modified Date: Dec 13, 2017
 * Last Modified By  : Cheng Liu <st.liucheng@gmail.com>
 */

#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

// 16 x 128K bit bitmap memory
#define BITMAP_MEM_DEPTH (256 * 1024) 
#define BUFFER_ADDR_WIDTH 18
#define BRAM_BANK_NUM 16
#define BUFFER_SIZE 16
#define BANK_BW 3 // 3 when pad == 8, 4 when pad == 16

typedef ap_uint<1>  uint1_dt;
typedef ap_uint<3>  uint3_dt;
typedef ap_uint<18> uint18_dt;
typedef ap_int<32>  graph_dt;
typedef ap_int<64>  rpa_dt;
typedef ap_int<256> cia_dt;

static void read_rpa(
		const rpa_dt            *rpa,
		hls::stream<rpa_dt>     &rpa_stream,
		const int               frontier_size
		)
{
    read_rpa:
	for(int i = 0; i < frontier_size; i++){
    #pragma HLS pipeline II=1
		rpa_stream << rpa[i];
	}
}

// Read cia of the frontier 
static void read_cia(
		const cia_dt            *cia,
		hls::stream<rpa_dt>     &rpa_stream,
		hls::stream<cia_dt>     &cia_stream,
		hls::stream<uint1_dt>   &cia_done_stream,
		const int               frontier_size
		)
{
	
    read_cia:
	for(int i = 0; i < frontier_size; i++){
		rpa_dt   word  = rpa_stream.read();
		graph_dt num   = (word.range(63, 32)) >> BANK_BW;
		graph_dt start = (word.range(31, 0))  >> BANK_BW;

		for(int j = 0; j < num; j++){
        #pragma HLS pipeline II=1
			cia_stream << cia[start + j];
		}
	}

	cia_done_stream << 1;
}

// Traverse cia 
static void traverse_cia(
		hls::stream<cia_dt>    &cia_stream,
		hls::stream<uint1_dt>  &cia_done_stream,

		hls::stream<graph_dt>  &next_frontier_stream0,
		hls::stream<graph_dt>  &next_frontier_stream1,
		hls::stream<graph_dt>  &next_frontier_stream2,
		hls::stream<graph_dt>  &next_frontier_stream3,
		hls::stream<graph_dt>  &next_frontier_stream4,
		hls::stream<graph_dt>  &next_frontier_stream5,
		hls::stream<graph_dt>  &next_frontier_stream6,
		hls::stream<graph_dt>  &next_frontier_stream7,

		hls::stream<uint1_dt>  &next_frontier_done_stream0,
		hls::stream<uint1_dt>  &next_frontier_done_stream1,
		hls::stream<uint1_dt>  &next_frontier_done_stream2,
		hls::stream<uint1_dt>  &next_frontier_done_stream3,
		hls::stream<uint1_dt>  &next_frontier_done_stream4,
		hls::stream<uint1_dt>  &next_frontier_done_stream5,
		hls::stream<uint1_dt>  &next_frontier_done_stream6,
		hls::stream<uint1_dt>  &next_frontier_done_stream7,

		int                    *next_frontier_size,
		graph_dt               root_vidx,
		const char             level
		)
{
	int done = 0;
	int cia_empty = 0;
	int done_empty = 0;

	static uint1_dt bitmap0[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap1[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap2[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap3[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap4[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap5[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap6[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap7[BITMAP_MEM_DEPTH];

   	if(level == 0){
        traverse_init:
		for (int i = 0; i < BITMAP_MEM_DEPTH; i++){
        #pragma HLS pipeline II=1
			bitmap0[i] = 0;
			bitmap1[i] = 0;
			bitmap2[i] = 0;
			bitmap3[i] = 0;
			bitmap4[i] = 0;
			bitmap5[i] = 0;
			bitmap6[i] = 0;
			bitmap7[i] = 0;
		}

		uint3_dt  bank_idx = root_vidx.range(2, 0);
		uint18_dt offset   = root_vidx.range(20, 3);
		if(bank_idx == 0) bitmap0[offset] = 1;
		if(bank_idx == 1) bitmap1[offset] = 1;
		if(bank_idx == 2) bitmap2[offset] = 1;
		if(bank_idx == 3) bitmap3[offset] = 1;
		if(bank_idx == 4) bitmap4[offset] = 1;
		if(bank_idx == 5) bitmap5[offset] = 1;
		if(bank_idx == 6) bitmap6[offset] = 1;
		if(bank_idx == 7) bitmap7[offset] = 1;
	}

	int mem_addr0 = 0;
	int mem_addr1 = 0;
	int mem_addr2 = 0;
	int mem_addr3 = 0;
	int mem_addr4 = 0;
	int mem_addr5 = 0;
	int mem_addr6 = 0;
	int mem_addr7 = 0;
    traverse_main:
	while(cia_empty != 1 || done != 1){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		cia_empty  = cia_stream.empty();
		done_empty = cia_done_stream.empty();

		if(cia_empty != 1){
			cia_dt  word = cia_stream.read();	
			graph_dt  v0 = word.range((0  + 1) * 32 - 1, 0  * 32);
			graph_dt  v1 = word.range((1  + 1) * 32 - 1, 1  * 32);
			graph_dt  v2 = word.range((2  + 1) * 32 - 1, 2  * 32);
			graph_dt  v3 = word.range((3  + 1) * 32 - 1, 3  * 32);
			graph_dt  v4 = word.range((4  + 1) * 32 - 1, 4  * 32);
			graph_dt  v5 = word.range((5  + 1) * 32 - 1, 5  * 32);
			graph_dt  v6 = word.range((6  + 1) * 32 - 1, 6  * 32);
			graph_dt  v7 = word.range((7  + 1) * 32 - 1, 7  * 32);

		    uint18_dt d0 = v0.range(20, 3);
		    uint18_dt d1 = v1.range(20, 3);
		    uint18_dt d2 = v2.range(20, 3);
		    uint18_dt d3 = v3.range(20, 3);
		    uint18_dt d4 = v4.range(20, 3);
		    uint18_dt d5 = v5.range(20, 3);
		    uint18_dt d6 = v6.range(20, 3);
		    uint18_dt d7 = v7.range(20, 3);

			if((int)v0 != -1){
				if(bitmap0[d0] == 0){
					next_frontier_stream0 << v0;
					bitmap0[d0] = 1;
					mem_addr0++;
				}
			}

			if((int)v1 != -1){
				if(bitmap1[d1] == 0){
					next_frontier_stream1 << v1;
					bitmap1[d1] = 1;
					mem_addr1++;
				}
			}

			if((int)v2 != -1){
				if(bitmap2[d2] == 0){
					next_frontier_stream2 << v2;
					bitmap2[d2] = 1;
					mem_addr2++;
				}
			}

			if((int)v3 != -1){
				if(bitmap3[d3] == 0){
					next_frontier_stream3 << v3;
					bitmap3[d3] = 1;
					mem_addr3++;
				}
			}

			if((int)v4 != -1){
				if(bitmap4[d4] == 0){
					next_frontier_stream4 << v4;
					bitmap4[d4] = 1;
					mem_addr4++;
				}
			}

			if((int)v5 != -1){
				if(bitmap5[d5] == 0){
					next_frontier_stream5 << v5;
					bitmap5[d5] = 1;
					mem_addr5++;
				}
			}

			if((int)v6 != -1){
				if(bitmap6[d6] == 0){
					next_frontier_stream6 << v6;
					bitmap6[d6] = 1;
					mem_addr6++; 
				}
			}

			if((int)v7 != -1){
				if(bitmap7[d7] == 0){
					next_frontier_stream7 << v7;
					bitmap7[d7] = 1;
					mem_addr7++;
				}
			}

		}

		if(done_empty != 1 && cia_empty == 1){
			done = cia_done_stream.read();
		}
	}

	next_frontier_done_stream0 << 1;
	next_frontier_done_stream1 << 1;
	next_frontier_done_stream2 << 1;
	next_frontier_done_stream3 << 1;
	next_frontier_done_stream4 << 1;
	next_frontier_done_stream5 << 1;
	next_frontier_done_stream6 << 1;
	next_frontier_done_stream7 << 1;

	*(next_frontier_size  + 0)  = mem_addr0;
	*(next_frontier_size  + 1)  = mem_addr1;
	*(next_frontier_size  + 2)  = mem_addr2;
	*(next_frontier_size  + 3)  = mem_addr3;
	*(next_frontier_size  + 4)  = mem_addr4;
	*(next_frontier_size  + 5)  = mem_addr5;
	*(next_frontier_size  + 6)  = mem_addr6;
	*(next_frontier_size  + 7)  = mem_addr7;
}
 
// load depth for inspection 
static void write_frontier(
		hls::stream<graph_dt>   &next_frontier_stream,
		hls::stream<uint1_dt>   &next_frontier_done_stream,
		int                     *next_frontier
		)
{
	uint1_dt done = 0;
	uint1_dt done_empty = 0;
	uint1_dt next_frontier_empty = 0;

	int buffer[BUFFER_SIZE];
	int global_idx = 0;
	int local_idx  = 0;

    write_frontier_main:
    while(next_frontier_empty  != 1 || done  != 1)
	{
		next_frontier_empty  = next_frontier_stream.empty();
		done_empty           = next_frontier_done_stream.empty();
		
		if(next_frontier_empty != 1){
			int vidx = next_frontier_stream.read();
			buffer[local_idx++] = vidx;

			if(local_idx == BUFFER_SIZE){
                write_frontier_kernel:
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier[global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		if(next_frontier_empty == 1 && done_empty != 1){
			done = next_frontier_done_stream.read();
		}
    }

	//data left in the buffer
	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier[global_idx++] = buffer[i];
		}
		local_idx = 0;
	}
}

extern "C" {
void bfs(
		const rpa_dt    *rpa, 
		const cia_dt    *cia,

		int             *next_frontier0,
		int             *next_frontier1,
		int             *next_frontier2,
		int             *next_frontier3,
		int             *next_frontier4,
		int             *next_frontier5,
		int             *next_frontier6,
		int             *next_frontier7,
		int             *next_frontier_size,

		const int       frontier_size,
		const int       root_vidx,
		const int       pad,
		const char      level
		)
{
#pragma HLS INTERFACE m_axi port=rpa                  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=cia                  offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=next_frontier0       offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=next_frontier1       offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=next_frontier2       offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=next_frontier3       offset=slave bundle=gmem33
#pragma HLS INTERFACE m_axi port=next_frontier4       offset=slave bundle=gmem34
#pragma HLS INTERFACE m_axi port=next_frontier5       offset=slave bundle=gmem35
#pragma HLS INTERFACE m_axi port=next_frontier6       offset=slave bundle=gmem36
#pragma HLS INTERFACE m_axi port=next_frontier7       offset=slave bundle=gmem37
#pragma HLS INTERFACE m_axi port=next_frontier_size   offset=slave bundle=gmem4
#pragma HLS INTERFACE s_axilite port=frontier_size    bundle=control
#pragma HLS INTERFACE s_axilite port=root_vidx        bundle=control
#pragma HLS INTERFACE s_axilite port=pad              bundle=control
#pragma HLS INTERFACE s_axilite port=level            bundle=control
#pragma HLS INTERFACE s_axilite port=return           bundle=control

hls::stream<rpa_dt>    rpa_stream;
hls::stream<cia_dt>    cia_stream;
hls::stream<uint1_dt>  cia_done_stream;
hls::stream<graph_dt>  next_frontier_stream0;
hls::stream<graph_dt>  next_frontier_stream1;
hls::stream<graph_dt>  next_frontier_stream2;
hls::stream<graph_dt>  next_frontier_stream3;
hls::stream<graph_dt>  next_frontier_stream4;
hls::stream<graph_dt>  next_frontier_stream5;
hls::stream<graph_dt>  next_frontier_stream6;
hls::stream<graph_dt>  next_frontier_stream7;
hls::stream<uint1_dt>  next_frontier_done_stream0;
hls::stream<uint1_dt>  next_frontier_done_stream1;
hls::stream<uint1_dt>  next_frontier_done_stream2;
hls::stream<uint1_dt>  next_frontier_done_stream3;
hls::stream<uint1_dt>  next_frontier_done_stream4;
hls::stream<uint1_dt>  next_frontier_done_stream5;
hls::stream<uint1_dt>  next_frontier_done_stream6;
hls::stream<uint1_dt>  next_frontier_done_stream7;

#pragma HLS STREAM variable=rpa_stream                  depth=64
#pragma HLS STREAM variable=cia_stream                  depth=64
#pragma HLS STREAM variable=cia_done_stream             depth=4

#pragma HLS STREAM variable=next_frontier_stream0       depth=64
#pragma HLS STREAM variable=next_frontier_stream1       depth=64
#pragma HLS STREAM variable=next_frontier_stream2       depth=64
#pragma HLS STREAM variable=next_frontier_stream3       depth=64
#pragma HLS STREAM variable=next_frontier_stream4       depth=64
#pragma HLS STREAM variable=next_frontier_stream5       depth=64
#pragma HLS STREAM variable=next_frontier_stream6       depth=64
#pragma HLS STREAM variable=next_frontier_stream7       depth=64

#pragma HLS STREAM variable=next_frontier_done_stream0  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream1  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream2  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream3  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream4  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream5  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream6  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream7  depth=4

#pragma HLS dataflow
read_rpa(rpa, rpa_stream, frontier_size);
read_cia(cia, rpa_stream, cia_stream, cia_done_stream, frontier_size);
traverse_cia(
		cia_stream,
		cia_done_stream,

		next_frontier_stream0, 
		next_frontier_stream1, 
		next_frontier_stream2, 
		next_frontier_stream3, 
		next_frontier_stream4, 
		next_frontier_stream5, 
		next_frontier_stream6, 
		next_frontier_stream7, 

		next_frontier_done_stream0, 
		next_frontier_done_stream1, 
		next_frontier_done_stream2, 
		next_frontier_done_stream3, 
		next_frontier_done_stream4, 
		next_frontier_done_stream5, 
		next_frontier_done_stream6, 
		next_frontier_done_stream7, 

		next_frontier_size,
		root_vidx,
		level);

write_frontier(next_frontier_stream0,  next_frontier_done_stream0,  next_frontier0);
write_frontier(next_frontier_stream1,  next_frontier_done_stream1,  next_frontier1);
write_frontier(next_frontier_stream2,  next_frontier_done_stream2,  next_frontier2);
write_frontier(next_frontier_stream3,  next_frontier_done_stream3,  next_frontier3);
write_frontier(next_frontier_stream4,  next_frontier_done_stream4,  next_frontier4);
write_frontier(next_frontier_stream5,  next_frontier_done_stream5,  next_frontier5);
write_frontier(next_frontier_stream6,  next_frontier_done_stream6,  next_frontier6);
write_frontier(next_frontier_stream7,  next_frontier_done_stream7,  next_frontier7);

}
}



