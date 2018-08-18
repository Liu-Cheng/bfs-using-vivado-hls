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
		const int             *frontier, 
		hls::stream<int>      &frontier_stream,
		hls::stream<uint1_dt> &frontier_done_stream,
        const int             frontier_size
		)
{
	int buffer[BUFFER_SIZE];
    read_frontier:
    for (int i = 0; i < frontier_size; i += BUFFER_SIZE){
		int len = BUFFER_SIZE;
		if(i + BUFFER_SIZE > frontier_size){
			len = frontier_size - i;
		}

		for(int j = 0; j < len; j++){
        #pragma HLS pipeline II=1
			buffer[j] = frontier[i + j];
		}

		for(int j = 0; j < len; j++){
        #pragma HLS pipeline II=1
			frontier_stream << buffer[j];
		}
	}

	frontier_done_stream << 1;
}

// Read rpa of the frontier 
static void read_csr(
		const int64_dt          *rpa,
		const int512_dt         *cia,
		hls::stream<int>        &frontier_stream,
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
			int32_dt start = (word.range(63, 32)) >> 4;
			int32_dt num   = (word.range(31, 0))  >> 4;

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

		const int              root_vidx,
		const char             level
		)
{
	int done = 0;
	int cia_empty = 0;
	int done_empty = 0;

	static uint1_dt bitmap[BRAM_BANK_NUM][BITMAP_MEM_DEPTH];
    #pragma HLS array_partition variable=bitmap cyclic factor=16 dim=1
    //#pragma HLS RESOURCE variable=bitmap core=RAM_T2P_BRAM latency=1
    //#pragma HLS DEPENDENCE variable=bitmap inter false
    //#pragma HLS DEPENDENCE variable=bitmap intra raw true

   	if(level == 0){
        traverse_init:
		for(int i = 0; i < BRAM_BANK_NUM; i++){
        #pragma HLS pipeline II=1
			for (int j = 0; j < BITMAP_MEM_DEPTH; j++){
				bitmap[i][j] = 0;
			}
		}

		uint4_dt  bank_idx = root_vidx.range(3, 0);
		uint17_dt offset   = root_vidx.range(20, 4);
		bitmap[bank_idx][offset] = 1;
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

		    int17_dt  d0   = v0.range(20, 4);
		    int17_dt  d1   = v1.range(20, 4);
		    int17_dt  d2   = v2.range(20, 4);
		    int17_dt  d3   = v3.range(20, 4);
		    int17_dt  d4   = v4.range(20, 4);
		    int17_dt  d5   = v5.range(20, 4);
		    int17_dt  d6   = v6.range(20, 4);
		    int17_dt  d7   = v7.range(20, 4);
		    int17_dt  d8   = v8.range(20, 4);
		    int17_dt  d9   = v9.range(20, 4);
		    int17_dt  da   = va.range(20, 4);
		    int17_dt  db   = vb.range(20, 4);
		    int17_dt  dc   = vc.range(20, 4);
		    int17_dt  dd   = vd.range(20, 4);
		    int17_dt  de   = ve.range(20, 4);
		    int17_dt  df   = vf.range(20, 4);

			if(v0 != -1){
				if(bitmap[0][d0] == 0){
					next_frontier_stream0 << v0;
					bitmap[0][d0] == 1;
				}
			}

			if(v1 != -1){
				if(bitmap[1][d1] == 0){
					next_frontier_stream1 << v1;
					bitmap[1][d1] == 1;
				}
			}

			if(v2 != -1){
				if(bitmap[2][d2] == 0){
					next_frontier_stream2 << v2;
					bitmap[2][d2] == 1;
				}
			}

			if(v3 != -1){
				if(bitmap[3][d3] == 0){
					next_frontier_stream3 << v3;
					bitmap[3][d3] == 1;
				}
			}

			if(v4 != -1){
				if(bitmap[4][d4] == 0){
					next_frontier_stream4 << v4;
					bitmap[4][d4] == 1;
				}
			}

			if(v5 != -1){
				if(bitmap[5][d5] == 0){
					next_frontier_stream5 << v5;
					bitmap[5][d5] == 1;
				}
			}

			if(v6 != -1){
				if(bitmap[6][d6] == 0){
					next_frontier_stream6 << v6;
					bitmap[6][d6] == 1;
				}
			}

			if(v7 != -1){
				if(bitmap[7][d7] == 0){
					next_frontier_stream7 << v7;
					bitmap[7][d7] == 1;
				}
			}

			if(v8 != -1){
				if(bitmap[8][d8] == 0){
					next_frontier_stream8 << v8;
					bitmap[8][d8] == 1;
				}
			}

			if(v9 != -1){
				if(bitmap[9][d9] == 0){
					next_frontier_stream9 << v9;
					bitmap[9][d9] == 1;
				}
			}

			if(va != -1){
				if(bitmap[10][da] == 0){
					next_frontier_stream10 << va;
					bitmap[10][da] == 1;
				}
			}

			if(vb != -1){
				if(bitmap[11][db] == 0){
					next_frontier_stream11 << vb;
					bitmap[11][db] == 1;
				}
			}

			if(vc != -1){
				if(bitmap[12][dc] == 0){
					next_frontier_stream12 << vc;
					bitmap[12][dc] == 1;
				}
			}

			if(vd != -1){
				if(bitmap[13][dd] == 0){
					next_frontier_stream13 << vd;
					bitmap[13][dd] == 1;
				}
			}

			if(ve != -1){
				if(bitmap[14][de] == 0){
					next_frontier_stream14 << ve;
					bitmap[14][de] == 1;
				}
			}

			if(vf != -1){
				if(bitmap[15][df] == 0){
					next_frontier_stream15 << vf;
					bitmap[15][df] == 1;
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
		hls::stream<int32_dt>   &next_frontier_stream,
		hls::stream<uint1_dt>   &next_frontier_done_stream,
		int                     *next_frontier, 
		int                     *next_frontier_size,
		const int               frontier_offset,
		const int               frontier_size_offset
		)
{
	uint1_dt done = 0;
	uint1_dt done_empty = 0;
	uint1_dt next_frontier_empty = 0;

	int buffer[BUFFER_SIZE];

	int global_idx = 0;
	int local_idx = 0;
    while(next_frontier_empty != 1 || done != 1)
	{
		next_frontier_empty = next_frontier_stream.empty();
		done_empty          = next_frontier_done_stream.empty();
		if(next_frontier_empty != 1){
			int vidx = next_frontier_stream.read();
			buffer[local_idx++] = vidx;

			if(local_idx == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
                #pragma HLS pipeline
					next_frontier[frontier_offset + global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		if(next_frontier_empty == 1 && done_empty != 1){
			done = next_frontier_done_stream.read();
		}
    }

	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier[frontier_offset + global_idx++] = buffer[i];
		}
		local_idx = 0;
	}

	*(next_frontier_size + frontier_size_offset) = global_idx;
}

extern "C" {
void bfs(
		const int       *frontier,
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

		const int       frontier_size,
		const int       root_vidx,
		const int       seg_size
		const char      level
		)
{
#pragma HLS INTERFACE m_axi port=frontier             offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=rpa                  offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=cia                  offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=next_frontier0       offset=slave bundle=gmem30
#pragma HLS INTERFACE m_axi port=next_frontier1       offset=slave bundle=gmem31
#pragma HLS INTERFACE m_axi port=next_frontier2       offset=slave bundle=gmem32
#pragma HLS INTERFACE m_axi port=next_frontier3       offset=slave bundle=gmem33
#pragma HLS INTERFACE m_axi port=next_frontier4       offset=slave bundle=gmem34
#pragma HLS INTERFACE m_axi port=next_frontier5       offset=slave bundle=gmem35
#pragma HLS INTERFACE m_axi port=next_frontier6       offset=slave bundle=gmem36
#pragma HLS INTERFACE m_axi port=next_frontier7       offset=slave bundle=gmem37
#pragma HLS INTERFACE m_axi port=next_frontier8       offset=slave bundle=gmem38
#pragma HLS INTERFACE m_axi port=next_frontier9       offset=slave bundle=gmem39
#pragma HLS INTERFACE m_axi port=next_frontier10      offset=slave bundle=gmem3a
#pragma HLS INTERFACE m_axi port=next_frontier11      offset=slave bundle=gmem3b
#pragma HLS INTERFACE m_axi port=next_frontier12      offset=slave bundle=gmem3c
#pragma HLS INTERFACE m_axi port=next_frontier13      offset=slave bundle=gmem3d
#pragma HLS INTERFACE m_axi port=next_frontier14      offset=slave bundle=gmem3e
#pragma HLS INTERFACE m_axi port=next_frontier15      offset=slave bundle=gmem3f
#pragma HLS INTERFACE m_axi port=next_frontier_size0  offset=slave bundle=gmem40
#pragma HLS INTERFACE m_axi port=next_frontier_size1  offset=slave bundle=gmem41
#pragma HLS INTERFACE m_axi port=next_frontier_size2  offset=slave bundle=gmem42
#pragma HLS INTERFACE m_axi port=next_frontier_size3  offset=slave bundle=gmem43
#pragma HLS INTERFACE m_axi port=next_frontier_size4  offset=slave bundle=gmem44
#pragma HLS INTERFACE m_axi port=next_frontier_size5  offset=slave bundle=gmem45
#pragma HLS INTERFACE m_axi port=next_frontier_size6  offset=slave bundle=gmem46
#pragma HLS INTERFACE m_axi port=next_frontier_size7  offset=slave bundle=gmem47
#pragma HLS INTERFACE m_axi port=next_frontier_size8  offset=slave bundle=gmem48
#pragma HLS INTERFACE m_axi port=next_frontier_size9  offset=slave bundle=gmem49
#pragma HLS INTERFACE m_axi port=next_frontier_size10 offset=slave bundle=gmem4a
#pragma HLS INTERFACE m_axi port=next_frontier_size11 offset=slave bundle=gmem4b
#pragma HLS INTERFACE m_axi port=next_frontier_size12 offset=slave bundle=gmem4c
#pragma HLS INTERFACE m_axi port=next_frontier_size13 offset=slave bundle=gmem4d
#pragma HLS INTERFACE m_axi port=next_frontier_size14 offset=slave bundle=gmem4e
#pragma HLS INTERFACE m_axi port=next_frontier_size15 offset=slave bundle=gmem4f

#pragma HLS INTERFACE s_axilite port=frontier_size    bundle=control
#pragma HLS INTERFACE s_axilite port=root_vidx        bundle=control
#pragma HLS INTERFACE s_axilite port=seg_size         bundle=control
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

#pragma HLS STREAM variable=frontier_stream             depth=16
#pragma HLS STREAM variable=frontier_done_stream        depth=4
#pragma HLS STREAM variable=rpa_stream                  depth=16
#pragma HLS STREAM variable=cia_stream                  depth=16
#pragma HLS STREAM variable=cia_done_stream             depth=4
#pragma HLS STREAM variable=next_frontier_stream0       depth=16
#pragma HLS STREAM variable=next_frontier_stream1       depth=16
#pragma HLS STREAM variable=next_frontier_stream2       depth=16
#pragma HLS STREAM variable=next_frontier_stream3       depth=16
#pragma HLS STREAM variable=next_frontier_stream4       depth=16
#pragma HLS STREAM variable=next_frontier_stream5       depth=16
#pragma HLS STREAM variable=next_frontier_stream6       depth=16
#pragma HLS STREAM variable=next_frontier_stream7       depth=16
#pragma HLS STREAM variable=next_frontier_stream8       depth=16
#pragma HLS STREAM variable=next_frontier_stream9       depth=16
#pragma HLS STREAM variable=next_frontier_stream10      depth=16
#pragma HLS STREAM variable=next_frontier_stream11      depth=16
#pragma HLS STREAM variable=next_frontier_stream12      depth=16
#pragma HLS STREAM variable=next_frontier_stream13      depth=16
#pragma HLS STREAM variable=next_frontier_stream14      depth=16
#pragma HLS STREAM variable=next_frontier_stream15      depth=16

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
read_frontier(frontier, frontier_stream, frontier_done_stream, frontier_size);
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

write_frontier(next_frontier_stream0, next_frontier_done_stream0, 
		       next_frontier0, next_frontier_size0,
			   0, 0);
write_frontier(next_frontier_stream1, next_frontier_done_stream1, 
		       next_frontier1, next_frontier_size1,
			   seg_size, 1);
write_frontier(next_frontier_stream2, next_frontier_done_stream2, 
		       next_frontier2, next_frontier_size2,
			   2 * seg_size, 2);
write_frontier(next_frontier_stream3, next_frontier_done_stream3, 
		       next_frontier3, next_frontier_size3,
			   3 * seg_size, 3);
write_frontier(next_frontier_stream4, next_frontier_done_stream4, 
		       next_frontier4, next_frontier_size4,
			   4 * seg_size, 4);
write_frontier(next_frontier_stream5, next_frontier_done_stream5, 
		       next_frontier5, next_frontier_size5,
			   5 * seg_size, 5);
write_frontier(next_frontier_stream6, next_frontier_done_stream6, 
		       next_frontier6, next_frontier_size6,
			   6 *seg_size, 6);
write_frontier(next_frontier_stream7, next_frontier_done_stream7, 
		       next_frontier7, next_frontier_size7,
			   7 * seg_size, 7);
write_frontier(next_frontier_stream8, next_frontier_done_stream8, 
		       next_frontier8, next_frontier_size8,
			   8 * seg_size, 8);
write_frontier(next_frontier_stream9, next_frontier_done_stream9, 
		       next_frontier9, next_frontier_size9,
			   9 * seg_size, 9);
write_frontier(next_frontier_stream10, next_frontier_done_stream10, 
		       next_frontier10, next_frontier_size10,
			   10 * seg_size, 10);
write_frontier(next_frontier_stream11, next_frontier_done_stream11, 
		       next_frontier11, next_frontier_size11,
			   11 * seg_size, 11);
write_frontier(next_frontier_stream12, next_frontier_done_stream12, 
		       next_frontier12, next_frontier_size12,
			   12 * seg_size, 12);
write_frontier(next_frontier_stream13, next_frontier_done_stream13, 
		       next_frontier13, next_frontier_size13,
			   13 * seg_size, 13);
write_frontier(next_frontier_stream14, next_frontier_done_stream14, 
		       next_frontier14, next_frontier_size14,
			   14 * seg_size, 14);
write_frontier(next_frontier_stream15, next_frontier_done_stream15, 
		       next_frontier15, next_frontier_size15,
			   15 * seg_size, 15);
}
}



