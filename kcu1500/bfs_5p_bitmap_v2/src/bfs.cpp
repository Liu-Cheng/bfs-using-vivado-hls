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

#define BUFFER_SIZE       16
typedef ap_uint<1>  uint1_dt;
typedef ap_int<32>  graph_dt;

/**
 * The following paramters should be changed with different pad setup.
 * Also note that the total vertex number must be no less than
 * BANK_NUM * BIT_MAP_DEPTH. To accommodate Youtube and RMAT-21,
 * total_vertex_num should be no less than 2^21 = 2 million.
 */
#define BITMAP_MEM_DEPTH  (128 * 1024) 
#define BUFFER_ADDR_WIDTH 17
#define BANK_NUM          16
#define BANK_BW           4 // 4 when pad is 16; 3 when pad is 8
typedef ap_uint<4>        bank_dt;
typedef ap_uint<17>       offset_dt;
typedef ap_int<64>        rpa_dt;
typedef ap_int<512>       cia_dt;

static void read_rpa(
		const rpa_dt           *rpa,
		hls::stream<rpa_dt>    &rpa_stream,
		const int              frontier_size
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
		const cia_dt           *cia,
		hls::stream<rpa_dt>    &rpa_stream,
		hls::stream<cia_dt>    &cia_stream,
		hls::stream<uint1_dt>  &cia_done_stream,
		const int              frontier_size
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

		hls::stream<int32_dt>  &next_frontier_stream1,
		hls::stream<int32_dt>  &next_frontier_stream2,
		hls::stream<int32_dt>  &next_frontier_stream3,
		hls::stream<int32_dt>  &next_frontier_stream4,
		hls::stream<int32_dt>  &next_frontier_stream5,
		hls::stream<int32_dt>  &next_frontier_stream6,
		hls::stream<int32_dt>  &next_frontier_stream7,
		hls::stream<int32_dt>  &next_frontier_stream8,
		hls::stream<int32_dt>  &next_frontier_stream9,
		hls::stream<int32_dt>  &next_frontier_stream10,
		hls::stream<int32_dt>  &next_frontier_stream11,
		hls::stream<int32_dt>  &next_frontier_stream12,
		hls::stream<int32_dt>  &next_frontier_stream13,
		hls::stream<int32_dt>  &next_frontier_stream14,
		hls::stream<int32_dt>  &next_frontier_stream15,

		hls::stream<uint1_dt>  &next_frontier_done_stream0,
		hls::stream<uint1_dt>  &next_frontier_done_stream1,
		hls::stream<uint1_dt>  &next_frontier_done_stream2,
		hls::stream<uint1_dt>  &next_frontier_done_stream3,
		hls::stream<uint1_dt>  &next_frontier_done_stream4,
		hls::stream<uint1_dt>  &next_frontier_done_stream5,
		hls::stream<uint1_dt>  &next_frontier_done_stream6,
		hls::stream<uint1_dt>  &next_frontier_done_stream7,
		hls::stream<uint1_dt>  &next_frontier_done_stream8,
		hls::stream<uint1_dt>  &next_frontier_done_stream9,
		hls::stream<uint1_dt>  &next_frontier_done_stream10,
		hls::stream<uint1_dt>  &next_frontier_done_stream11,
		hls::stream<uint1_dt>  &next_frontier_done_stream12,
		hls::stream<uint1_dt>  &next_frontier_done_stream13,
		hls::stream<uint1_dt>  &next_frontier_done_stream14,
		hls::stream<uint1_dt>  &next_frontier_done_stream15,

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
	static uint1_dt bitmap8[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap9[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap10[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap11[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap12[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap13[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap14[BITMAP_MEM_DEPTH];
	static uint1_dt bitmap15[BITMAP_MEM_DEPTH];

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
			bitmap8[i] = 0;
			bitmap9[i] = 0;
			bitmap10[i] = 0;
			bitmap11[i] = 0;
			bitmap12[i] = 0;
			bitmap13[i] = 0;
			bitmap14[i] = 0;
			bitmap15[i] = 0;
		}

		bank_dt   bank_idx = root_vidx.range(3, 0);
		offset_dt offset   = root_vidx.range(20, 4);
		if(bank_idx == 0)  bitmap0[offset]  = 1;
		if(bank_idx == 1)  bitmap1[offset]  = 1;
		if(bank_idx == 2)  bitmap2[offset]  = 1;
		if(bank_idx == 3)  bitmap3[offset]  = 1;
		if(bank_idx == 4)  bitmap4[offset]  = 1;
		if(bank_idx == 5)  bitmap5[offset]  = 1;
		if(bank_idx == 6)  bitmap6[offset]  = 1;
		if(bank_idx == 7)  bitmap7[offset]  = 1;
		if(bank_idx == 8)  bitmap8[offset]  = 1;
		if(bank_idx == 9)  bitmap9[offset]  = 1;
		if(bank_idx == 10) bitmap10[offset] = 1;
		if(bank_idx == 11) bitmap11[offset] = 1;
		if(bank_idx == 12) bitmap12[offset] = 1;
		if(bank_idx == 13) bitmap13[offset] = 1;
		if(bank_idx == 14) bitmap14[offset] = 1;
		if(bank_idx == 15) bitmap15[offset] = 1;
	}

	int num0  = 0;
	int num1  = 0;
	int num2  = 0;
	int num3  = 0;
	int num4  = 0;
	int num5  = 0;
	int num6  = 0;
	int num7  = 0;
	int num8  = 0;
	int num9  = 0;
	int num10 = 0;
	int num11 = 0;
	int num12 = 0;
	int num13 = 0;
	int num14 = 0;
	int num15 = 0;

    traverse_main:
	while(cia_empty != 1 || done != 1){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		cia_empty  = cia_stream.empty();
		done_empty = cia_done_stream.empty();

		if(cia_empty != 1){
			cia_dt    word = cia_stream.read();	
			graph_dt  v0   = word.range((0  + 1) * 32 - 1, 0  * 32);
			graph_dt  v1   = word.range((1  + 1) * 32 - 1, 1  * 32);
			graph_dt  v2   = word.range((2  + 1) * 32 - 1, 2  * 32);
			graph_dt  v3   = word.range((3  + 1) * 32 - 1, 3  * 32);
			graph_dt  v4   = word.range((4  + 1) * 32 - 1, 4  * 32);
			graph_dt  v5   = word.range((5  + 1) * 32 - 1, 5  * 32);
			graph_dt  v6   = word.range((6  + 1) * 32 - 1, 6  * 32);
			graph_dt  v7   = word.range((7  + 1) * 32 - 1, 7  * 32);
			graph_dt  v8   = word.range((8  + 1) * 32 - 1, 8  * 32);
			graph_dt  v9   = word.range((9  + 1) * 32 - 1, 9  * 32);
			graph_dt  va   = word.range((10 + 1) * 32 - 1, 10 * 32);
			graph_dt  vb   = word.range((11 + 1) * 32 - 1, 11 * 32);
			graph_dt  vc   = word.range((12 + 1) * 32 - 1, 12 * 32);
			graph_dt  vd   = word.range((13 + 1) * 32 - 1, 13 * 32);
			graph_dt  ve   = word.range((14 + 1) * 32 - 1, 14 * 32);
			graph_dt  vf   = word.range((15 + 1) * 32 - 1, 15 * 32);

		    offset_dt d0   = v0.range(20, 4);
		    offset_dt d1   = v1.range(20, 4);
		    offset_dt d2   = v2.range(20, 4);
		    offset_dt d3   = v3.range(20, 4);
		    offset_dt d4   = v4.range(20, 4);
		    offset_dt d5   = v5.range(20, 4);
		    offset_dt d6   = v6.range(20, 4);
		    offset_dt d7   = v7.range(20, 4);
		    offset_dt d8   = v8.range(20, 4);
		    offset_dt d9   = v9.range(20, 4);
		    offset_dt da   = va.range(20, 4);
		    offset_dt db   = vb.range(20, 4);
		    offset_dt dc   = vc.range(20, 4);
		    offset_dt dd   = vd.range(20, 4);
		    offset_dt de   = ve.range(20, 4);
		    offset_dt df   = vf.range(20, 4);


			if((int)v0 != -1){
				if(bitmap0[d0] == 0){
					next_frontier_stream0 << v0;
					bitmap0[d0] = 1;
					num0++;
				}
			}

			if((int)v1 != -1){
				if(bitmap1[d1] == 0){
					next_frontier_stream1 << v1;
					bitmap1[d1] = 1;
					num1++;
				}
			}

			if((int)v2 != -1){
				if(bitmap2[d2] == 0){
					next_frontier_stream2 << v2;
					bitmap2[d2] = 1;
					num2++;
				}
			}

			if((int)v3 != -1){
				if(bitmap3[d3] == 0){
					next_frontier_stream3 << v3;
					bitmap3[d3] = 1;
					num3++;
				}
			}

			if((int)v4 != -1){
				if(bitmap4[d4] == 0){
					next_frontier_stream4 << v4;
					bitmap4[d4] = 1;
					num4++;
				}
			}

			if((int)v5 != -1){
				if(bitmap5[d5] == 0){
					next_frontier_stream5 << v5;
					bitmap5[d5] = 1;
					num5++;
				}
			}

			if((int)v6 != -1){
				if(bitmap6[d6] == 0){
					next_frontier_stream6 << v6;
					bitmap6[d6] = 1;
					num6++;
				}
			}

			if((int)v7 != -1){
				if(bitmap7[d7] == 0){
					next_frontier_stream7 << v7;
					bitmap7[d7] = 1;
					num7++;
				}
			}

			if((int)v8 != -1){
				if(bitmap8[d8] == 0){
					next_frontier_stream8 << v8;
					bitmap8[d8] = 1;
					num8++;
				}
			}

			if((int)v9 != -1){
				if(bitmap9[d9] == 0){
					next_frontier_stream9 << v9;
					bitmap9[d9] = 1;
					num9++;
				}
			}

			if((int)va != -1){
				if(bitmap10[da] == 0){
					next_frontier_stream10 << va;
					bitmap10[da] = 1;
					num10++;
				}
			}

			if((int)vb != -1){
				if(bitmap11[db] == 0){
					next_frontier_stream11 << vb;
					bitmap11[db] = 1;
					num11++;
				}
			}

			if((int)vc != -1){
				if(bitmap12[dc] == 0){
					next_frontier_stream12 << vc;
					bitmap12[dc] = 1;
					num12++;
				}
			}

			if((int)vd != -1){
				if(bitmap13[dd] == 0){
					next_frontier_stream13 << vd;
					bitmap13[dd] = 1;
					num13++;
				}
			}

			if((int)ve != -1){
				if(bitmap14[de] == 0){
					next_frontier_stream14 << ve;
					bitmap14[de] = 1;
					num14++;
				}
			}

			if((int)vf != -1){
				if(bitmap15[df] == 0){
					next_frontier_stream15 << vf;
					bitmap15[df] = 1;
					num15++;
				}
			}
		}

		if(done_empty != 1 && cia_empty == 1){
			done = cia_done_stream.read();
		}
	}

	next_frontier_done_stream0  << 1;
	next_frontier_done_stream1  << 1;
	next_frontier_done_stream2  << 1;
	next_frontier_done_stream3  << 1;
	next_frontier_done_stream4  << 1;
	next_frontier_done_stream5  << 1;
	next_frontier_done_stream6  << 1;
	next_frontier_done_stream7  << 1;
	next_frontier_done_stream8  << 1;
	next_frontier_done_stream9  << 1;
	next_frontier_done_stream10 << 1;
	next_frontier_done_stream11 << 1;
	next_frontier_done_stream12 << 1;
	next_frontier_done_stream13 << 1;
	next_frontier_done_stream14 << 1;
	next_frontier_done_stream15 << 1;

	int buffer[BANK_NUM];
	buffer[0]  = num0;  buffer[1]  = num1;  buffer[2]  = num2;  buffer[3]  = num3;
	buffer[4]  = num4;  buffer[5]  = num5;  buffer[6]  = num6;  buffer[7]  = num7;
	buffer[8]  = num8;  buffer[9]  = num9;  buffer[10] = num10; buffer[11] = num11;
	buffer[12] = num12; buffer[13] = num13; buffer[14] = num14; buffer[15] = num15;

    write_frontier_size:
	for(int i = 0; i < BANK_NUM; i++){
    #pragma HLS pipeline II=1
		*(next_frontier_size + i) = buffer[i];
	}
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
	int mem_addr  = 0;
	int local_idx = 0;

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
		int             *next_frontier8,
		int             *next_frontier9,
		int             *next_frontier10,
		int             *next_frontier11,
		int             *next_frontier12,
		int             *next_frontier13,
		int             *next_frontier14,
		int             *next_frontier15,
		int             *next_frontier_size,

		const int       frontier_size,
		const int       root_vidx,
		const int       pad,
		const char      level
		)
{
#pragma HLS INTERFACE m_axi port=rpa                  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=cia                  offset=slave bundle=gmem1

#pragma HLS INTERFACE m_axi port=next_frontier0       offset=slave bundle=gmem20
#pragma HLS INTERFACE m_axi port=next_frontier1       offset=slave bundle=gmem21
#pragma HLS INTERFACE m_axi port=next_frontier2       offset=slave bundle=gmem22
#pragma HLS INTERFACE m_axi port=next_frontier3       offset=slave bundle=gmem23
#pragma HLS INTERFACE m_axi port=next_frontier4       offset=slave bundle=gmem24
#pragma HLS INTERFACE m_axi port=next_frontier5       offset=slave bundle=gmem25
#pragma HLS INTERFACE m_axi port=next_frontier6       offset=slave bundle=gmem26
#pragma HLS INTERFACE m_axi port=next_frontier7       offset=slave bundle=gmem27
#pragma HLS INTERFACE m_axi port=next_frontier8       offset=slave bundle=gmem28
#pragma HLS INTERFACE m_axi port=next_frontier9       offset=slave bundle=gmem29
#pragma HLS INTERFACE m_axi port=next_frontier10      offset=slave bundle=gmem2a
#pragma HLS INTERFACE m_axi port=next_frontier11      offset=slave bundle=gmem2b
#pragma HLS INTERFACE m_axi port=next_frontier12      offset=slave bundle=gmem2c
#pragma HLS INTERFACE m_axi port=next_frontier13      offset=slave bundle=gmem2d
#pragma HLS INTERFACE m_axi port=next_frontier14      offset=slave bundle=gmem2e
#pragma HLS INTERFACE m_axi port=next_frontier15      offset=slave bundle=gmem2f

#pragma HLS INTERFACE m_axi port=next_frontier_size   offset=slave bundle=gmem3

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
hls::stream<graph_dt>  next_frontier_stream8;
hls::stream<graph_dt>  next_frontier_stream9;
hls::stream<graph_dt>  next_frontier_stream10;
hls::stream<graph_dt>  next_frontier_stream11;
hls::stream<graph_dt>  next_frontier_stream12;
hls::stream<graph_dt>  next_frontier_stream13;
hls::stream<graph_dt>  next_frontier_stream14;
hls::stream<graph_dt>  next_frontier_stream15;

hls::stream<uint1_dt>  next_frontier_done_stream0;
hls::stream<uint1_dt>  next_frontier_done_stream1;
hls::stream<uint1_dt>  next_frontier_done_stream2;
hls::stream<uint1_dt>  next_frontier_done_stream3;
hls::stream<uint1_dt>  next_frontier_done_stream4;
hls::stream<uint1_dt>  next_frontier_done_stream5;
hls::stream<uint1_dt>  next_frontier_done_stream6;
hls::stream<uint1_dt>  next_frontier_done_stream7;
hls::stream<uint1_dt>  next_frontier_done_stream8;
hls::stream<uint1_dt>  next_frontier_done_stream9;
hls::stream<uint1_dt>  next_frontier_done_stream10;
hls::stream<uint1_dt>  next_frontier_done_stream11;
hls::stream<uint1_dt>  next_frontier_done_stream12;
hls::stream<uint1_dt>  next_frontier_done_stream13;
hls::stream<uint1_dt>  next_frontier_done_stream14;
hls::stream<uint1_dt>  next_frontier_done_stream15;


#pragma HLS STREAM variable=rpa_stream                  depth=64
#pragma HLS STREAM variable=cia_stream                  depth=16
#pragma HLS STREAM variable=cia_done_stream             depth=4

#pragma HLS STREAM variable=next_frontier_stream0       depth=64
#pragma HLS STREAM variable=next_frontier_stream1       depth=64
#pragma HLS STREAM variable=next_frontier_stream2       depth=64
#pragma HLS STREAM variable=next_frontier_stream3       depth=64
#pragma HLS STREAM variable=next_frontier_stream4       depth=64
#pragma HLS STREAM variable=next_frontier_stream5       depth=64
#pragma HLS STREAM variable=next_frontier_stream6       depth=64
#pragma HLS STREAM variable=next_frontier_stream7       depth=64
#pragma HLS STREAM variable=next_frontier_stream8       depth=64
#pragma HLS STREAM variable=next_frontier_stream9       depth=64
#pragma HLS STREAM variable=next_frontier_stream10      depth=64
#pragma HLS STREAM variable=next_frontier_stream11      depth=64
#pragma HLS STREAM variable=next_frontier_stream12      depth=64
#pragma HLS STREAM variable=next_frontier_stream13      depth=64
#pragma HLS STREAM variable=next_frontier_stream14      depth=64
#pragma HLS STREAM variable=next_frontier_stream15      depth=64

#pragma HLS STREAM variable=next_frontier_done_stream0  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream1  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream2  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream3  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream4  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream5  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream6  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream7  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream8  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream9  depth=4
#pragma HLS STREAM variable=next_frontier_done_stream10 depth=4
#pragma HLS STREAM variable=next_frontier_done_stream11 depth=4
#pragma HLS STREAM variable=next_frontier_done_stream12 depth=4
#pragma HLS STREAM variable=next_frontier_done_stream13 depth=4
#pragma HLS STREAM variable=next_frontier_done_stream14 depth=4
#pragma HLS STREAM variable=next_frontier_done_stream15 depth=4

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
		next_frontier_stream8,
		next_frontier_stream9,
		next_frontier_stream10,
		next_frontier_stream11,
		next_frontier_stream12,
		next_frontier_stream13,
		next_frontier_stream14,
		next_frontier_stream15,

		next_frontier_done_stream0,
		next_frontier_done_stream1,
		next_frontier_done_stream2,
		next_frontier_done_stream3,
		next_frontier_done_stream4,
		next_frontier_done_stream5,
		next_frontier_done_stream6,
		next_frontier_done_stream7,
		next_frontier_done_stream8,
		next_frontier_done_stream9,
		next_frontier_done_stream10,
		next_frontier_done_stream11,
		next_frontier_done_stream12,
		next_frontier_done_stream13,
		next_frontier_done_stream14,
		next_frontier_done_stream15,

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
write_frontier(next_frontier_stream8,  next_frontier_done_stream8,  next_frontier8);
write_frontier(next_frontier_stream9,  next_frontier_done_stream9,  next_frontier9);
write_frontier(next_frontier_stream10, next_frontier_done_stream10, next_frontier10);
write_frontier(next_frontier_stream11, next_frontier_done_stream11, next_frontier11);
write_frontier(next_frontier_stream12, next_frontier_done_stream12, next_frontier12);
write_frontier(next_frontier_stream13, next_frontier_done_stream13, next_frontier13);
write_frontier(next_frontier_stream14, next_frontier_done_stream14, next_frontier14);
write_frontier(next_frontier_stream15, next_frontier_done_stream15, next_frontier15);

}
}



