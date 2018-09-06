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

// read frontier 
static void read_frontier(
		int                   *frontier, 
        int                   *frontier_size,
		hls::stream<int32_dt> &frontier_stream,
		hls::stream<uint1_dt> &frontier_done_stream,
		const int             seg_size,
		const int             pad
		)
{
	int buffer[BUFFER_SIZE];
    read_frontier:
	for(int s = 0; s < pad; s++){
		int size = frontier_size[s]; 
		int base = seg_size * s;
		if(size > 0){
			for (int i = 0; i < size; i += BUFFER_SIZE){
				int len = BUFFER_SIZE;
				if(i + BUFFER_SIZE > size){
					len = size - i;
				}

				for(int j = 0; j < len; j++){
                #pragma HLS pipeline II=1
					buffer[j] = frontier[base + i + j];
				}

				for(int j = 0; j < len; j++){
                #pragma HLS pipeline II=1
					frontier_stream << buffer[j];
				}
			}
		}
	}
	frontier_done_stream << 1;

}

// Read rpa of the frontier 
static void read_csr(
		const int64_dt          *rpa,
		const int512_dt         *cia,
		hls::stream<int32_dt>   &frontier_stream,
		hls::stream<uint1_dt>   &frontier_done_stream,
		hls::stream<int512_dt>  &cia_stream,
		hls::stream<uint1_dt>   &cia_done_stream)
{
	
	uint1_dt frontier_empty = 0;
	uint1_dt done_empty = 0;
	uint1_dt done = 0;

    read_csr:
	while((frontier_empty != 1) || (done != 1)){
	#pragma HLS LOOP_FLATTEN off
    #pragma HLS pipeline
		frontier_empty = frontier_stream.empty();
	    done_empty     = frontier_done_stream.empty();

		if(frontier_empty != 1){
			int32_dt vidx  = frontier_stream.read();
			int64_dt word  = rpa[vidx];
			int32_dt num   = (word.range(63, 32)) >> 4;
			int32_dt start = (word.range(31, 0))  >> 4;

			for(int i = 0; i < num; i++){
				cia_stream << cia[start + i];
			}
		}

		if(done_empty != 1 && frontier_empty == 1){
			done = frontier_done_stream.read();
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

		int                     *next_frontier0, 
		int                     *next_frontier1, 
		int                     *next_frontier2, 
		int                     *next_frontier3, 
		int                     *next_frontier4, 
		int                     *next_frontier5, 
		int                     *next_frontier6, 
		int                     *next_frontier7, 
		int                     *next_frontier8, 
		int                     *next_frontier9, 
		int                     *next_frontier10, 
		int                     *next_frontier11, 
		int                     *next_frontier12, 
		int                     *next_frontier13, 
		int                     *next_frontier14, 
		int                     *next_frontier15, 

		int                     *next_frontier_size0,
		int                     *next_frontier_size1,
		int                     *next_frontier_size2,
		int                     *next_frontier_size3,
		int                     *next_frontier_size4,
		int                     *next_frontier_size5,
		int                     *next_frontier_size6,
		int                     *next_frontier_size7,
		int                     *next_frontier_size8,
		int                     *next_frontier_size9,
		int                     *next_frontier_size10,
		int                     *next_frontier_size11,
		int                     *next_frontier_size12,
		int                     *next_frontier_size13,
		int                     *next_frontier_size14,
		int                     *next_frontier_size15
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

	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];
	int buffer4[BUFFER_SIZE];
	int buffer5[BUFFER_SIZE];
	int buffer6[BUFFER_SIZE];
	int buffer7[BUFFER_SIZE];
	int buffer8[BUFFER_SIZE];
	int buffer9[BUFFER_SIZE];
	int buffer10[BUFFER_SIZE];
	int buffer11[BUFFER_SIZE];
	int buffer12[BUFFER_SIZE];
	int buffer13[BUFFER_SIZE];
	int buffer14[BUFFER_SIZE];
	int buffer15[BUFFER_SIZE];

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

	int local_idx0 = 0;
	int local_idx1 = 0;
	int local_idx2 = 0;
	int local_idx3 = 0;
	int local_idx4 = 0;
	int local_idx5 = 0;
	int local_idx6 = 0;
	int local_idx7 = 0;
	int local_idx8 = 0;
	int local_idx9 = 0;
	int local_idx10 = 0;
	int local_idx11 = 0;
	int local_idx12 = 0;
	int local_idx13 = 0;
	int local_idx14 = 0;
	int local_idx15 = 0;

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
			buffer0[local_idx0++] = vidx;

			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier0[global_idx0++] = buffer0[i];
				}
				local_idx0 = 0;
			}
		}
		if(next_frontier_empty0 == 1 && done_empty0 != 1){
			done0 = next_frontier_done_stream0.read();
		}

		//stream1
		if(next_frontier_empty1 != 1){
			int vidx = next_frontier_stream1.read();
			buffer1[local_idx1++] = vidx;

			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier1[global_idx1++] = buffer1[i];
				}
				local_idx1 = 0;
			}
		}
		if(next_frontier_empty1 == 1 && done_empty1 != 1){
			done1 = next_frontier_done_stream1.read();
		}

		//stream2
		if(next_frontier_empty2 != 1){
			int vidx = next_frontier_stream2.read();
			buffer2[local_idx2++] = vidx;

			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier2[global_idx2++] = buffer2[i];
				}
				local_idx2 = 0;
			}
		}
		if(next_frontier_empty2 == 1 && done_empty2 != 1){
			done2 = next_frontier_done_stream2.read();
		}

		//stream3
		if(next_frontier_empty3 != 1){
			int vidx = next_frontier_stream3.read();
			buffer3[local_idx3++] = vidx;

			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier3[global_idx3++] = buffer3[i];
				}
				local_idx3 = 0;
			}
		}
		if(next_frontier_empty3 == 1 && done_empty3 != 1){
			done3 = next_frontier_done_stream3.read();
		}

		//stream4
		if(next_frontier_empty4 != 1){
			int vidx = next_frontier_stream4.read();
			buffer4[local_idx4++] = vidx;

			if(local_idx4 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier4[global_idx4++] = buffer4[i];
				}
				local_idx4 = 0;
			}
		}
		if(next_frontier_empty4 == 1 && done_empty4 != 1){
			done4 = next_frontier_done_stream4.read();
		}

		//stream5
		if(next_frontier_empty5 != 1){
			int vidx = next_frontier_stream5.read();
			buffer5[local_idx5++] = vidx;

			if(local_idx5 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier5[global_idx5++] = buffer5[i];
				}
				local_idx5 = 0;
			}
		}
		if(next_frontier_empty5 == 1 && done_empty5 != 1){
			done5 = next_frontier_done_stream5.read();
		}

		//stream6
		if(next_frontier_empty6 != 1){
			int vidx = next_frontier_stream6.read();
			buffer6[local_idx6++] = vidx;

			if(local_idx6 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier6[global_idx6++] = buffer6[i];
				}
				local_idx6 = 0;
			}
		}
		if(next_frontier_empty6 == 1 && done_empty6 != 1){
			done6 = next_frontier_done_stream6.read();
		}

		//stream7
		if(next_frontier_empty7 != 1){
			int vidx = next_frontier_stream7.read();
			buffer7[local_idx7++] = vidx;

			if(local_idx7 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier7[global_idx7++] = buffer7[i];
				}
				local_idx7 = 0;
			}
		}
		if(next_frontier_empty7 == 1 && done_empty7 != 1){
			done7 = next_frontier_done_stream7.read();
		}

		//stream8
		if(next_frontier_empty8 != 1){
			int vidx = next_frontier_stream8.read();
			buffer8[local_idx8++] = vidx;

			if(local_idx8 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier8[global_idx8++] = buffer8[i];
				}
				local_idx8 = 0;
			}
		}
		if(next_frontier_empty8 == 1 && done_empty8 != 1){
			done8 = next_frontier_done_stream8.read();
		}

		//stream9
		if(next_frontier_empty9 != 1){
			int vidx = next_frontier_stream9.read();
			buffer9[local_idx9++] = vidx;

			if(local_idx9 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier9[global_idx9++] = buffer9[i];
				}
				local_idx9 = 0;
			}
		}
		if(next_frontier_empty9 == 1 && done_empty9 != 1){
			done9 = next_frontier_done_stream9.read();
		}

		//stream10
		if(next_frontier_empty10 != 1){
			int vidx = next_frontier_stream10.read();
			buffer10[local_idx10++] = vidx;

			if(local_idx10 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier10[global_idx10++] = buffer10[i];
				}
				local_idx10 = 0;
			}
		}
		if(next_frontier_empty10 == 1 && done_empty10 != 1){
			done10 = next_frontier_done_stream10.read();
		}

		//stream11
		if(next_frontier_empty11 != 1){
			int vidx = next_frontier_stream11.read();
			buffer11[local_idx11++] = vidx;

			if(local_idx11 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier11[global_idx11++] = buffer11[i];
				}
				local_idx11 = 0;
			}
		}
		if(next_frontier_empty11 == 1 && done_empty11 != 1){
			done11 = next_frontier_done_stream11.read();
		}

		//stream12
		if(next_frontier_empty12 != 1){
			int vidx = next_frontier_stream12.read();
			buffer12[local_idx12++] = vidx;

			if(local_idx12 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier12[global_idx12++] = buffer12[i];
				}
				local_idx12 = 0;
			}
		}
		if(next_frontier_empty12 == 1 && done_empty12 != 1){
			done12 = next_frontier_done_stream12.read();
		}

		//stream13
		if(next_frontier_empty13 != 1){
			int vidx = next_frontier_stream13.read();
			buffer13[local_idx13++] = vidx;

			if(local_idx13 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier13[global_idx13++] = buffer13[i];
				}
				local_idx13 = 0;
			}
		}
		if(next_frontier_empty13 == 1 && done_empty13 != 1){
			done13 = next_frontier_done_stream13.read();
		}

		//stream14
		if(next_frontier_empty14 != 1){
			int vidx = next_frontier_stream14.read();
			buffer14[local_idx14++] = vidx;

			if(local_idx14 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier14[global_idx14++] = buffer14[i];
				}
				local_idx14 = 0;
			}
		}
		if(next_frontier_empty14 == 1 && done_empty14 != 1){
			done14 = next_frontier_done_stream14.read();
		}

		//stream15
		if(next_frontier_empty15 != 1){
			int vidx = next_frontier_stream15.read();
			buffer15[local_idx15++] = vidx;

			if(local_idx15 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier15[global_idx15++] = buffer15[i];
				}
				local_idx15 = 0;
			}
		}
		if(next_frontier_empty15 == 1 && done_empty15 != 1){
			done15 = next_frontier_done_stream15.read();
		}
    }

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			next_frontier0[global_idx0++] = buffer0[i];
		}
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			next_frontier1[global_idx1++] = buffer1[i];
		}
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			next_frontier2[global_idx2++] = buffer2[i];
		}
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			next_frontier3[global_idx3++] = buffer3[i];
		}
		local_idx3 = 0;
	}

	if(local_idx4 > 0){
		for(int i = 0; i < local_idx4; i++){
			next_frontier4[global_idx4++] = buffer4[i];
		}
		local_idx4 = 0;
	}

	if(local_idx5 > 0){
		for(int i = 0; i < local_idx5; i++){
			next_frontier5[global_idx5++] = buffer5[i];
		}
		local_idx5 = 0;
	}

	if(local_idx6 > 0){
		for(int i = 0; i < local_idx6; i++){
			next_frontier6[global_idx6++] = buffer6[i];
		}
		local_idx6 = 0;
	}

	if(local_idx7 > 0){
		for(int i = 0; i < local_idx7; i++){
			next_frontier7[global_idx7++] = buffer7[i];
		}
		local_idx7 = 0;
	}

	if(local_idx8 > 0){
		for(int i = 0; i < local_idx8; i++){
			next_frontier8[global_idx8++] = buffer8[i];
		}
		local_idx8 = 0;
	}

	if(local_idx9 > 0){
		for(int i = 0; i < local_idx9; i++){
			next_frontier9[global_idx9++] = buffer9[i];
		}
		local_idx9 = 0;
	}

	if(local_idx10 > 0){
		for(int i = 0; i < local_idx10; i++){
			next_frontier10[global_idx10++] = buffer10[i];
		}
		local_idx10 = 0;
	}

	if(local_idx11 > 0){
		for(int i = 0; i < local_idx11; i++){
			next_frontier11[global_idx11++] = buffer11[i];
		}
		local_idx11 = 0;
	}

	if(local_idx12 > 0){
		for(int i = 0; i < local_idx12; i++){
			next_frontier12[global_idx12++] = buffer12[i];
		}
		local_idx12 = 0;
	}

	if(local_idx13 > 0){
		for(int i = 0; i < local_idx13; i++){
			next_frontier13[global_idx13++] = buffer13[i];
		}
		local_idx13 = 0;
	}

	if(local_idx14 > 0){
		for(int i = 0; i < local_idx14; i++){
			next_frontier14[global_idx14++] = buffer14[i];
		}
		local_idx14 = 0;
	}

	if(local_idx15 > 0){
		for(int i = 0; i < local_idx15; i++){
			next_frontier15[global_idx15++] = buffer15[i];
		}
		local_idx15 = 0;
	}

	*(next_frontier_size0  + 0)  = global_idx0;
	*(next_frontier_size1  + 1)  = global_idx1;
	*(next_frontier_size2  + 2)  = global_idx2;
	*(next_frontier_size3  + 3)  = global_idx3;
	*(next_frontier_size4  + 4)  = global_idx4;
	*(next_frontier_size5  + 5)  = global_idx5;
	*(next_frontier_size6  + 6)  = global_idx6;
	*(next_frontier_size7  + 7)  = global_idx7;
	*(next_frontier_size8  + 8)  = global_idx8;
	*(next_frontier_size9  + 9)  = global_idx9;
	*(next_frontier_size10 + 10) = global_idx10;
	*(next_frontier_size11 + 11) = global_idx11;
	*(next_frontier_size12 + 12) = global_idx12;
	*(next_frontier_size13 + 13) = global_idx13;
	*(next_frontier_size14 + 14) = global_idx14;
	*(next_frontier_size15 + 15) = global_idx15;
}

extern "C" {
void bfs(
		int             *frontier,
		int             *frontier_size,
		const int64_dt  *rpa, 
		const int512_dt *cia,

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

		int             *next_frontier_size0,
		int             *next_frontier_size1,
		int             *next_frontier_size2,
		int             *next_frontier_size3,
		int             *next_frontier_size4,
		int             *next_frontier_size5,
		int             *next_frontier_size6,
		int             *next_frontier_size7,
		int             *next_frontier_size8,
		int             *next_frontier_size9,
		int             *next_frontier_size10,
		int             *next_frontier_size11,
		int             *next_frontier_size12,
		int             *next_frontier_size13,
		int             *next_frontier_size14,
		int             *next_frontier_size15,

		const int       root_vidx,
		const int       seg_size,
		const int       pad,
		const char      level
		)
{
#pragma HLS INTERFACE m_axi port=frontier             offset=slave bundle=gmem00
#pragma HLS INTERFACE m_axi port=frontier_size        offset=slave bundle=gmem01
#pragma HLS INTERFACE m_axi port=rpa                  offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=cia                  offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=next_frontier0       offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=next_frontier1       offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=next_frontier2       offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=next_frontier3       offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=next_frontier4       offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=next_frontier5       offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=next_frontier6       offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=next_frontier7       offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=next_frontier8       offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=next_frontier9       offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=next_frontier10      offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=next_frontier11      offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=next_frontier12      offset=slave bundle=gmem33
#pragma HLS INTERFACE m_axi port=next_frontier13      offset=slave bundle=gmem33
#pragma HLS INTERFACE m_axi port=next_frontier14      offset=slave bundle=gmem33
#pragma HLS INTERFACE m_axi port=next_frontier15      offset=slave bundle=gmem33
#pragma HLS INTERFACE m_axi port=next_frontier_size0  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size1  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size2  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size3  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size4  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size5  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size6  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size7  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size8  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size9  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size10 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size11 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size12 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size13 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size14 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=next_frontier_size15 offset=slave bundle=gmem4

#pragma HLS INTERFACE s_axilite port=root_vidx        bundle=control
#pragma HLS INTERFACE s_axilite port=seg_size         bundle=control
#pragma HLS INTERFACE s_axilite port=pad              bundle=control
#pragma HLS INTERFACE s_axilite port=level            bundle=control
#pragma HLS INTERFACE s_axilite port=return           bundle=control

hls::stream<int32_dt>  frontier_stream;
hls::stream<uint1_dt>  frontier_done_stream;
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

#pragma HLS STREAM variable=frontier_stream             depth=128
#pragma HLS STREAM variable=frontier_done_stream        depth=4
#pragma HLS STREAM variable=rpa_stream                  depth=128
#pragma HLS STREAM variable=cia_stream                  depth=128
#pragma HLS STREAM variable=cia_done_stream             depth=4
#pragma HLS STREAM variable=next_frontier_stream0       depth=128
#pragma HLS STREAM variable=next_frontier_stream1       depth=128
#pragma HLS STREAM variable=next_frontier_stream2       depth=128
#pragma HLS STREAM variable=next_frontier_stream3       depth=128
#pragma HLS STREAM variable=next_frontier_stream4       depth=128
#pragma HLS STREAM variable=next_frontier_stream5       depth=128
#pragma HLS STREAM variable=next_frontier_stream6       depth=128
#pragma HLS STREAM variable=next_frontier_stream7       depth=128
#pragma HLS STREAM variable=next_frontier_stream8       depth=128
#pragma HLS STREAM variable=next_frontier_stream9       depth=128
#pragma HLS STREAM variable=next_frontier_stream10      depth=128
#pragma HLS STREAM variable=next_frontier_stream11      depth=128
#pragma HLS STREAM variable=next_frontier_stream12      depth=128
#pragma HLS STREAM variable=next_frontier_stream13      depth=128
#pragma HLS STREAM variable=next_frontier_stream14      depth=128
#pragma HLS STREAM variable=next_frontier_stream15      depth=128

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
read_frontier(frontier, frontier_size, frontier_stream, frontier_done_stream, seg_size, pad);
read_csr(rpa, cia, frontier_stream, frontier_done_stream, cia_stream, cia_done_stream);
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
		       next_frontier0, 
		       next_frontier1 + 1 * seg_size, 
		       next_frontier2 + 2 * seg_size, 
		       next_frontier3 + 3 * seg_size, 
		       next_frontier4 + 4 * seg_size, 
		       next_frontier5 + 5 * seg_size, 
		       next_frontier6 + 6 * seg_size, 
		       next_frontier7 + 7 * seg_size, 
		       next_frontier8 + 8 * seg_size, 
		       next_frontier9 + 9 * seg_size, 
		       next_frontier10 + 10 * seg_size, 
		       next_frontier11 + 11 * seg_size, 
		       next_frontier12 + 12 * seg_size, 
		       next_frontier13 + 13 * seg_size, 
		       next_frontier14 + 14 * seg_size, 
		       next_frontier15 + 15 * seg_size, 
			   next_frontier_size0,
			   next_frontier_size1,
			   next_frontier_size2,
			   next_frontier_size3,
			   next_frontier_size4,
			   next_frontier_size5,
			   next_frontier_size6,
			   next_frontier_size7,
			   next_frontier_size8,
			   next_frontier_size9,
			   next_frontier_size10,
			   next_frontier_size11,
			   next_frontier_size12,
			   next_frontier_size13,
			   next_frontier_size14,
			   next_frontier_size15
			   );
}
}



