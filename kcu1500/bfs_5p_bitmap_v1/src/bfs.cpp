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
#define BITMAP_MEM_DEPTH (128*1024) 
#define BUFFER_ADDR_WIDTH 17
#define BRAM_BANK_NUM 16
#define BUFFER_SIZE 16

typedef ap_uint<1>  uint1_dt;
typedef ap_uint<4>  uint4_dt;
typedef ap_uint<17> uint17_dt;

typedef ap_int<32>  int32_dt;
typedef ap_int<64>  int64_dt;
typedef ap_int<512> int512_dt;

static void read_rpa(
		const int64_dt          *rpa,
		hls::stream<int64_dt>   &rpa_stream,
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
		const int512_dt         *cia,
		hls::stream<int64_dt>   &rpa_stream,
		hls::stream<int512_dt>  &cia_stream,
		hls::stream<uint1_dt>   &cia_done_stream,
		const int               frontier_size
		)
{
	
    read_cia:
	for(int i = 0; i < frontier_size; i++){
		int64_dt word  = rpa_stream.read();
		int32_dt num   = (word.range(63, 32)) >> 4;
		int32_dt start = (word.range(31, 0))  >> 4;

		for(int j = 0; j < num; j++){
        #pragma HLS pipeline II=1
			cia_stream << cia[start + j];
		}
	}

	cia_done_stream << 1;
}

// Traverse cia 
static void traverse_cia(
		hls::stream<int512_dt> &cia_stream,
		hls::stream<uint1_dt>  &cia_done_stream,

		hls::stream<int32_dt>  &next_frontier_stream0,
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

		int32_dt               root_vidx,
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

		uint4_dt  bank_idx = root_vidx.range(3, 0);
		uint17_dt offset   = root_vidx.range(20, 4);
		if(bank_idx == 0) bitmap0[offset] = 1;
		if(bank_idx == 1) bitmap1[offset] = 1;
		if(bank_idx == 2) bitmap2[offset] = 1;
		if(bank_idx == 3) bitmap3[offset] = 1;
		if(bank_idx == 4) bitmap4[offset] = 1;
		if(bank_idx == 5) bitmap5[offset] = 1;
		if(bank_idx == 6) bitmap6[offset] = 1;
		if(bank_idx == 7) bitmap7[offset] = 1;
		if(bank_idx == 8) bitmap8[offset] = 1;
		if(bank_idx == 9) bitmap9[offset] = 1;
		if(bank_idx == 10) bitmap10[offset] = 1;
		if(bank_idx == 11) bitmap11[offset] = 1;
		if(bank_idx == 12) bitmap12[offset] = 1;
		if(bank_idx == 13) bitmap13[offset] = 1;
		if(bank_idx == 14) bitmap14[offset] = 1;
		if(bank_idx == 15) bitmap15[offset] = 1;
	}

    traverse_main:
	while(cia_empty != 1 || done != 1){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		cia_empty  = cia_stream.empty();
		done_empty = cia_done_stream.empty();

		if(cia_empty != 1){
			int512_dt word = cia_stream.read();	
			int32_dt  v0   = word.range((0  + 1) * 32 - 1, 0  * 32);
			int32_dt  v1   = word.range((1  + 1) * 32 - 1, 1  * 32);
			int32_dt  v2   = word.range((2  + 1) * 32 - 1, 2  * 32);
			int32_dt  v3   = word.range((3  + 1) * 32 - 1, 3  * 32);
			int32_dt  v4   = word.range((4  + 1) * 32 - 1, 4  * 32);
			int32_dt  v5   = word.range((5  + 1) * 32 - 1, 5  * 32);
			int32_dt  v6   = word.range((6  + 1) * 32 - 1, 6  * 32);
			int32_dt  v7   = word.range((7  + 1) * 32 - 1, 7  * 32);
			int32_dt  v8   = word.range((8  + 1) * 32 - 1, 8  * 32);
			int32_dt  v9   = word.range((9  + 1) * 32 - 1, 9  * 32);
			int32_dt  va   = word.range((10 + 1) * 32 - 1, 10 * 32);
			int32_dt  vb   = word.range((11 + 1) * 32 - 1, 11 * 32);
			int32_dt  vc   = word.range((12 + 1) * 32 - 1, 12 * 32);
			int32_dt  vd   = word.range((13 + 1) * 32 - 1, 13 * 32);
			int32_dt  ve   = word.range((14 + 1) * 32 - 1, 14 * 32);
			int32_dt  vf   = word.range((15 + 1) * 32 - 1, 15 * 32);

		    uint17_dt  d0   = v0.range(20, 4);
		    uint17_dt  d1   = v1.range(20, 4);
		    uint17_dt  d2   = v2.range(20, 4);
		    uint17_dt  d3   = v3.range(20, 4);
		    uint17_dt  d4   = v4.range(20, 4);
		    uint17_dt  d5   = v5.range(20, 4);
		    uint17_dt  d6   = v6.range(20, 4);
		    uint17_dt  d7   = v7.range(20, 4);
		    uint17_dt  d8   = v8.range(20, 4);
		    uint17_dt  d9   = v9.range(20, 4);
		    uint17_dt  da   = va.range(20, 4);
		    uint17_dt  db   = vb.range(20, 4);
		    uint17_dt  dc   = vc.range(20, 4);
		    uint17_dt  dd   = vd.range(20, 4);
		    uint17_dt  de   = ve.range(20, 4);
		    uint17_dt  df   = vf.range(20, 4);

			if((int)v0 != -1){
				if(bitmap0[d0] == 0){
					next_frontier_stream0 << v0;
					bitmap0[d0] = 1;
				}
			}

			if((int)v1 != -1){
				if(bitmap1[d1] == 0){
					next_frontier_stream1 << v1;
					bitmap1[d1] = 1;
				}
			}

			if((int)v2 != -1){
				if(bitmap2[d2] == 0){
					next_frontier_stream2 << v2;
					bitmap2[d2] = 1;
				}
			}

			if((int)v3 != -1){
				if(bitmap3[d3] == 0){
					next_frontier_stream3 << v3;
					bitmap3[d3] = 1;
				}
			}

			if((int)v4 != -1){
				if(bitmap4[d4] == 0){
					next_frontier_stream4 << v4;
					bitmap4[d4] = 1;
				}
			}

			if((int)v5 != -1){
				if(bitmap5[d5] == 0){
					next_frontier_stream5 << v5;
					bitmap5[d5] = 1;
				}
			}

			if((int)v6 != -1){
				if(bitmap6[d6] == 0){
					next_frontier_stream6 << v6;
					bitmap6[d6] = 1;
				}
			}

			if((int)v7 != -1){
				if(bitmap7[d7] == 0){
					next_frontier_stream7 << v7;
					bitmap7[d7] = 1;
				}
			}

			if((int)v8 != -1){
				if(bitmap8[d8] == 0){
					next_frontier_stream8 << v8;
					bitmap8[d8] = 1;
				}
			}

			if((int)v9 != -1){
				if(bitmap9[d9] == 0){
					next_frontier_stream9 << v9;
					bitmap9[d9] = 1;
				}
			}

			if((int)va != -1){
				if(bitmap10[da] == 0){
					next_frontier_stream10 << va;
					bitmap10[da] = 1;
				}
			}

			if((int)vb != -1){
				if(bitmap11[db] == 0){
					next_frontier_stream11 << vb;
					bitmap11[db] = 1;
				}
			}

			if((int)vc != -1){
				if(bitmap12[dc] == 0){
					next_frontier_stream12 << vc;
					bitmap12[dc] = 1;
				}
			}

			if((int)vd != -1){
				if(bitmap13[dd] == 0){
					next_frontier_stream13 << vd;
					bitmap13[dd] = 1;
				}
			}

			if((int)ve != -1){
				if(bitmap14[de] == 0){
					next_frontier_stream14 << ve;
					bitmap14[de] = 1;
				}
			}

			if((int)vf != -1){
				if(bitmap15[df] == 0){
					next_frontier_stream15 << vf;
					bitmap15[df] = 1;
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
}
 
// load depth for inspection 
static void write_frontier(
		hls::stream<int32_dt>   &next_frontier_stream0,
		hls::stream<int32_dt>   &next_frontier_stream1,
		hls::stream<int32_dt>   &next_frontier_stream2,
		hls::stream<int32_dt>   &next_frontier_stream3,
		hls::stream<int32_dt>   &next_frontier_stream4,
		hls::stream<int32_dt>   &next_frontier_stream5,
		hls::stream<int32_dt>   &next_frontier_stream6,
		hls::stream<int32_dt>   &next_frontier_stream7,
		hls::stream<int32_dt>   &next_frontier_stream8,
		hls::stream<int32_dt>   &next_frontier_stream9,
		hls::stream<int32_dt>   &next_frontier_stream10,
		hls::stream<int32_dt>   &next_frontier_stream11,
		hls::stream<int32_dt>   &next_frontier_stream12,
		hls::stream<int32_dt>   &next_frontier_stream13,
		hls::stream<int32_dt>   &next_frontier_stream14,
		hls::stream<int32_dt>   &next_frontier_stream15,

		hls::stream<uint1_dt>   &next_frontier_done_stream0,
		hls::stream<uint1_dt>   &next_frontier_done_stream1,
		hls::stream<uint1_dt>   &next_frontier_done_stream2,
		hls::stream<uint1_dt>   &next_frontier_done_stream3,
		hls::stream<uint1_dt>   &next_frontier_done_stream4,
		hls::stream<uint1_dt>   &next_frontier_done_stream5,
		hls::stream<uint1_dt>   &next_frontier_done_stream6,
		hls::stream<uint1_dt>   &next_frontier_done_stream7,
		hls::stream<uint1_dt>   &next_frontier_done_stream8,
		hls::stream<uint1_dt>   &next_frontier_done_stream9,
		hls::stream<uint1_dt>   &next_frontier_done_stream10,
		hls::stream<uint1_dt>   &next_frontier_done_stream11,
		hls::stream<uint1_dt>   &next_frontier_done_stream12,
		hls::stream<uint1_dt>   &next_frontier_done_stream13,
		hls::stream<uint1_dt>   &next_frontier_done_stream14,
		hls::stream<uint1_dt>   &next_frontier_done_stream15,

		int                     *next_frontier, 
		int                     *next_frontier_size,
		const int               seg_size
		)
{
	uint1_dt done0 = 0;
	uint1_dt done1 = 0;
	uint1_dt done2 = 0;
	uint1_dt done3 = 0;
	uint1_dt done4 = 0;
	uint1_dt done5 = 0;
	uint1_dt done6 = 0;
	uint1_dt done7 = 0;
	uint1_dt done8 = 0;
	uint1_dt done9 = 0;
	uint1_dt done10 = 0;
	uint1_dt done11 = 0;
	uint1_dt done12 = 0;
	uint1_dt done13 = 0;
	uint1_dt done14 = 0;
	uint1_dt done15 = 0;

	uint1_dt done_empty0 = 0;
	uint1_dt done_empty1 = 0;
	uint1_dt done_empty2 = 0;
	uint1_dt done_empty3 = 0;
	uint1_dt done_empty4 = 0;
	uint1_dt done_empty5 = 0;
	uint1_dt done_empty6 = 0;
	uint1_dt done_empty7 = 0;
	uint1_dt done_empty8 = 0;
	uint1_dt done_empty9 = 0;
	uint1_dt done_empty10 = 0;
	uint1_dt done_empty11 = 0;
	uint1_dt done_empty12 = 0;
	uint1_dt done_empty13 = 0;
	uint1_dt done_empty14 = 0;
	uint1_dt done_empty15 = 0;

	uint1_dt next_frontier_empty0 = 0;
	uint1_dt next_frontier_empty1 = 0;
	uint1_dt next_frontier_empty2 = 0;
	uint1_dt next_frontier_empty3 = 0;
	uint1_dt next_frontier_empty4 = 0;
	uint1_dt next_frontier_empty5 = 0;
	uint1_dt next_frontier_empty6 = 0;
	uint1_dt next_frontier_empty7 = 0;
	uint1_dt next_frontier_empty8 = 0;
	uint1_dt next_frontier_empty9 = 0;
	uint1_dt next_frontier_empty10 = 0;
	uint1_dt next_frontier_empty11 = 0;
	uint1_dt next_frontier_empty12 = 0;
	uint1_dt next_frontier_empty13 = 0;
	uint1_dt next_frontier_empty14 = 0;
	uint1_dt next_frontier_empty15 = 0;

	int global_idx0 = 0;
	int global_idx1 = 0;
	int global_idx2 = 0;
	int global_idx3 = 0;
	int global_idx4 = 0;
	int global_idx5 = 0;
	int global_idx6 = 0;
	int global_idx7 = 0;
	int global_idx8 = 0;
	int global_idx9 = 0;
	int global_idx10 = 0;
	int global_idx11 = 0;
	int global_idx12 = 0;
	int global_idx13 = 0;
	int global_idx14 = 0;
	int global_idx15 = 0;

    while(next_frontier_empty0  != 1 || done0  != 1 ||
		  next_frontier_empty1  != 1 || done1  != 1 ||
		  next_frontier_empty2  != 1 || done2  != 1 ||
		  next_frontier_empty3  != 1 || done3  != 1 ||
		  next_frontier_empty4  != 1 || done4  != 1 ||
		  next_frontier_empty5  != 1 || done5  != 1 ||
		  next_frontier_empty6  != 1 || done6  != 1 ||
		  next_frontier_empty7  != 1 || done7  != 1 ||
		  next_frontier_empty8  != 1 || done8  != 1 ||
		  next_frontier_empty9  != 1 || done9  != 1 ||
		  next_frontier_empty10 != 1 || done10 != 1 ||
		  next_frontier_empty11 != 1 || done11 != 1 ||
		  next_frontier_empty12 != 1 || done12 != 1 ||
		  next_frontier_empty13 != 1 || done13 != 1 ||
		  next_frontier_empty14 != 1 || done14 != 1 ||
		  next_frontier_empty15 != 1 || done15 != 1)
	{
		next_frontier_empty0  = next_frontier_stream0.empty();
		next_frontier_empty1  = next_frontier_stream1.empty();
		next_frontier_empty2  = next_frontier_stream2.empty();
		next_frontier_empty3  = next_frontier_stream3.empty();
		next_frontier_empty4  = next_frontier_stream4.empty();
		next_frontier_empty5  = next_frontier_stream5.empty();
		next_frontier_empty6  = next_frontier_stream6.empty();
		next_frontier_empty7  = next_frontier_stream7.empty();
		next_frontier_empty8  = next_frontier_stream8.empty();
		next_frontier_empty9  = next_frontier_stream9.empty();
		next_frontier_empty10 = next_frontier_stream10.empty();
		next_frontier_empty11 = next_frontier_stream11.empty();
		next_frontier_empty12 = next_frontier_stream12.empty();
		next_frontier_empty13 = next_frontier_stream13.empty();
		next_frontier_empty14 = next_frontier_stream14.empty();
		next_frontier_empty15 = next_frontier_stream15.empty();

		done_empty0          = next_frontier_done_stream0.empty();
		done_empty1          = next_frontier_done_stream1.empty();
		done_empty2          = next_frontier_done_stream2.empty();
		done_empty3          = next_frontier_done_stream3.empty();
		done_empty4          = next_frontier_done_stream4.empty();
		done_empty5          = next_frontier_done_stream5.empty();
		done_empty6          = next_frontier_done_stream6.empty();
		done_empty7          = next_frontier_done_stream7.empty();
		done_empty8          = next_frontier_done_stream8.empty();
		done_empty9          = next_frontier_done_stream9.empty();
		done_empty10         = next_frontier_done_stream10.empty();
		done_empty11         = next_frontier_done_stream11.empty();
		done_empty12         = next_frontier_done_stream12.empty();
		done_empty13         = next_frontier_done_stream13.empty();
		done_empty14         = next_frontier_done_stream14.empty();
		done_empty15         = next_frontier_done_stream15.empty();

		//stream0
		if(next_frontier_empty0 != 1){
			int vidx = next_frontier_stream0.read();
			next_frontier[0 * seg_size + global_idx0++] = vidx; 
		}
		//stream1
		if(next_frontier_empty1 != 1){
			int vidx = next_frontier_stream1.read();
			next_frontier[1 * seg_size + global_idx1++] = vidx;
		}
		//stream2
		if(next_frontier_empty2 != 1){
			int vidx = next_frontier_stream2.read();
			next_frontier[2 * seg_size + global_idx2++] = vidx;
		}
		//stream3
		if(next_frontier_empty3 != 1){
			int vidx = next_frontier_stream3.read();
			next_frontier[3 * seg_size + global_idx3++] = vidx;
		}
		//stream4
		if(next_frontier_empty4 != 1){
			int vidx = next_frontier_stream4.read();
			next_frontier[4 * seg_size + global_idx4++] = vidx;
		}
		//stream5
		if(next_frontier_empty5 != 1){
			int vidx = next_frontier_stream5.read();
			next_frontier[5 * seg_size + global_idx5++] = vidx;
		}
		//stream6
		if(next_frontier_empty6 != 1){
			int vidx = next_frontier_stream6.read();
			next_frontier[6 * seg_size + global_idx6++] = vidx;
		}
		//stream7
		if(next_frontier_empty7 != 1){
			int vidx = next_frontier_stream7.read();
			next_frontier[7 * seg_size + global_idx7++] = vidx;
		}
		//stream8
		if(next_frontier_empty8 != 1){
			int vidx = next_frontier_stream8.read();
			next_frontier[8 * seg_size + global_idx8++] = vidx;
		}
		//stream9
		if(next_frontier_empty9 != 1){
			int vidx = next_frontier_stream9.read();
			next_frontier[9 * seg_size + global_idx9++] = vidx;
		}
		//stream10
		if(next_frontier_empty10 != 1){
			int vidx = next_frontier_stream10.read();
			next_frontier[10 * seg_size + global_idx10++] = vidx;
		}
		//stream11
		if(next_frontier_empty11 != 1){
			int vidx = next_frontier_stream11.read();
			next_frontier[11 * seg_size + global_idx11++] = vidx;
		}
		//stream12
		if(next_frontier_empty12 != 1){
			int vidx = next_frontier_stream12.read();
			next_frontier[12 * seg_size + global_idx12++] = vidx;
		}
		//stream13
		if(next_frontier_empty13 != 1){
			int vidx = next_frontier_stream13.read();
			next_frontier[13 * seg_size + global_idx13++] = vidx;
		}
		//stream14
		if(next_frontier_empty14 != 1){
			int vidx = next_frontier_stream14.read();
			next_frontier[14 * seg_size + global_idx14++] = vidx;
		}
		//stream15
		if(next_frontier_empty15 != 1){
			int vidx = next_frontier_stream15.read();
			next_frontier[15 * seg_size + global_idx15++] = vidx;
		}
	
		//done signal
		if(next_frontier_empty0 == 1 && done_empty0 != 1 &&
		   next_frontier_empty1 == 1 && done_empty1 != 1 &&
		   next_frontier_empty2 == 1 && done_empty2 != 1 &&
		   next_frontier_empty3 == 1 && done_empty3 != 1 &&
		   next_frontier_empty4 == 1 && done_empty4 != 1 &&
		   next_frontier_empty5 == 1 && done_empty5 != 1 &&
		   next_frontier_empty6 == 1 && done_empty6 != 1 &&
		   next_frontier_empty7 == 1 && done_empty7 != 1 &&
		   next_frontier_empty8 == 1 && done_empty8 != 1 &&
		   next_frontier_empty9 == 1 && done_empty9 != 1 &&
		   next_frontier_empty10 == 1 && done_empty10 != 1 &&
		   next_frontier_empty11 == 1 && done_empty11 != 1 &&
		   next_frontier_empty12 == 1 && done_empty12 != 1 &&
		   next_frontier_empty13 == 1 && done_empty13 != 1 &&
		   next_frontier_empty14 == 1 && done_empty14 != 1 &&
		   next_frontier_empty15 == 1 && done_empty15 != 1)
		{
			done0 = next_frontier_done_stream0.read();
			done1 = next_frontier_done_stream1.read();
			done2 = next_frontier_done_stream2.read();
			done3 = next_frontier_done_stream3.read();
			done4 = next_frontier_done_stream4.read();
			done5 = next_frontier_done_stream5.read();
			done6 = next_frontier_done_stream6.read();
			done7 = next_frontier_done_stream7.read();
			done8 = next_frontier_done_stream8.read();
			done9 = next_frontier_done_stream9.read();
			done10 = next_frontier_done_stream10.read();
			done11 = next_frontier_done_stream11.read();
			done12 = next_frontier_done_stream12.read();
			done13 = next_frontier_done_stream13.read();
			done14 = next_frontier_done_stream14.read();
			done15 = next_frontier_done_stream15.read();
		}
    }
		
	*(next_frontier_size + 0)  = global_idx0;
	*(next_frontier_size + 1)  = global_idx1;
	*(next_frontier_size + 2)  = global_idx2;
	*(next_frontier_size + 3)  = global_idx3;
	*(next_frontier_size + 4)  = global_idx4;
	*(next_frontier_size + 5)  = global_idx5;
	*(next_frontier_size + 6)  = global_idx6;
	*(next_frontier_size + 7)  = global_idx7;
	*(next_frontier_size + 8)  = global_idx8;
	*(next_frontier_size + 9)  = global_idx9;
	*(next_frontier_size + 10) = global_idx10;
	*(next_frontier_size + 11) = global_idx11;
	*(next_frontier_size + 12) = global_idx12;
	*(next_frontier_size + 13) = global_idx13;
	*(next_frontier_size + 14) = global_idx14;
	*(next_frontier_size + 15) = global_idx15;
}

extern "C" {
void bfs(
		const int64_dt  *rpa, 
		const int512_dt *cia,
		int             *next_frontier,
		int             *next_frontier_size,
		const int       frontier_size,
		const int       root_vidx,
		const int       seg_size,
		const int       pad,
		const char      level
		)
{
#pragma HLS INTERFACE m_axi port=rpa                  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=cia                  offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=next_frontier        offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=next_frontier_size   offset=slave bundle=gmem4
#pragma HLS INTERFACE s_axilite port=frontier_size    bundle=control
#pragma HLS INTERFACE s_axilite port=root_vidx        bundle=control
#pragma HLS INTERFACE s_axilite port=seg_size         bundle=control
#pragma HLS INTERFACE s_axilite port=pad              bundle=control
#pragma HLS INTERFACE s_axilite port=level            bundle=control
#pragma HLS INTERFACE s_axilite port=return           bundle=control

hls::stream<int64_dt>  rpa_stream;
hls::stream<int512_dt> cia_stream;
hls::stream<uint1_dt>  cia_done_stream;
hls::stream<int32_dt>  next_frontier_stream0;
hls::stream<int32_dt>  next_frontier_stream1;
hls::stream<int32_dt>  next_frontier_stream2;
hls::stream<int32_dt>  next_frontier_stream3;
hls::stream<int32_dt>  next_frontier_stream4;
hls::stream<int32_dt>  next_frontier_stream5;
hls::stream<int32_dt>  next_frontier_stream6;
hls::stream<int32_dt>  next_frontier_stream7;
hls::stream<int32_dt>  next_frontier_stream8;
hls::stream<int32_dt>  next_frontier_stream9;
hls::stream<int32_dt>  next_frontier_stream10;
hls::stream<int32_dt>  next_frontier_stream11;
hls::stream<int32_dt>  next_frontier_stream12;
hls::stream<int32_dt>  next_frontier_stream13;
hls::stream<int32_dt>  next_frontier_stream14;
hls::stream<int32_dt>  next_frontier_stream15;
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

#pragma HLS STREAM variable=rpa_stream                  depth=32
#pragma HLS STREAM variable=cia_stream                  depth=32
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
		root_vidx,
		level);

write_frontier(next_frontier_stream0, 
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
		       next_frontier, 
			   next_frontier_size,
			   seg_size
			   );
}
}



