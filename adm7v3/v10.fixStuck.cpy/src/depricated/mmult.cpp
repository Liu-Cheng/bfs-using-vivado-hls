/*******************************************************************************

 SDx Key Concept :

 This is a matrix multiplication example which showcases the "Systolic Array"
 based algorithm design. Systolic array type of implementation is well suited
 for FPGAs.

 *******************************************************************************/

/*

 Kernel Description :

 This kernel is a systolic array based matrix multiplication. Though the
 maximum size of the input matrices are restricted to a smaller SYSTLC_SIZE, it
 is still possible to use this approach and get better performance for larger
 matrices by using tiling.

 Arguments :

 int *a     (input )  --> Input  Matrix A
 int *b     (input )  --> Input  Matrix B
 int *c     (output)  --> Output Matrix
 int  a_row (input )  --> Row Size Matrix A
 int  a_col (input )  --> Col Size Matrix A
 int  b_col (input )  --> Col Size Matrix B

 Kernel Configuration :

 Max Size    --> 16

 Note :
 Max Size is dependent on the available DSP resources in the FPGA
 */

#include <stdio.h>
#include <assert.h>
#include <iostream>
#include "hls_stream.h"

#include "hku_defs.h"
#include "hku_types.h"

#ifdef HALF_CAST
#include "hls/utils/x_hls_utils.h"
#endif

// For 8 x 32 systolic array, A tile 8 x 128, B tile 128 x 32
// input A 1024 x 1024, B 1024 x 1024 => C 1024 x 1024
// A_TROW_TRIP = 1024 / 8 = 128
// B_TCOL_TRIP = 1024 / 32 = 32
// A_TCOL_TRIP = 1024 / 128 = 8 
const unsigned A_TROW_TRIP = (16 - 1) / A_TILE_H + 1;
const unsigned B_TCOL_TRIP = (16 - 1) / B_TILE_W + 1;
const unsigned B_TCOL_MAX_TRIP = (16 - 1) / SYSTLC_COL + 1;
const unsigned A_TCOL_TRIP = (16 - 1) / A_TILE_W + 1;
const unsigned DF_FLAT_FAC = 4;
const unsigned A_TROW_FLATTEN_TRIP = (A_TROW_TRIP - 1) / DF_FLAT_FAC + 1;

#ifdef HALF_CAST
#define halfToClHalf(x) fp_struct<half>(x).to_int()
#define clHalfToHalf(x) fp_struct<half>(x).to_ieee()
#define floatToClHalf(x) fp_struct<half>((half)(x)).to_int()
#else
#define halfToClHalf(x) (x)
#define clHalfToHalf(x) (x)
#define floatToClHalf(x) (x)
#endif

#ifdef HALF_CAST
/*
 * Converter utility that works for vivado_hls 2017.1 or above,
 * since fp_struct<half> is used
 */
template<typename T, typename HalfType>
void half_to_fixed(HalfType &in, T &out) {
	fp_struct<HalfType> in_conv = fp_struct<HalfType>(in);
	ap_ufixed<11,1> in_man = in_conv.mantissa();
	T tmp;
	if (in_conv.sign == 1) {tmp = -in_man;}
	else {tmp = in_man;}
	if (in_conv.exp < 15) {
		switch(-in_conv.expv()) {
			case 1: out = tmp >> 1; break;
			case 2: out = tmp >> 2; break;
			case 3: out = tmp >> 3; break;
			case 4: out = tmp >> 4; break;
			case 5: out = tmp >> 5; break;
			case 6: out = tmp >> 6; break;
			case 7: out = tmp >> 7; break;
			case 8: out = tmp >> 8; break;
			case 9: out = tmp >> 9; break;
			case 10: out = tmp >> 10; break;
			case 11: out = tmp >> 11; break;
			case 12: out = tmp >> 12; break;
			case 13: out = tmp >> 13; break;
			case 14: out = tmp >> 14; break;
			default: out = 0; break; // case 15 for denormal
		}
	} else {
		switch(in_conv.expv()) {
			case 1: out = tmp << 1; break;
			case 2: out = tmp << 2; break;
			case 3: out = tmp << 3; break;
			case 4: out = tmp << 4; break;
			case 5: out = tmp << 5; break;
			case 6: out = tmp << 6; break;
			case 7: out = tmp << 7; break;
			case 8: out = tmp << 8; break;
			case 9: out = tmp << 9; break;
			case 10: out = tmp << 10; break;
			case 11: out = tmp << 11; break;
			case 12: out = tmp << 12; break;
			case 13: out = tmp << 13; break;
			case 14: out = tmp << 14; break;
			case 15: out = tmp << 15; break;
			case 16: out = 65520; break; // case 16 for Inf, NaN
			default: out = tmp; break;// case 0 for 2^0
		}
	}
}
#endif

void reduce_sum(dout_t in[TREE_SIZE], dout_t &out) {
#pragma HLS PIPELINE
	dout_t adder_tree[num_ranks][TREE_SIZE];
#pragma HLS ARRAY_PARTITION variable=adder_tree dim=0 complete

	init_add_loop: for (int d = 0; d < TREE_SIZE; d++)
		adder_tree[num_ranks - 1][d] = in[d];
	unsigned rank_size = TREE_SIZE;
	add_level_loop: for (int adder_tree_rank = num_ranks - 2; adder_tree_rank >= 0; adder_tree_rank--) {
		unsigned prev_rank_is_odd = rank_size % 2;
		rank_size = (rank_size + 1) / 2;
		add_col_loop: for (int jj = 0; jj < (TREE_SIZE + 1) / 2; jj++) {
			if (jj < rank_size) {
				if (prev_rank_is_odd && jj == rank_size - 1) {
					adder_tree[adder_tree_rank][jj] = adder_tree[adder_tree_rank + 1][jj * 2];
				} else {
					adder_tree[adder_tree_rank][jj] = adder_tree[adder_tree_rank + 1][jj * 2] + adder_tree[adder_tree_rank + 1][jj * 2 + 1];
				}
			}
		}
	}
	out = adder_tree[0][0];
}

void reduce_sum_inline(accm_t in[TREE_SIZE], accm_t &out) {
#pragma HLS INLINE
	accm_t adder_tree[num_ranks][TREE_SIZE];
#pragma HLS ARRAY_PARTITION variable=adder_tree dim=0 complete

	init_add_loop: for (int d = 0; d < TREE_SIZE; d++)
		adder_tree[num_ranks - 1][d] = in[d];
	unsigned rank_size = TREE_SIZE;
	add_level_loop: for (int adder_tree_rank = num_ranks - 2; adder_tree_rank >= 0; adder_tree_rank--) {
		unsigned prev_rank_is_odd = rank_size % 2;
		rank_size = (rank_size + 1) / 2;
		add_col_loop: for (int jj = 0; jj < (TREE_SIZE + 1) / 2; jj++) {
			if (jj < rank_size) {
				if (prev_rank_is_odd && jj == rank_size - 1) {
					adder_tree[adder_tree_rank][jj] = adder_tree[adder_tree_rank + 1][jj * 2];
				} else {
					adder_tree[adder_tree_rank][jj] = adder_tree[adder_tree_rank + 1][jj * 2] + adder_tree[adder_tree_rank + 1][jj * 2 + 1];
				}
			}
		}
	}
	out = adder_tree[0][0];
}

/*
 * pipeline accum2, latency 1314, ii 5,                         DSP 36, FF 20, LUT 28
 * pipeline unroll 2 accum2, latency 675, ii 5                  DSP 36, FF 20, LUT 38, fadd8 4
 * pipeline unroll 4 accum2, latency 355, ii 5                  DSP 36, FF 21, LUT 36
 * pipeline unroll 4 accum2, inline reduce, latency 97, ii 1,   DSP 37, FF 29, LUT 53, fadd8 36
 * pipeline unroll 2 accum2, inline reduce, latency 161, ii 1,
 *  FADD_LAT 9,  clk 5.76,                                      DSP 36, FF 28, LUT 46, fadd8 18
 *      p&r clock achieved 5.096, LUT 132336, FF 219519, DSP 1325
 * pipeline accum2, inline reduce, latency 288, ii 1:
 *  FADD_LAT 9,  clk 5.76,                                      DSP 36, FF 28, LUT 32, fadd8 9
 *  FADD_LAT 10, clk 6.16,                                      DSP 36, FF 30, LUT 33, fadd8 10
 *  FADD_LAT 11, clk 5.26 (2016.1),                             DSP 36, FF 32, LUT 34, fadd8 11
 *  FADD_LAT 12, clk 5.37 (2016.1),                             DSP 36, FF 34, LUT 35, fadd8 12
 *  FADD_LAT 9,  clk 4.92 (2017.1),                             DSP 36, FF 36, LUT 36, fadd8 9
 *  FADD_LAT 10, clk 5.32 (2017.1),                             DSP 36, FF 38, LUT 37,
 *  FADD_LAT 11, clk 4.43 (2017.1),                             DSP 36, FF 40, LUT 37
 *  FADD_LAT 12, clk 4.53 (2017.1),                             DSP 36, FF 43, LUT 38
 */
/*
 void compute_default(
 const dina_t *a,   // Read-Only Matrix A
 const dinb_t *b,   // Read-Only Matrix B
 dout_t *c,     // Output Result
 unsigned a_row,    // Matrix A Row Size
 unsigned a_col, // Matrix A Col Size
 unsigned b_col) // Matrix B Col Size
 {
 unsigned b_row = a_col;
 unsigned c_row = a_row;
 unsigned c_col = b_col;

 dina_t localA[SYSTLC_ROW][SYSTLC_COL];
 #pragma HLS ARRAY_PARTITION variable=localA dim=1 complete

 dinb_t localB[SYSTLC_COL][SYSTLC_ROW];
 #pragma HLS ARRAY_PARTITION variable=localB dim=2 complete

 dout_t localC[SYSTLC_ROW][SYSTLC_COL];
 #pragma HLS ARRAY_PARTITION variable=localC dim=0 complete

 accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][TREE_SIZE];
 #pragma HLS ARRAY_PARTITION variable=sum_p dim=1 complete
 #pragma HLS ARRAY_PARTITION variable=sum_p dim=2 complete

 accm_t radd;
 #pragma HLS RESOURCE variable=radd core=FAddSub_fulldsp latency=FADD_LAT_SUB1

 readA: for (unsigned loc = 0, i = 0, j = 0; loc < a_row * a_col; loc++) {
 #pragma HLS LOOP_TRIPCOUNT min=21*21 max=21*21
 #pragma HLS PIPELINE
 localA[i][j] = a[loc];
 if (j == a_col - 1) { i++; j = 0; }
 else { j++; }
 }

 readB: for (unsigned loc = 0, i = 0, j = 0; loc < b_row * b_col; loc++) {
 #pragma HLS LOOP_TRIPCOUNT min=21*21 max=21*21
 #pragma HLS PIPELINE
 localB[i][j] = b[loc];
 if (j == b_col - 1) { i++; j = 0; }
 else { j++; }
 }

 systolic1: for (int k = 0; k < a_col; k++) {
 #pragma HLS LOOP_TRIPCOUNT min=16 max=16
 #pragma HLS PIPELINE
 systolic2: for (int i = 0; i < SYSTLC_ROW; i++) {
 systolic3: for (int j = 0; j < SYSTLC_COL; j++) {

 // Get previous sum
 //             dout_t last = (k < TREE_SIZE) ? 0 : sum_p[i][j][k % TREE_SIZE];

 // Update current sum
 // Handle boundary conditions
 //             dina_t a_val = (i < a_row && k < a_col) ? localA[i][k] : 0;
 //             dinb_t b_val = (k < b_row && j < b_col) ? localB[k][j] : 0;
 dout_t last;
 if (k < TREE_SIZE) {
 last = 0;
 } else {
 last = sum_p[i][j][k % TREE_SIZE];
 }

 dina_t a_val;
 dinb_t b_val;
 if (i < a_row && k < a_col) {
 a_val = localA[i][k];
 } else {
 a_val = 0;
 }
 if (k < b_row && j < b_col) {
 b_val = localB[k][j];
 } else {
 b_val = 0;
 }
 mult_t rmul = a_val * b_val;
 radd = last + rmul;

 // Write back results
 sum_p[i][j][k % TREE_SIZE] = radd;
 }
 }
 }

 accum1: for (int i = 0; i < SYSTLC_ROW; i++) {
 accum2: for (int j = 0; j < SYSTLC_COL; j++) {
 //pragma HLS UNROLL factor=2
 #pragma HLS PIPELINE
 accm_t reduce_out;
 reduce_sum_inline(sum_p[i][j], reduce_out);
 localC[i][j] = halfToClHalf(reduce_out);
 }
 }

 writeC: for (unsigned loc = 0, i = 0, j = 0; loc < c_row * c_col; loc++) {
 #pragma HLS LOOP_TRIPCOUNT min=256 max=256
 #pragma HLS PIPELINE
 c[loc] = localC[i][j];
 if (j == c_col - 1) { i++; j = 0; }
 else { j++; }
 }
 }*/

void read_gmem( //
		const bina_t *a, //
		const binb_t *b, //
		hls::stream<bina_t> &aFifo, //
		hls::stream<binb_t> &bFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned a_wrdPerRow = (a_col - 1) / A_VCTR_SIZE + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_wrdPerRow = (b_col - 1) / B_VCTR_SIZE + 1;
	unsigned b_wrdPerCol = (b_row - 1) / B_VCTR_SIZE + 1;

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			// assuming A_TILE_W == B_TILE_H
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				unsigned b_tRow = a_tCol;

				// if caching, only read A for first tile column of B
				if (!a_cache || b_tCol == 0) {
					aH_loop: for (unsigned i = 0; i < A_TILE_H; i++) {
						aW_loop: for (unsigned j = 0; j < A_TILE_W_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = a_tRow * A_TILE_H * a_wrdPerRow + i * a_wrdPerRow + a_tCol * A_TILE_W_WRD + j;
							DEBUG_PRINT("read A loc %d\n", loc);
							bina_t wordA = a[loc];
							aFifo.write(wordA);
						}
					}
				}
				if (!b_trans) {
					//bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) 
					bH1_loop: for (unsigned i = 0; i < B_TILE_H; i++) {
						bW1_loop: for (unsigned j = 0; j < B_TILE_W_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = b_tRow * B_TILE_H * b_wrdPerRow + i * b_wrdPerRow + b_tCol * B_TILE_W_WRD + j;
							DEBUG_PRINT("read B loc %d\n", loc);
							binb_t wordB = b[loc];
							bFifo.write(wordB);
							//if (j == B_TILE_W_WRD - 1) {
							//	j = 0;
							//	i++;
							//} else {
							//	j++;
							//}
						}
					}
				} else {
					bW2_loop: for (unsigned i = 0; i < B_TILE_W; i++) {
						bH2_loop: for (unsigned j = 0; j < B_TILE_H_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = b_tCol * B_TILE_W * b_wrdPerCol + i * b_wrdPerCol + b_tRow * B_TILE_H_WRD + j;
							DEBUG_PRINT("read B loc %d\n", loc);
							binb_t wordB = b[loc];
							bFifo.write(wordB);
						}
					}
				}
			}
		}
	}
}

void read_gmem_v7( //
		const bina_t *a, //
		const binb_t *b, //
		hls::stream<bina_t> &aFifo, //
		hls::stream<binb_t> &bFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned a_wrdPerRow = (a_col - 1) / A_VCTR_SIZE + 1;
	unsigned b_wrdPerRow = (b_col - 1) / B_VCTR_SIZE + 1;
	unsigned b_wrdPerCol = (b_row - 1) / B_VCTR_SIZE + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
			// assuming A_TILE_W == B_TILE_H
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				unsigned b_tRow = a_tCol;

				// if caching, only read A for first tile column of B
				if (!a_cache || b_tCol == 0) {
					aH_loop: for (unsigned i = 0; i < A_TILE_H; i++) {
						aW_loop: for (unsigned j = 0; j < A_TILE_W_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = a_tRow * A_TILE_H * a_wrdPerRow + i * a_wrdPerRow + a_tCol * A_TILE_W_WRD + j;
							DEBUG_PRINT("read A loc %d\n", loc);
							bina_t wordA = a[loc];
							aFifo.write(wordA);
						}
					}
				}
				if (!b_trans) {
					//bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) 
					bH1_loop: for (unsigned i = 0; i < B_TILE_H; i++) {
						bW1_loop: for (unsigned j = 0; j < B_TILE_W_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = b_tRow * B_TILE_H * b_wrdPerRow + i * b_wrdPerRow + b_tCol * B_TILE_W_WRD + j;
							DEBUG_PRINT("read B loc %d\n", loc);
							binb_t wordB = b[loc];
							bFifo.write(wordB);
							//if (j == B_TILE_W_WRD - 1) {
							//	j = 0;
							//	i++;
							//} else {
							//	j++;
							//}
						}
					}
				} else {
					bW2_loop: for (unsigned i = 0; i < SYSTLC_COL; i++) {
						bH2_loop: for (unsigned j = 0; j < B_TILE_H_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = b_tCol * SYSTLC_COL * b_wrdPerCol + i * b_wrdPerCol + b_tRow * B_TILE_H_WRD + j;
							DEBUG_PRINT("read B loc %d\n", loc);
							binb_t wordB = b[loc];
							bFifo.write(wordB);
						}
					}
				}
			}
		}
	}
}

/*
 *
 */
void write_gmem( //
		hls::stream<bout_t> &cFifo, //
		bout_t *c, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_wrdPerRow = (b_col - 1) / C_VCTR_SIZE + 1;
	bout_t wordC;

	const unsigned MAX_TRIP = C_TILE_W_WRD;

	unsigned wrdCountInRowPre;
	unsigned wrdCountInRowPost;
	unsigned w_wrd;
	unsigned wrdToClear;
	int remainWrd;

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP

		wrdCountInRowPre = 0;
		wrdCountInRowPost = C_TILE_W_WRD;
		remainWrd = b_wrdPerRow;

		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP

//			if (wrdCountInRowPost <= b_wrdPerRow)
			if (remainWrd >= C_TILE_W_WRD) {
				w_wrd = C_TILE_W_WRD;
			} else {
//				w_wrd = b_wrdPerRow - wrdCountInRowPre;
				w_wrd = remainWrd;
			}

			//cT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < C_TILE_WRD; beat++) {
			cH_loop: for (unsigned i = 0; i < C_TILE_H; i++) {
				unsigned global_i = a_tRow * A_TILE_H + i;

				if (global_i < a_row) {
//					unsigned w_wrd = (wrdCountInRowPost < b_wrdPerRow ? C_TILE_W_WRD : b_wrdPerRow - b_tCol * C_TILE_W_WRD);

					cW_loop: for (unsigned char j = 0; j < w_wrd; j++) {
#pragma HLS LOOP_TRIPCOUNT min=MAX_TRIP max=MAX_TRIP 
#pragma HLS PIPELINE
						wordC = cFifo.read();
						unsigned loc = a_tRow * C_TILE_H * b_wrdPerRow + i * b_wrdPerRow + b_tCol * C_TILE_W_WRD + j;
						c[loc] = wordC;
						DEBUG_PRINT("write C loc %d\n", loc);
					}
					// end cW_loop

					if (wrdCountInRowPost > b_wrdPerRow) {
						wrdToClear = wrdCountInRowPost - b_wrdPerRow;
						clear1_loop: for (unsigned j = 0; j < wrdToClear; j++) {
#pragma HLS LOOP_TRIPCOUNT min=MAX_TRIP max=MAX_TRIP
#pragma HLS PIPELINE
							wordC = cFifo.read();
						}
					}
				} else {
					clear2_loop: for (unsigned j = 0; j < C_TILE_W_WRD; j++) {
#pragma HLS PIPELINE
						wordC = cFifo.read();
					}
				}
			}
			// end cH_loop
			wrdCountInRowPre += C_TILE_W_WRD;
			wrdCountInRowPost += C_TILE_W_WRD;
			remainWrd -= C_TILE_W_WRD;
		}
		// end b_tc_loop
	}
}

/*
 void compute_stream_v0(
 hls::stream<bina_t> &aFifo,
 hls::stream<binb_t> &bFifo,
 hls::stream<bout_t> &cFifo,
 unsigned a_row,    // Matrix A Row Size
 unsigned a_col, // Matrix A Col Size
 unsigned b_col, // Matrix B Col Size
 bool a_cache, // caching A feasible only if localA fits whole row
 bool b_trans) // specifies B is transposed in gmem, s.t. we can burst read columns
 {
 unsigned b_row = a_col;
 unsigned a_tPerCol     = (a_row-1)/A_TILE_H + 1;
 unsigned b_tPerRow     = (b_col-1)/B_TILE_W + 1;
 unsigned a_tPerRow     = (a_col-1)/A_TILE_W + 1;

 unsigned bT_tile_h     = (b_trans ? B_TILE_W : B_TILE_H);
 //unsigned bT_tile_w_wrd   = (b_trans ? B_TILE_H_WRD : B_TILE_W_WRD);
 unsigned bT_tile_w     = (b_trans ? B_TILE_H : B_TILE_W);

 dina_t localA[A_TILE_H][A_TILE_W*A_LCL_TILES];
 // dina_t localA[A_TILE_H*A_TILE_W_WRD*A_LCL_TILES][A_VCTR_SIZE];
 #pragma HLS RESOURCE variable=localA core=RAM_2P_BRAM
 //pragma HLS ARRAY_PARTITION variable=localA complete dim=1
 #pragma HLS ARRAY_PARTITION variable=localA block factor=A_DIM1_PART dim=1
 #pragma HLS ARRAY_PARTITION variable=localA cyclic factor=A_DIM2_PART dim=2

 dinb_t localB[B_TILE_H][B_TILE_W];
 #pragma HLS RESOURCE variable=localB core=RAM_2P_BRAM
 #pragma HLS ARRAY_PARTITION variable=localB cyclic factor=B_DIM1_PART dim=1
 #pragma HLS ARRAY_PARTITION variable=localB cyclic factor=B_DIM2_PART dim=2
 //pragma HLS ARRAY_PARTITION variable=localB complete dim=2

 dout_t localC[C_TILE_H][C_TILE_W];
 #pragma HLS ARRAY_PARTITION variable=localC complete dim=0

 dout_t sum_p[SYSTLC_ROW][SYSTLC_COL][TREE_SIZE];
 #pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
 #pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2

 accm_t radd;
 #pragma HLS RESOURCE variable=radd core=FAddSub_fulldsp latency=FADD_LAT_SUB1

 bina_t wordA;
 #pragma HLS DATA_PACK variable=wordA

 binb_t wordB;
 #pragma HLS DATA_PACK variable=wordB

 a_tr_loop: for (unsigned a_tRow = 0; a_tRow <  a_tPerCol; a_tRow++) {
 #pragma HLS LOOP_TRIPCOUNT min=2 max=2
 b_tc_loop: for (unsigned b_tCol = 0; b_tCol <  b_tPerRow; b_tCol++) {
 #pragma HLS LOOP_TRIPCOUNT min=2 max=2
 a_tc_loop: for (unsigned a_tCol = 0; a_tCol <  a_tPerRow; a_tCol++) {
 #pragma HLS LOOP_TRIPCOUNT min=2 max=2
 unsigned b_tRow = a_tCol;

 if (!a_cache || b_tCol == 0) {
 aH_loop: for (unsigned i = 0; i < A_TILE_H; i++) {
 aW_loop: for (unsigned j = 0; j < A_TILE_W; j += A_VCTR_SIZE) {
 #pragma HLS PIPELINE
 wordA = aFifo.read();
 unsigned offset = (a_cache ? a_tCol * A_TILE_W : 0) + j;
 bus_to_aryA: for (unsigned d = 0; d < A_VCTR_SIZE; d++) {
 localA[i][offset + d] = wordA.d[d];
 }
 }
 }
 }
 if (!b_trans) {
 //bH_loop: for (unsigned i = 0; i < B_TILE_H; i++) {
 //bW_loop: for (unsigned j = 0; j < B_TILE_W; j += B_VCTR_SIZE) {
 bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H*B_TILE_W_WRD; beat++) {
 #pragma HLS PIPELINE
 wordB = bFifo.read();
 bus_to_aryB: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
 localB[i][j + d] = wordB.d[d];
 }
 if (j + B_VCTR_SIZE >= B_TILE_W) { j = 0; i++; }
 else { j += B_VCTR_SIZE; }
 }
 } else {
 //rdBT_row: for (unsigned i = 0; i < B_TILE_W; i++) {
 //rdBT_col: for (unsigned j = 0; j < B_TILE_H; j += B_VCTR_SIZE) {
 rdBT_flat: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_W*B_TILE_H_WRD; beat++) {
 #pragma HLS PIPELINE
 wordB = bFifo.read();
 bus_to_aryBT: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
 localB[j + d][i] = wordB.d[d];
 }
 if (j + B_VCTR_SIZE >= B_TILE_H) { j = 0; i++; }
 else { j += B_VCTR_SIZE; }
 }
 }
 //             bH_loop: for (unsigned i = 0; i < bT_tile_h; i++) {
 // pragma HLS LOOP_TRIPCOUNT min=16 max=16 // transposed
 //                 bW_loop: for (unsigned j = 0; j < bT_tile_w; j += B_VCTR_SIZE) {
 // pragma HLS LOOP_TRIPCOUNT min=2 max=2 // transposed
 // pragma HLS PIPELINE
 //                     wordB = bFifo.read();
 //                     bus_to_aryB: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
 //                         if (!b_trans)
 //                             localB[i][j + d] = wordB.d[d];
 //                         else
 //                             localB[j + d][i] = wordB.d[d];
 //                     }
 //                 }
 //             }

 unsigned a_k_offset = a_tCol * A_TILE_W;

 systolic1: for (unsigned k = 0; k < A_TILE_W && a_k_offset + k < a_col; k++) {
 #pragma HLS PIPELINE II=1
 systolic2: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
 systolic3: for (unsigned j = 0; j < SYSTLC_COL; j++) {

 unsigned global_i = a_tRow * A_TILE_H + i;
 unsigned global_k = a_k_offset + k;
 unsigned global_j = b_tCol * B_TILE_W + j;

 // Get previous sum
 dout_t last = (k < TREE_SIZE && a_tCol == 0) ? 0 : sum_p[i][j][k % TREE_SIZE];

 unsigned offset = (a_cache ? a_k_offset : 0);

 // Update current sum
 // Handle boundary conditions
 dina_t a_val = (global_i < a_row && global_k < a_col) ? localA[i][offset + k] : 0;
 dinb_t b_val = (global_k < b_row && global_j < b_col) ? localB[k][j] : 0;
 mult_t rmul = a_val * b_val;
 radd = last + rmul;

 // Write back results
 sum_p[i][j][k % TREE_SIZE] = radd;
 }
 }
 }
 } // end a_tc_loop

 accum1: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
 accum2: for (unsigned j = 0; j < SYSTLC_COL; j++) {
 //pragma HLS UNROLL factor=2
 #pragma HLS PIPELINE
 reduce_sum_inline(sum_p[i][j], localC[i][j]);
 }
 }

 cT_loop: for (unsigned i = 0, j = 0, loc = 0; loc < C_TILE_WRD; loc++) {
 #pragma HLS PIPELINE
 bout_t wordC;
 cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
 wordC.d[d] = localC[i][j + d];
 cFifo.write(wordC);
 if (j + C_VCTR_SIZE >= C_TILE_W) { j = 0; i++; }
 else { j += C_VCTR_SIZE; }
 }
 //         wrC_row: for (unsigned i = 0; i < C_TILE_H; i++) {
 //             wrC_col: for (unsigned j = 0; j < C_TILE_W; j += C_VCTR_SIZE) {
 //pragma HLS PIPELINE
 //                 bout_t wordC;
 //                 cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
 //                     wordC.d[d] = localC[i][j + d];
 //                 cFifo.write(wordC);
 //             }
 //         }
 } // end b_tc_loop
 } // end a_tr_loop
 } // end compute_stream
 */

/*
 void compute_stream_v1(
 hls::stream<bina_t> &aFifo,
 hls::stream<binb_t> &bFifo,
 hls::stream<bout_t> &cFifo,
 unsigned a_row,    // Matrix A Row Size
 unsigned a_col, // Matrix A Col Size
 unsigned b_col, // Matrix B Col Size
 bool a_cache, // caching A feasible only if localA fits whole row
 bool b_trans) // specifies B is transposed in gmem, s.t. we can burst read columns
 {
 unsigned b_row = a_col;
 unsigned a_tPerCol     = (a_row-1)/A_TILE_H + 1;
 unsigned b_tPerRow     = (b_col-1)/B_TILE_W + 1;
 unsigned a_tPerRow     = (a_col-1)/A_TILE_W + 1;

 unsigned bT_tile_h     = (b_trans ? B_TILE_W : B_TILE_H);
 unsigned bT_tile_w_wrd = (b_trans ? B_TILE_H_WRD : B_TILE_W_WRD);
 //unsigned bT_tile_w       = (b_trans ? B_TILE_H : B_TILE_W);

 dina_t localA[A_TILE_H][A_TILE_W*A_LCL_TILES];
 #pragma HLS RESOURCE variable=localA core=RAM_2P_BRAM
 //pragma HLS ARRAY_PARTITION variable=localA complete dim=1
 #pragma HLS ARRAY_PARTITION variable=localA block factor=A_DIM1_PART dim=1
 #pragma HLS ARRAY_PARTITION variable=localA cyclic factor=A_DIM2_PART dim=2

 binb_t localB[B_TILE_W][B_TILE_H_WRD];
 //pragma HLS RESOURCE variable=localB core=RAM_1P_BRAM
 //pragma HLS DATA_PACK variable=localB
 //pragma HLS ARRAY_PARTITION variable=localB complete dim=1
 //pragma HLS ARRAY_PARTITION variable=localB cyclic factor=B_PART_FAC dim=1

 dout_t localC[C_TILE_H][C_TILE_W];
 #pragma HLS ARRAY_PARTITION variable=localC complete dim=0

 dout_t sum_p[SYSTLC_ROW][SYSTLC_COL][TREE_SIZE];
 #pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
 #pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2

 accm_t radd;
 #pragma HLS RESOURCE variable=radd core=FAddSub_fulldsp latency=FADD_LAT_SUB1

 bina_t wordA;
 #pragma HLS DATA_PACK variable=wordA

 binb_t wordB;
 #pragma HLS DATA_PACK variable=wordB

 binb_t wordBdist;
 #pragma HLS ARRAY_PARTITION variable=wordBdist.d complete dim=1

 binb_t reshapeB[B_TILE_W];
 //pragma HLS ARRAY_PARTITION variable=reshapeB complete dim=1

 a_tr_loop: for (unsigned a_tRow = 0; a_tRow <  a_tPerCol; a_tRow++) {
 #pragma HLS LOOP_TRIPCOUNT min=2 max=2
 b_tc_loop: for (unsigned b_tCol = 0; b_tCol <  b_tPerRow; b_tCol++) {
 #pragma HLS LOOP_TRIPCOUNT min=2 max=2
 a_tc_loop: for (unsigned a_tCol = 0; a_tCol <  a_tPerRow; a_tCol++) {
 #pragma HLS LOOP_TRIPCOUNT min=2 max=2
 unsigned b_tRow = a_tCol;

 if (!a_cache || b_tCol == 0) {
 aH_loop: for (unsigned i = 0; i < A_TILE_H; i++) {
 aW_loop: for (unsigned j = 0; j < A_TILE_W; j += A_VCTR_SIZE) {
 #pragma HLS PIPELINE
 wordA = aFifo.read();
 unsigned offset = (a_cache ? a_tCol * A_TILE_W : 0) + j;
 bus_to_aryA: for (unsigned d = 0; d < A_VCTR_SIZE; d++) {
 localA[i][offset + d] = wordA.d[d];
 }
 }
 }
 }
 if (!b_trans) {
 //                 bH_loop: for (unsigned i = 0; i < B_TILE_H; i++) {
 //                     bW_loop: for (unsigned j = 0; j < B_TILE_W; j += B_VCTR_SIZE) {
 //                 bT_loop: for (unsigned i = 0, j = 0, loc = 0; loc < B_TILE_H*B_TILE_W_WRD; loc++) {

 bH_loop: for (unsigned i = 0; i < B_TILE_H_WRD; i++) {
 bW_loop: for (unsigned j = 0, ii = 0, loc = 0; loc < B_TILE_W_WRD*B_VCTR_SIZE; loc++) {
 #pragma HLS PIPELINE
 wordBdist = bFifo.read();
 bV_loop: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
 reshapeB[j+d].d[ii] = wordBdist.d[d];
 }
 if (j + B_VCTR_SIZE >= B_TILE_W) { j = 0; ii++; }
 else { j += B_VCTR_SIZE; }
 }
 rdB_reshape: for (unsigned j = 0; j < B_TILE_W; j++) {
 #pragma HLS UNROLL
 localB[j][i] = reshapeB[j];
 }
 }

 //localB[j*B_VCTR_SIZE + d][i / B_VCTR_SIZE].d[i % B_VCTR_SIZE] = wordB.d[d];
 } else {
 rdBT_row: for (unsigned i = 0; i < B_TILE_W; i++) {
 rdBT_col: for (unsigned j = 0; j < B_TILE_H_WRD; j++) {
 #pragma HLS PIPELINE
 wordB = bFifo.read();
 localB[i][j] = wordB;
 }
 }
 }

 unsigned a_k_offset = a_tCol * A_TILE_W;

 systolic1: for (unsigned k = 0; k < A_TILE_W; k++) {
 #pragma HLS PIPELINE II=1
 systolic2: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
 systolic3: for (unsigned j = 0; j < SYSTLC_COL; j++) {

 unsigned global_i = a_tRow * A_TILE_H + i;
 unsigned global_j = b_tCol * B_TILE_W + j;
 unsigned global_k = a_k_offset + k;

 if (global_k < a_col){
 // Get previous sum
 dout_t last = (k < TREE_SIZE && a_tCol == 0) ? 0 : sum_p[i][j][k % TREE_SIZE];

 unsigned offset = (a_cache ? a_k_offset : 0);

 wordBdist = localB[j][k / B_VCTR_SIZE];

 dinb_t rdB = wordB.d[k % B_VCTR_SIZE];

 // Update current sum
 // Handle boundary conditions
 dina_t a_val = (global_i < a_row && global_k < a_col) ? localA[i][offset + k] : 0;
 dinb_t b_val = (global_k < b_row && global_j < b_col) ? rdB : 0;
 //dinb_t b_val = (global_k < b_row && global_j < b_col) ? localB[k][j] : 0;
 mult_t rmul = a_val * b_val;
 radd = last + rmul;

 // Write back results
 sum_p[i][j][k % TREE_SIZE] = radd;
 }
 }
 }
 }
 } // end a_tc_loop

 accum1: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
 accum2: for (unsigned j = 0; j < SYSTLC_COL; j++) {
 //pragma HLS UNROLL factor=2
 #pragma HLS PIPELINE
 reduce_sum_inline(sum_p[i][j], localC[i][j]);
 }
 }

 cT_loop: for (unsigned i = 0, j = 0, loc = 0; loc < C_TILE_WRD; loc++) {
 #pragma HLS PIPELINE
 bout_t word;
 cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
 word.d[d] = localC[i][j + d];
 cFifo.write(word);
 if (j + C_VCTR_SIZE >= C_TILE_W) { j = 0; i++; }
 else { j += C_VCTR_SIZE; }
 }
 //         wrC_row: for (unsigned i = 0; i < C_TILE_H; i++) {
 //             wrC_col: for (unsigned j = 0; j < C_TILE_W; j += C_VCTR_SIZE) {
 //pragma HLS PIPELINE
 //                 bout_t word;
 //                 cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
 //                     word.d[d] = localC[i][j + d];
 //                 cFifo.write(word);
 //             }
 //         }
 } // end b_tc_loop
 } // end a_tr_loop
 } // end compute_stream
 */

/*
 *
 */
void compute_stream_v2( //
		hls::stream<bina_t> &aFifo, //
		hls::stream<binb_t> &bFifo, //
		hls::stream<bout_t> &cFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned bT_tile_h = (b_trans ? B_TILE_W : B_TILE_H);
	//unsigned bT_tile_w_wrd    = (b_trans ? B_TILE_H_WRD : B_TILE_W_WRD);
	unsigned bT_tile_w = (b_trans ? B_TILE_H : B_TILE_W);

	dina_t localA[A_TILE_H][A_TILE_W * A_LCL_TILES];
#pragma HLS ARRAY_PARTITION variable=localA complete dim=1
	dinb_t localB[B_TILE_H][B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localB complete dim=2
	dout_t localC[C_TILE_H][C_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC complete dim=0
	accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][TREE_SIZE];
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2
	accm_t radd;
#pragma HLS RESOURCE variable=radd core=FAddSub_fulldsp latency=FADD_LAT_SUB1
	bina_t wordA;
#pragma HLS ARRAY_PARTITION variable=wordA.d complete dim=1
//pragma HLS DATA_PACK variable=wordA
	bina_t pWordA[A_TILE_H];
#pragma HLS ARRAY_PARTITION variable=pWordA complete dim=0
	binb_t wordB;
#pragma HLS ARRAY_PARTITION variable=wordB.d complete dim=1
//pragma HLS DATA_PACK variable=wordB
	binb_t pWordB[B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=pWordB complete dim=0

	const unsigned A_RSHP_DPTH = A_TILE_W_WRD;
	const unsigned B_RSHP_DPTH = B_TILE_H_WRD;

	hls::stream<bina_t> reshapeA[A_TILE_H];
#pragma HLS STREAM variable=reshapeA depth=A_RSHP_DPTH dim=1
//pragma HLS DATA_PACK variable=reshapeA
	hls::stream<binb_t> reshapeB[B_TILE_W];
#pragma HLS STREAM variable=reshapeB depth=B_RSHP_DPTH dim=1
//pragma HLS DATA_PACK variable=reshapeB

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				//unsigned b_tRow = a_tCol;
				unsigned a_tCol_offset = a_tCol * A_TILE_W;
				unsigned a_tCol_offset_optn = (a_cache ? a_tCol_offset : 0);
				if (!a_cache || b_tCol == 0) {
					aH_loop: for (unsigned i = 0; i < A_TILE_H; i++) {
						aW_loop: for (unsigned j = 0; j < A_TILE_W; j += A_VCTR_SIZE) {
#pragma HLS PIPELINE
							wordA = aFifo.read();
							reshapeA[i].write(wordA);
						}
					}
					reshapeA1: for (unsigned i = 0, d = 0; i < A_TILE_W; i++) {
#pragma HLS PIPELINE
						reshapeA2: for (unsigned j = 0; j < A_TILE_H; j++) {
							if (d == 0)
								pWordA[j] = reshapeA[j].read();
							localA[j][a_tCol_offset_optn + i] = pWordA[j].d[d];
							DEBUG_PRINT("localA[%d][%d] %f\n", j, a_tCol_offset_optn + i, localA[j][a_tCol_offset_optn + i]);
						}
						if (d == A_VCTR_SIZE - 1) {
							d = 0;
						} else {
							d++;
						}
					}
//                  reshapeA1: for (unsigned i = 0; i < A_TILE_W; i += A_VCTR_SIZE) {
//                      reshapeA2: for (unsigned j = 0; j < A_TILE_H; j++) {
//pragma HLS UNROLL
//                          wordA = reshapeA[j].read();
//                          reshapeA3: for (unsigned d = 0; d < A_VCTR_SIZE; d++) {
//pragma HLS PIPELINE
//                              localA[j][a_tCol_offset_optn + i + d] = wordA.d[d];
//                          }
//                      }
//                  }
				}
				DEBUG_PRINT("---- finish read A ----\n\n");
				if (!b_trans) {
					bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
						wordB = bFifo.read();
						bV_loop: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
							localB[i][j + d] = wordB.d[d];
							DEBUG_PRINT("localB[%d][%d] %f\n", i, j + d, localB[i][j + d]);
						}
						if (j + B_VCTR_SIZE >= B_TILE_W) {
							j = 0;
							i++;
						} else {
							j += B_VCTR_SIZE;
						}
					}
				} else {
#pragma HLS DATAFLOW
					rdBT_flat: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_W * B_TILE_H_WRD; beat++) {
#pragma HLS PIPELINE
						wordB = bFifo.read();
						reshapeB[i].write(wordB);
						if (j == B_TILE_H_WRD - 1) {
							j = 0;
							i++;
						} else {
							j++;
						}
					}
					reshapeB1: for (unsigned i = 0, d = 0; i < B_TILE_H; i++) {
#pragma HLS PIPELINE
						reshapeB2: for (unsigned j = 0; j < B_TILE_W; j++) {
							if (d == 0)
								pWordB[j] = reshapeB[j].read();
							localB[i][j] = pWordB[j].d[d];
							DEBUG_PRINT("localB[%d][%d] %f\n", i, j, localB[i][j]);
						}
						if (d == B_VCTR_SIZE - 1) {
							d = 0;
						} else {
							d++;
						}
					}
				}
//              DEBUG_PRINT("---- finish read B ----\n\n");
//              bH_loop: for (unsigned i = 0; i < bT_tile_h; i++) {
//  pragma HLS LOOP_TRIPCOUNT min=16 max=16 // transposed
//                  bW_loop: for (unsigned j = 0; j < bT_tile_w; j += B_VCTR_SIZE) {
//  pragma HLS LOOP_TRIPCOUNT min=2 max=2 // transposed
//  pragma HLS PIPELINE
//                      wordB = bFifo.read();
//                      bus_to_aryB: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
//                          if (!b_trans)
//                              localB[i][j + d] = wordB.d[d];
//                          else
//                              localB[j + d][i] = wordB.d[d];
//                      }
//                  }
//              }
				systolic1: for (unsigned k = 0; k < A_TILE_W; k++) {
#pragma HLS PIPELINE II=1
					systolic2: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
						systolic3: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							unsigned global_i = a_tRow * A_TILE_H + i;
							unsigned global_k = a_tCol_offset + k;
							unsigned global_j = b_tCol * B_TILE_W + j;
							// Get previous sum
//                          dout_t last = (k < TREE_SIZE && a_tCol == 0) ? 0 : sum_p[i][j][k % TREE_SIZE];
							//unsigned offset = (a_cache ? a_tCol_offset : 0);
							// Update current sum
							// Handle boundary conditions
//                          dina_t a_val = (global_i < a_row && global_k < a_col) ? localA[i][a_tCol_offset_optn + k] : 0;
//                          dinb_t b_val = (global_k < b_row && global_j < b_col) ? localB[k][j] : 0;
							dout_t last;
							if (k < TREE_SIZE && a_tCol == 0) {
								last = 0;
							} else {
								last = sum_p[i][j][k % TREE_SIZE];
							}
							dina_t a_val;
							dinb_t b_val;
							if (global_i < a_row && global_k < a_col) {
								a_val = localA[i][a_tCol_offset_optn + k];
							} else {
								a_val = 0;
							}
							if (global_k < b_row && global_j < b_col) {
								b_val = localB[k][j];
							} else {
								b_val = 0;
							}
							mult_t rmul = a_val * b_val;
							radd = last + rmul;
							// Write back results
							if (a_tCol_offset + k < a_col)
								sum_p[i][j][k % TREE_SIZE] = radd;
						}
					}
				}
			} // end a_tc_loop
			accum1: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
				accum2: for (unsigned j = 0; j < SYSTLC_COL; j++) {
//pragma HLS UNROLL factor=2
#pragma HLS PIPELINE
					accm_t reduce_out;
					reduce_sum_inline(sum_p[i][j], reduce_out);
					localC[i][j] = halfToClHalf(reduce_out);
//                  DEBUG_PRINT("localC[%d][%d] %f\n", i, j, localC[i][j]);
				}
			}
			DEBUG_PRINT("---- finish calculate local C ----\n\n");
			cT_loop: for (unsigned i = 0, j = 0, loc = 0; loc < C_TILE_WRD; loc++) {
#pragma HLS PIPELINE
				bout_t word;
				cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
					word.d[d] = localC[i][j + d];
				cFifo.write(word);
				if (j + C_VCTR_SIZE >= C_TILE_W) {
					j = 0;
					i++;
				} else {
					j += C_VCTR_SIZE;
				}
			}
		} // end b_tc_loop
	} // end a_tr_loop
} // end compute_stream

/*
 * Compute core that process buckets of fifos, count equals local RAM channels which equals systolic rows/cols
 */
void compute_stream_v3( //
		hls::stream<bina_t> reshapeA[A_TILE_H], //
		hls::stream<binb_t> reshapeB[B_TILE_W], //
		hls::stream<bout_t> &cFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;

	dina_t localA[A_TILE_H][A_TILE_W * A_LCL_TILES];
#pragma HLS ARRAY_PARTITION variable=localA complete dim=1
	dinb_t localB[B_TILE_H][B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localB complete dim=2
	dout_t localC[C_TILE_H][C_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC complete dim=0
	accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][TREE_SIZE];
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2
	bina_t wordA;
#pragma HLS ARRAY_PARTITION variable=wordA.d complete dim=1
	bina_t pWordA[A_TILE_H];
#pragma HLS ARRAY_PARTITION variable=pWordA complete dim=0
	binb_t wordB;
#pragma HLS ARRAY_PARTITION variable=wordB.d complete dim=1
	binb_t pWordB[B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=pWordB complete dim=0

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				//unsigned b_tRow = a_tCol;
				unsigned a_tCol_offset = a_tCol * A_TILE_W;
				unsigned a_tCol_offset_optn = (a_cache ? a_tCol_offset : 0);
				if (!a_cache || b_tCol == 0) {
					reshapeA1: for (unsigned i = 0, d = 0; i < A_TILE_W; i++) {
#pragma HLS PIPELINE
						reshapeA2: for (unsigned j = 0; j < A_TILE_H; j++) {
							if (d == 0)
								pWordA[j] = reshapeA[j].read();
							localA[j][a_tCol_offset_optn + i] = pWordA[j].d[d];
							DBG_cout << "localA[" << j << "][" << a_tCol_offset_optn + i << "] " << localA[j][a_tCol_offset_optn + i] << std::endl;
						}
						if (d == A_VCTR_SIZE - 1) {
							d = 0;
						} else {
							d++;
						}
					}
				}
				DEBUG_PRINT("---- finish read A ----\n\n");
				if (!b_trans) {
					bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
						//wordB = bFifo.read();
						wordB = reshapeB[0].read();
						bus_to_aryB: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
							localB[i][j + d] = wordB.d[d];
							DBG_cout << "localB[" << i << "][" << j + d << "] " << localB[i][j + d] << std::endl;
						}
						if (j + B_VCTR_SIZE >= B_TILE_W) {
							j = 0;
							i++;
						} else {
							j += B_VCTR_SIZE;
						}
					}
				} else {
					reshapeB1: for (unsigned i = 0, d = 0; i < B_TILE_H; i++) {
#pragma HLS PIPELINE
						reshapeB2: for (unsigned j = 0; j < B_TILE_W; j++) {
							if (d == 0)
								pWordB[j] = reshapeB[j].read();
							localB[i][j] = pWordB[j].d[d];
							DBG_cout << "localB[" << i << "][" << j << "] " << localB[i][j] << std::endl;
						}
						if (d == B_VCTR_SIZE - 1) {
							d = 0;
						} else {
							d++;
						}
					}
				}
				DEBUG_PRINT("---- finish read B ----\n\n");
				systolic1: for (unsigned k = 0; k < A_TILE_W; k++) {
#pragma HLS PIPELINE II=1
					systolic2: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
						systolic3: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							unsigned global_i = a_tRow * A_TILE_H + i;
							unsigned global_k = a_tCol_offset + k;
							unsigned global_j = b_tCol * B_TILE_W + j;
							accm_t last;
							if (k < TREE_SIZE && a_tCol == 0) {
								last = 0;
							} else {
								last = sum_p[i][j][k % TREE_SIZE];
							}

							dina_t a_val;
							dinb_t b_val;
							if (global_i < a_row && global_k < a_col) {
								a_val = localA[i][a_tCol_offset_optn + k];
							} else {
								a_val = 0;
							}
							if (global_k < b_row && global_j < b_col) {
								b_val = localB[k][j];
							} else {
								b_val = 0;
							}

							mult_t rmul = clHalfToHalf(a_val) * clHalfToHalf(b_val);
							accm_t radd;
//pragma HLS RESOURCE variable=radd core=HAddSub_nodsp latency=FADD_LAT_SUB1
//pragma HLS RESOURCE variable=radd core=FAddSub_fulldsp latency=FADD_LAT_SUB1
							radd = last + rmul;
							if (a_tCol_offset + k < a_col)
								sum_p[i][j][k % TREE_SIZE] = radd;
						}
					}
				}
			} // end a_tc_loop
			accum1: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
				accum2: for (unsigned j = 0; j < SYSTLC_COL; j++) {
//pragma HLS UNROLL factor=2
#pragma HLS PIPELINE
					accm_t reduce_out;
					reduce_sum_inline(sum_p[i][j], reduce_out);
					localC[i][j] = halfToClHalf(reduce_out);
					DBG_cout << "localC[" << i << "][" << j << "] " << localC[i][j] << std::endl;
				}
			}
			DEBUG_PRINT("---- finish calculate local C ----\n\n");
			cT_loop: for (unsigned i = 0, j = 0, loc = 0; loc < C_TILE_WRD; loc++) {
#pragma HLS PIPELINE
				bout_t word;
				cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
					word.d[d] = localC[i][j + d];
				cFifo.write(word);
				if (j + C_VCTR_SIZE >= C_TILE_W) {
					j = 0;
					i++;
				} else {
					j += C_VCTR_SIZE;
				}
			}
		} // end b_tc_loop
	} // end a_tr_loop
} // end compute_stream

/*
 *
 */
void compute_dataflow_v3_1( //
		hls::stream<bina_t> reshapeA[A_TILE_H], //
		hls::stream<binb_t> reshapeB[B_TILE_W], //
		accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][TREE_SIZE], //
		unsigned a_tRow, //
		unsigned b_tCol, //
		unsigned a_tCol, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
#pragma HLS INLINE

	dina_t localA[A_TILE_H][A_TILE_W * A_LCL_TILES];
#pragma HLS ARRAY_PARTITION variable=localA complete dim=1
	dinb_t localB[B_TILE_H][B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localB complete dim=2
	binb_t wordB;
#pragma HLS ARRAY_PARTITION variable=wordB.d complete dim=1
	bina_t pWordA[A_TILE_H];
#pragma HLS ARRAY_PARTITION variable=pWordA complete dim=0
	binb_t pWordB[B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=pWordB complete dim=0

	unsigned b_row = a_col;
	//unsigned b_tRow = a_tCol;
	unsigned a_tCol_offset = a_tCol * A_TILE_W;
	unsigned a_tCol_offset_optn = (a_cache ? a_tCol_offset : 0);
	assert(B_TILE_W_WRD == 1 && "Combined rdB and reshapeB loops only work when B_TILE_W_WRD == 1");

	reshapeA1: for (unsigned i = 0, d = 0; i < A_TILE_W; i++) {
#pragma HLS PIPELINE
		if (!a_cache || b_tCol == 0) {
			reshapeA2: for (unsigned j = 0; j < A_TILE_H; j++) {
				if (d == 0)
					pWordA[j] = reshapeA[j].read();
				localA[j][a_tCol_offset_optn + i] = pWordA[j].d[d];
				DBG_cout << "localA[" << j << "][" << a_tCol_offset_optn + i << "] " << localA[j][a_tCol_offset_optn + i] << std::endl;
			}
			if (d == A_VCTR_SIZE - 1) {
				d = 0;
			} else {
				d++;
			}
		}
	}
	DEBUG_PRINT("---- finish read A ----\n\n");
	rdB_combined: for (unsigned i = 0, dd = 0; i < B_TILE_H; i++) {
#pragma HLS PIPELINE
		if (!b_trans) {
			wordB = reshapeB[0].read();
			bus_to_aryB: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
				localB[i][d] = wordB.d[d];
				DBG_cout << "localB[" << i << "][" << d << "] " << localB[i][d] << std::endl;
			}
		} else {
			reshapeB2: for (unsigned j = 0; j < B_TILE_W; j++) {
				if (dd == 0)
					pWordB[j] = reshapeB[j].read();
				localB[i][j] = pWordB[j].d[dd];
				DBG_cout << "localB[" << i << "][" << j << "] " << localB[i][j] << std::endl;
			}
			if (dd == B_VCTR_SIZE - 1) {
				dd = 0;
			} else {
				dd++;
			}
		}
	}
	DEBUG_PRINT("---- finish read B ----\n\n");
	systolic1: for (unsigned k = 0; k < A_TILE_W; k++) {
#pragma HLS PIPELINE II=1
		systolic2: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
			systolic3: for (unsigned j = 0; j < SYSTLC_COL; j++) {
				unsigned global_i = a_tRow * A_TILE_H + i;
				unsigned global_k = a_tCol_offset + k;
				unsigned global_j = b_tCol * B_TILE_W + j;
				accm_t last;
				if (k < TREE_SIZE && a_tCol == 0) {
					last = 0;
				} else {
					last = sum_p[i][j][k % TREE_SIZE];
				}

				dina_t a_val;
				dinb_t b_val;
				if (global_i < a_row && global_k < a_col) {
					a_val = localA[i][a_tCol_offset_optn + k];
				} else {
					a_val = 0;
				}
				if (global_k < b_row && global_j < b_col) {
					b_val = localB[k][j];
				} else {
					b_val = 0;
				}

				mult_t rmul = clHalfToHalf(a_val) * clHalfToHalf(b_val);
				accm_t radd;
//pragma HLS RESOURCE variable=radd core=HAddSub_nodsp latency=FADD_LAT_SUB1
//pragma HLS RESOURCE variable=radd core=FAddSub_fulldsp latency=FADD_LAT_SUB1
				radd = last + rmul;
				if (a_tCol_offset + k < a_col)
					sum_p[i][j][k % TREE_SIZE] = radd;
			}
		}
	}
}

/*
 * Compute core that process buckets of fifos, count equals local RAM channels which equals systolic rows/cols
 * This variant uses dataflow for each tile B RAM write and systolic compute
 */
void compute_stream_v3_1( //
		hls::stream<bina_t> reshapeA[A_TILE_H], //
		hls::stream<binb_t> reshapeB[B_TILE_W], //
		hls::stream<bout_t> &cFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;

	dout_t localC[C_TILE_H][C_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC complete dim=0
	accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][TREE_SIZE];
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
#pragma HLS DATAFLOW
				//unsigned b_tRow = a_tCol;
				//unsigned a_tCol_offset = a_tCol * A_TILE_W;
				//unsigned a_tCol_offset_optn = (a_cache ? a_tCol_offset : 0);
				compute_dataflow_v3_1(reshapeA, reshapeB, sum_p, a_tRow, b_tCol, a_tCol, a_row, a_col, b_col, a_cache, b_trans);
			} // end a_tc_loop
			accum1: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
				accum2: for (unsigned j = 0; j < SYSTLC_COL; j++) {
//pragma HLS UNROLL factor=2
#pragma HLS PIPELINE
					accm_t reduce_out;
					reduce_sum_inline(sum_p[i][j], reduce_out);
					localC[i][j] = halfToClHalf(reduce_out);
					DBG_cout << "localC[" << i << "][" << j << "] " << localC[i][j] << std::endl;
				}
			}
			DEBUG_PRINT("---- finish calculate local C ----\n\n");
			cT_loop: for (unsigned i = 0, j = 0, loc = 0; loc < C_TILE_WRD; loc++) {
#pragma HLS PIPELINE
				bout_t word;
				cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
					word.d[d] = localC[i][j + d];
				cFifo.write(word);
				if (j + C_VCTR_SIZE >= C_TILE_W) {
					j = 0;
					i++;
				} else {
					j += C_VCTR_SIZE;
				}
			}
		} // end b_tc_loop
	} // end a_tr_loop
} // end compute_stream

/*
 * Compute core that process from buckets of fifos, count equals to burst beats
 */
void compute_stream_v4( //
		hls::stream<bina_t> reshapeA[A_TILE_W_WRD], //
		hls::stream<binb_t> reshapeB[B_TILE_H_WRD], //
		hls::stream<bout_t> &cFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;

	dina_t localA[A_TILE_H][A_TILE_W * A_LCL_TILES];
#pragma HLS ARRAY_PARTITION variable=localA complete dim=1
	dinb_t localB[B_TILE_H][B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localB complete dim=2
	dout_t localC[C_TILE_H][C_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC complete dim=0
	accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][TREE_SIZE];
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2
	bina_t wordA;
#pragma HLS ARRAY_PARTITION variable=wordA.d complete dim=1
	bina_t pWordA[A_TILE_H];
#pragma HLS ARRAY_PARTITION variable=pWordA complete dim=0
	binb_t wordB;
#pragma HLS ARRAY_PARTITION variable=wordB.d complete dim=1
	binb_t pWordB[B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=pWordB complete dim=0

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				//unsigned b_tRow = a_tCol;
				unsigned a_tCol_offset = a_tCol * A_TILE_W;
				unsigned a_tCol_offset_optn = (a_cache ? a_tCol_offset : 0);
				if (!a_cache || b_tCol == 0) {
					reshapeA1: for (unsigned beat = 0; beat < A_TILE_W_WRD; beat++) {
//pragma HLS DATAFLOW
						reshapeA2: for (unsigned j = 0; j < A_TILE_H; j++) {
#pragma HLS PIPELINE
							pWordA[j] = reshapeA[beat].read();
						}
						reshapeA3: for (unsigned d = 0; d < A_VCTR_SIZE; d++) {
#pragma HLS PIPELINE
							reshapeA4: for (unsigned j = 0; j < A_TILE_H; j++) {
								localA[j][a_tCol_offset_optn + beat * A_VCTR_SIZE + d] = pWordA[j].d[d];
							}
						}
					}
				}
				DEBUG_PRINT("---- finish read A ----\n\n");
				if (!b_trans) {
					bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
						//wordB = bFifo.read();
						wordB = reshapeB[0].read();
						bus_to_aryB: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
							localB[i][j + d] = wordB.d[d];
							DBG_cout << "localB[" << i << "][" << j + d << "] " << localB[i][j + d] << std::endl;
						}
						if (j + B_VCTR_SIZE >= B_TILE_W) {
							j = 0;
							i++;
						} else {
							j += B_VCTR_SIZE;
						}
					}
				} else {
					reshapeB1: for (unsigned beat = 0; beat < B_TILE_H_WRD; beat++) {
//pragma HLS DATAFLOW
						reshapeB2: for (unsigned j = 0; j < B_TILE_W; j++) {
#pragma HLS PIPELINE
							pWordB[j] = reshapeB[beat].read();
						}
						reshapeB3: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
#pragma HLS PIPELINE
							reshapeB4: for (unsigned j = 0; j < B_TILE_W; j++) {
								localB[beat * A_VCTR_SIZE + d][j] = pWordB[j].d[d];
							}
						}
					}
				}
				DEBUG_PRINT("---- finish read B ----\n\n");
				systolic1: for (unsigned k = 0; k < A_TILE_W; k++) {
#pragma HLS PIPELINE II=1
					systolic2: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
						systolic3: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							unsigned global_i = a_tRow * A_TILE_H + i;
							unsigned global_k = a_tCol_offset + k;
							unsigned global_j = b_tCol * B_TILE_W + j;
							accm_t last;
							if (k < TREE_SIZE && a_tCol == 0) {
								last = 0;
							} else {
								last = sum_p[i][j][k % TREE_SIZE];
							}

							dina_t a_val;
							dinb_t b_val;
							if (global_i < a_row && global_k < a_col) {
								a_val = localA[i][a_tCol_offset_optn + k];
							} else {
								a_val = 0;
							}
							if (global_k < b_row && global_j < b_col) {
								b_val = localB[k][j];
							} else {
								b_val = 0;
							}

							mult_t rmul = clHalfToHalf(a_val) * clHalfToHalf(b_val);
							accm_t radd;
//pragma HLS RESOURCE variable=radd core=HAddSub_nodsp latency=FADD_LAT_SUB1
//pragma HLS RESOURCE variable=radd core=FAddSub_fulldsp latency=FADD_LAT_SUB1
							radd = last + rmul;
							if (a_tCol_offset + k < a_col)
								sum_p[i][j][k % TREE_SIZE] = radd;
						}
					}
				}
			} // end a_tc_loop
			accum1: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
				accum2: for (unsigned j = 0; j < SYSTLC_COL; j++) {
//pragma HLS UNROLL factor=2
#pragma HLS PIPELINE
					accm_t reduce_out;
					reduce_sum_inline(sum_p[i][j], reduce_out);
					localC[i][j] = halfToClHalf(reduce_out);
					DBG_cout << "localC[" << i << "][" << j << "] " << localC[i][j] << std::endl;
				}
			}
			DEBUG_PRINT("---- finish calculate local C ----\n\n");
			cT_loop: for (unsigned i = 0, j = 0, loc = 0; loc < C_TILE_WRD; loc++) {
#pragma HLS PIPELINE
				bout_t word;
				cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
					word.d[d] = localC[i][j + d];
				cFifo.write(word);
				if (j + C_VCTR_SIZE >= C_TILE_W) {
					j = 0;
					i++;
				} else {
					j += C_VCTR_SIZE;
				}
			}
		} // end b_tc_loop
	} // end a_tr_loop
} // end compute_stream

/*
 * Compute core that process from buckets of fifos, count equals to burst beats
 * Combines reshapeB2 and reshapeB3 loop
 */
void compute_stream_v4_0_1( //
		hls::stream<bina_t> reshapeA[A_TILE_W_WRD], //
		hls::stream<binb_t> reshapeB[B_TILE_H_WRD], //
		hls::stream<bout_t> &cFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;

	dina_t localA[A_TILE_H][A_TILE_W * A_LCL_TILES];
#pragma HLS ARRAY_PARTITION variable=localA complete dim=1
	dinb_t localB[B_TILE_H][B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localB complete dim=2
	dout_t localC[C_TILE_H][C_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC complete dim=0
	accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][TREE_SIZE];
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2
	bina_t wordA;
#pragma HLS ARRAY_PARTITION variable=wordA.d complete dim=1
	bina_t pWordA[A_TILE_H];
#pragma HLS ARRAY_PARTITION variable=pWordA complete dim=0
	binb_t wordB;
#pragma HLS ARRAY_PARTITION variable=wordB.d complete dim=1
	binb_t pWordB[B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=pWordB complete dim=0

	assert(B_TILE_W == B_VCTR_SIZE && "Combining reshapeB loops requires B_TILE_W == B_VCTR_SIZE");

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				//unsigned b_tRow = a_tCol;
				unsigned a_tCol_offset = a_tCol * A_TILE_W;
				unsigned a_tCol_offset_optn = (a_cache ? a_tCol_offset : 0);
				if (!a_cache || b_tCol == 0) {
					reshapeA1: for (unsigned beat = 0; beat < A_TILE_W_WRD; beat++) {
//pragma HLS DATAFLOW
						reshapeA2: for (unsigned j = 0; j < A_TILE_H; j++) {
#pragma HLS PIPELINE
							pWordA[j] = reshapeA[beat].read();
						}
						reshapeA3: for (unsigned d = 0; d < A_VCTR_SIZE; d++) {
#pragma HLS PIPELINE
							reshapeA4: for (unsigned j = 0; j < A_TILE_H; j++) {
								localA[j][a_tCol_offset_optn + beat * A_VCTR_SIZE + d] = pWordA[j].d[d];
							}
						}
					}
				}
				DEBUG_PRINT("---- finish read A ----\n\n");
				if (!b_trans) {
					bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
						//wordB = bFifo.read();
						wordB = reshapeB[0].read();
						bus_to_aryB: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
							localB[i][j + d] = wordB.d[d];
							DBG_cout << "localB[" << i << "][" << j + d << "] " << localB[i][j + d] << std::endl;
						}
						if (j + B_VCTR_SIZE >= B_TILE_W) {
							j = 0;
							i++;
						} else {
							j += B_VCTR_SIZE;
						}
					}
				} else {
					reshapeB1: for (unsigned beat = 0; beat < B_TILE_H_WRD; beat++) {
#pragma HLS DATAFLOW
						reshapeB2: for (unsigned j = 0; j < B_TILE_W; j++) {
#pragma HLS PIPELINE
							pWordB[j] = reshapeB[beat].read();
						}
						reshapeB3: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
#pragma HLS PIPELINE
							reshapeB4: for (unsigned j = 0; j < B_TILE_W; j++) {
								localB[beat * A_VCTR_SIZE + d][j] = pWordB[j].d[d];
							}
						}
					}
				}
				DEBUG_PRINT("---- finish read B ----\n\n");
				systolic1: for (unsigned k = 0; k < A_TILE_W; k++) {
#pragma HLS PIPELINE II=1
					systolic2: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
						systolic3: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							unsigned global_i = a_tRow * A_TILE_H + i;
							unsigned global_k = a_tCol_offset + k;
							unsigned global_j = b_tCol * B_TILE_W + j;
							accm_t last;
							if (k < TREE_SIZE && a_tCol == 0) {
								last = 0;
							} else {
								last = sum_p[i][j][k % TREE_SIZE];
							}

							dina_t a_val;
							dinb_t b_val;
							if (global_i < a_row && global_k < a_col) {
								a_val = localA[i][a_tCol_offset_optn + k];
							} else {
								a_val = 0;
							}
							if (global_k < b_row && global_j < b_col) {
								b_val = localB[k][j];
							} else {
								b_val = 0;
							}

							mult_t rmul = clHalfToHalf(a_val) * clHalfToHalf(b_val);
							accm_t radd;
//pragma HLS RESOURCE variable=radd core=HAddSub_nodsp latency=FADD_LAT_SUB1
//pragma HLS RESOURCE variable=radd core=FAddSub_fulldsp latency=FADD_LAT_SUB1
							radd = last + rmul;
							if (a_tCol_offset + k < a_col)
								sum_p[i][j][k % TREE_SIZE] = radd;
						}
					}
				}
			} // end a_tc_loop
			accum1: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
				accum2: for (unsigned j = 0; j < SYSTLC_COL; j++) {
//pragma HLS UNROLL factor=2
#pragma HLS PIPELINE
					accm_t reduce_out;
					reduce_sum_inline(sum_p[i][j], reduce_out);
					localC[i][j] = halfToClHalf(reduce_out);
					DBG_cout << "localC[" << i << "][" << j << "] " << localC[i][j] << std::endl;
				}
			}
			DEBUG_PRINT("---- finish calculate local C ----\n\n");
			cT_loop: for (unsigned i = 0, j = 0, loc = 0; loc < C_TILE_WRD; loc++) {
#pragma HLS PIPELINE
				bout_t word;
				cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
					word.d[d] = localC[i][j + d];
				cFifo.write(word);
				if (j + C_VCTR_SIZE >= C_TILE_W) {
					j = 0;
					i++;
				} else {
					j += C_VCTR_SIZE;
				}
			}
		} // end b_tc_loop
	} // end a_tr_loop
} // end compute_stream

/*
 * Compute core for fixed-point or integer accumulation, hence partial sum "sum_p" need not be 3d array
 */
template<typename AccmType>
void compute_stream_v4_1( //
		hls::stream<bina_t> reshapeA[A_TILE_W_WRD], //
		hls::stream<binb_t> reshapeB[B_TILE_H_WRD], //
		hls::stream<bout_t> &cFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;

	dina_t localA[A_TILE_H][A_TILE_W * A_LCL_TILES];
#pragma HLS ARRAY_PARTITION variable=localA complete dim=1
	dinb_t localB[B_TILE_H][B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localB complete dim=2
	dout_t localC[C_TILE_H][C_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC complete dim=0
	AccmType sum_p[SYSTLC_ROW][SYSTLC_COL];
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2
	bina_t wordA;
#pragma HLS ARRAY_PARTITION variable=wordA.d complete dim=1
	bina_t pWordA[A_TILE_H];
#pragma HLS ARRAY_PARTITION variable=pWordA complete dim=0
	binb_t wordB;
#pragma HLS ARRAY_PARTITION variable=wordB.d complete dim=1
	binb_t pWordB[B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=pWordB complete dim=0

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				//unsigned b_tRow = a_tCol;
				unsigned a_tCol_offset = a_tCol * A_TILE_W;
				unsigned a_tCol_offset_optn = (a_cache ? a_tCol_offset : 0);
				if (!a_cache || b_tCol == 0) {
					reshapeA1: for (unsigned beat = 0; beat < A_TILE_W_WRD; beat++) {
//pragma HLS DATAFLOW
						reshapeA2: for (unsigned j = 0; j < A_TILE_H; j++) {
#pragma HLS PIPELINE
							pWordA[j] = reshapeA[beat].read();
						}
						reshapeA3: for (unsigned d = 0; d < A_VCTR_SIZE; d++) {
#pragma HLS PIPELINE
							reshapeA4: for (unsigned j = 0; j < A_TILE_H; j++) {
								localA[j][a_tCol_offset_optn + beat * A_VCTR_SIZE + d] = pWordA[j].d[d];
//                              DBG_cout << "localA[" << j << "][" << a_tCol_offset_optn + beat*A_VCTR_SIZE + d << "] " << pWordA[j].d[d] << std::endl;
							}
						}
					}
				}
				DEBUG_PRINT("---- finish read A ----\n\n");
				if (!b_trans) {
					bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
						//wordB = bFifo.read();
						wordB = reshapeB[0].read();
						bus_to_aryB: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
							localB[i][j + d] = wordB.d[d];
//                          DBG_cout << "localB[" << i << "][" << j+d << "] " << localB[i][j+d] << std::endl;
						}
						if (j + B_VCTR_SIZE >= B_TILE_W) {
							j = 0;
							i++;
						} else {
							j += B_VCTR_SIZE;
						}
					}
				} else {
					reshapeB1: for (unsigned beat = 0; beat < B_TILE_H_WRD; beat++) {
//pragma HLS DATAFLOW
						reshapeB2: for (unsigned j = 0; j < B_TILE_W; j++) {
#pragma HLS PIPELINE
							pWordB[j] = reshapeB[beat].read();
						}
						reshapeB3: for (unsigned d = 0; d < B_VCTR_SIZE; d++) {
#pragma HLS PIPELINE
							reshapeB4: for (unsigned j = 0; j < B_TILE_W; j++) {
								localB[beat * A_VCTR_SIZE + d][j] = pWordB[j].d[d];
							}
						}
					}
				}
				DEBUG_PRINT("---- finish read B ----\n\n");
				systolic1: for (unsigned k = 0; k < A_TILE_W; k++) {
#pragma HLS PIPELINE II=1
					systolic2: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
						systolic3: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							unsigned global_i = a_tRow * A_TILE_H + i;
							unsigned global_k = a_tCol_offset + k;
							unsigned global_j = b_tCol * B_TILE_W + j;
							AccmType last;
							if (k == 0 && a_tCol == 0) {
								last = 0;
							} else {
								last = sum_p[i][j];
							}

							dina_t a_val;
							dinb_t b_val;
							if (global_i < a_row && global_k < a_col) {
								a_val = localA[i][a_tCol_offset_optn + k];
							} else {
								a_val = 0;
							}
							if (global_k < b_row && global_j < b_col) {
								b_val = localB[k][j];
							} else {
								b_val = 0;
							}

							mult_t rmul = clHalfToHalf(a_val) * clHalfToHalf(b_val);
//                          AccmType rmul_fixed = accm_t(rmul);
							AccmType rmul_fixed;
							half_to_fixed(rmul, rmul_fixed);
							AccmType radd;
//pragma HLS RESOURCE variable=radd core=HAddSub_nodsp latency=FADD_LAT_SUB1
//pragma HLS RESOURCE variable=radd core=FAddSub_fulldsp latency=FADD_LAT_SUB1
							radd = last + rmul_fixed;

							if (a_tCol_offset + k < a_col)
								sum_p[i][j] = radd;
//                          DBG_cout << "a_cast is " << a_cast << ", b_cast is " << b_cast << std::endl;
//                          DBG_cout << "rmul is " << rmul << ", rmul_fixed is " << rmul_fixed << std::endl;
//                          DBG_cout << "radd is " << radd << std::endl;
//                          DBG_cout << "sum_p[" << i << "][" << j << "] is " << sum_p[i][j] << std::endl;
						}
					}
				}
			} // end a_tc_loop
			accum1: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
				accum2: for (unsigned j = 0; j < SYSTLC_COL; j++) {
#pragma HLS PIPELINE
//                  accm_t reduce_out;
//                  reduce_sum_inline(sum_p[i][j], reduce_out);
					localC[i][j] = floatToClHalf(sum_p[i][j].to_float());
//                  DBG_cout << "sum_p[" << i << "][" << j << "].to_float() is " << sum_p[i][j].to_float() << std::endl;
//                  DBG_cout << "localC[" << i << "][" << j << "] " << localC[i][j] << std::endl;
				}
			}
			DEBUG_PRINT("---- finish calculate local C ----\n\n");
			cT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < C_TILE_WRD; beat++) {
#pragma HLS PIPELINE
				bout_t word;
				cV_loop: for (unsigned d = 0; d < C_VCTR_SIZE; d++)
					word.d[d] = localC[i][j + d];
				cFifo.write(word);
				if (j + C_VCTR_SIZE >= C_TILE_W) {
					j = 0;
					i++;
				} else {
					j += C_VCTR_SIZE;
				}
			}
		} // end b_tc_loop
	} // end a_tr_loop
} // end compute_stream

/*
 * Read gmem and output to buckets of fifos, bucket count equals tile rows/cols
 */
void read_gmem_buckets( //
		const bina_t *a, //
		const binb_t *b, //
		hls::stream<bina_t> reshapeA[A_TILE_H], //
		hls::stream<binb_t> reshapeB[B_TILE_W], //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned a_wrdPerRow = (a_col - 1) / A_VCTR_SIZE + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerCol = (b_row - 1) / B_TILE_H + 1;
	unsigned b_wrdPerRow = (b_col - 1) / B_VCTR_SIZE + 1;
	unsigned b_wrdPerCol = (b_row - 1) / B_VCTR_SIZE + 1;
	bina_t wordA;
	binb_t wordB;

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			// assuming A_TILE_W == B_TILE_H
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				unsigned b_tRow = a_tCol;
				// if caching, only read A for first tile column of B
				if (!a_cache || b_tCol == 0) {
					aH_loop: for (unsigned i = 0; i < A_TILE_H; i++) {
						aW_loop: for (unsigned j = 0; j < A_TILE_W_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = a_tRow * A_TILE_H * a_wrdPerRow + i * a_wrdPerRow + a_tCol * A_TILE_W_WRD + j;
							wordA = a[loc];
							reshapeA[i].write(wordA);
							DEBUG_PRINT("read A loc %d\n", loc);
						}
					}
				}
				if (!b_trans) {
					bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
						unsigned loc = b_tRow * B_TILE_H * b_wrdPerRow + i * b_wrdPerRow + b_tCol * B_TILE_W_WRD + j;
						wordB = b[loc];
						reshapeB[0].write(wordB);
						if (j == B_TILE_W_WRD - 1) {
							j = 0;
							i++;
						} else {
							j++;
						}
						DEBUG_PRINT("read B loc %d\n", loc);
					}
				} else {
					bW_loop: for (unsigned i = 0; i < B_TILE_W; i++) {
						bH_loop: for (unsigned j = 0; j < B_TILE_H_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = b_tCol * B_TILE_W * b_wrdPerCol + i * b_wrdPerCol + b_tRow * B_TILE_H_WRD + j;
							wordB = b[loc];
							reshapeB[i].write(wordB);
							DEBUG_PRINT("read B loc %d\n", loc);
						}
					}
				}
			}
		}
	}
}

/*
 * Same as read_gmem but writes to burst-beats number of bucket fifos
 */
void read_gmem_buckets_v4( //
		const bina_t *a, //
		const binb_t *b, //
		hls::stream<bina_t> reshapeA[A_TILE_W_WRD], //
		hls::stream<binb_t> reshapeB[B_TILE_H_WRD], //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned a_wrdPerRow = (a_col - 1) / A_VCTR_SIZE + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerCol = (b_row - 1) / B_TILE_H + 1;
	unsigned b_wrdPerRow = (b_col - 1) / B_VCTR_SIZE + 1;
	unsigned b_wrdPerCol = (b_row - 1) / B_VCTR_SIZE + 1;
	bina_t wordA;
	binb_t wordB;

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			// assuming A_TILE_W == B_TILE_H
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				unsigned b_tRow = a_tCol;
				// if caching, only read A for first tile column of B
				if (!a_cache || b_tCol == 0) {
					aH_loop: for (unsigned i = 0; i < A_TILE_H; i++) {
						aW_loop: for (unsigned j = 0; j < A_TILE_W_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = a_tRow * A_TILE_H * a_wrdPerRow + i * a_wrdPerRow + a_tCol * A_TILE_W_WRD + j;
							wordA = a[loc];
							reshapeA[j].write(wordA);
							DEBUG_PRINT("read A loc %d\n", loc);
						}
					}
				}
				if (!b_trans) {
					bT_loop: for (unsigned i = 0, j = 0, beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
						unsigned loc = b_tRow * B_TILE_H * b_wrdPerRow + i * b_wrdPerRow + b_tCol * B_TILE_W_WRD + j;
						wordB = b[loc];
						reshapeB[0].write(wordB);
						if (j == B_TILE_W_WRD - 1) {
							j = 0;
							i++;
						} else {
							j++;
						}
						DEBUG_PRINT("read B loc %d\n", loc);
					}
				} else {
					bW_loop: for (unsigned i = 0; i < B_TILE_W; i++) {
						bH_loop: for (unsigned j = 0; j < B_TILE_H_WRD; j++) {
#pragma HLS PIPELINE
							unsigned loc = b_tCol * B_TILE_W * b_wrdPerCol + i * b_wrdPerCol + b_tRow * B_TILE_H_WRD + j;
							wordB = b[loc];
							reshapeB[j].write(wordB);
							DEBUG_PRINT("read B loc %d\n", loc);
						}
					}
				}
			}
		}
	}
}

/*
 * Transforms input matrix A's raster scan order to column by column order
 */
void raster_trans_A( //
		hls::stream<bina_t> &inFifo, //
		hls::stream<bina_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;

	bina_t localA[A_TILE_H * A_TILE_W_WRD];

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_FLATTEN_TRIP max=A_TROW_FLATTEN_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
//pragma HLS DATAFLOW
				if (!a_cache || b_tCol == 0) {
					aT1_loop: for (unsigned beat = 0; beat < A_TILE_H * A_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
//					if (!a_cache || b_tCol == 0) {
						bina_t wordA = inFifo.read();
						localA[beat] = wordA;
//					}
					}
					aT2_loop: for (unsigned beat = 0, i = 0, j = 0; beat < A_TILE_H * A_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
//					if (!a_cache || b_tCol == 0) {
						bina_t wordA = localA[i * A_TILE_W_WRD + j];
						outFifo.write(wordA);
						DBG_cout << "raster_trans_A wrote beat " << beat << std::endl;
						if (i == A_TILE_H - 1) {
							i = 0;
							j++;
						} else {
							i++;
						}
//					}
					}
				}
			}
		}
	}
}

/*
 * Transforms input matrix A's raster scan order to column by column order
 */
void raster_trans_A_v7( //
		hls::stream<bina_t> &inFifo, //
		hls::stream<bina_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	bina_t localA[A_TILE_H * A_TILE_W_WRD];

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_FLATTEN_TRIP max=A_TROW_FLATTEN_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
//pragma HLS DATAFLOW
				if (!a_cache || b_tCol == 0) {
					aT1_loop: for (unsigned beat = 0; beat < A_TILE_H * A_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
//					if (!a_cache || b_tCol == 0) {
						bina_t wordA = inFifo.read();
						localA[beat] = wordA;
//					}
					}
					aT2_loop: for (unsigned beat = 0, i = 0, j = 0; beat < A_TILE_H * A_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
//					if (!a_cache || b_tCol == 0) {
						bina_t wordA = localA[i * A_TILE_W_WRD + j];
						outFifo.write(wordA);
						DBG_cout << "raster_trans_A wrote beat " << beat << std::endl;
						if (i == A_TILE_H - 1) {
							i = 0;
							j++;
						} else {
							i++;
						}
//					}
					}
				}
			}
		}
	}
}

/*
 *
 */
void raster_trans_B( //
		hls::stream<binb_t> &inFifo, //
		hls::stream<binb_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;

	bina_t localB[B_TILE_H * B_TILE_W_WRD];

	assert(B_TILE_H * B_TILE_W_WRD == B_TILE_W * B_TILE_H_WRD && "raster_trans_B: B_TILE_H * B_TILE_W_WRD needs to equal B_TILE_W * B_TILE_H_WRD");

	b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
		a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP

			if (!b_trans) {
				a_tr1_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
					bT3_loop: for (unsigned beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
						binb_t wordB = inFifo.read();
						outFifo.write(wordB);
					}
				}
			} else {
				a_tr2_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow += DF_FLAT_FAC) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_FLATTEN_TRIP max=A_TROW_FLATTEN_TRIP
					bDataflow_loop: for (unsigned f = 0; f < DF_FLAT_FAC; f++) { // to fix variable loop bound for DATAFLOW
#pragma HLS DATAFLOW
#pragma HLS LOOP_FLATTEN off
						bT1_loop: for (unsigned beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
							if (a_tRow + f < a_tPerCol) {
								binb_t wordB = inFifo.read();
								localB[beat] = wordB;
							}
						}
						bT2_loop: for (unsigned beat = 0, i = 0, j = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
							if (a_tRow + f < a_tPerCol) {
								binb_t wordB;
								wordB = localB[i * B_TILE_H_WRD + j];
								outFifo.write(wordB);
								if (i == B_TILE_W - 1) {
									i = 0;
									j++;
								} else {
									i++;
								}
							}
						}
					}
				}
			}

//			a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow += DF_FLAT_FAC) {
//#pragma HLS LOOP_TRIPCOUNT min=A_TROW_FLATTEN_TRIP max=A_TROW_FLATTEN_TRIP
//#pragma HLS LOOP_FLATTEN
//
//				bDataflow_loop: for (unsigned f = 0; f < DF_FLAT_FAC; f++) { // to fix variable loop bound for DATAFLOW
//#pragma HLS DATAFLOW
//#pragma HLS LOOP_FLATTEN off
//					bT1_loop: for (unsigned beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
//#pragma HLS PIPELINE
//						if (a_tRow + f < a_tPerCol) {
//							binb_t wordB = inFifo.read();
//							localB[beat] = wordB;
//						}
//					}
//					bT2_loop: for (unsigned beat = 0, i = 0, j = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
//#pragma HLS PIPELINE
//						if (a_tRow + f < a_tPerCol) {
//							binb_t wordB;
//							if (!b_trans) {
//								wordB = localB[beat];
//							} else {
//								wordB = localB[i * B_TILE_H_WRD + j];
//							}
//							outFifo.write(wordB);
//							if (i == B_TILE_W - 1) {
//								i = 0;
//								j++;
//							} else {
//								i++;
//							}
//						}
//					}
//				}
//			}

		}
	}
}

/*
 *
 */
void raster_trans_B_v7( //
		hls::stream<binb_t> &inFifo, //
		hls::stream<binb_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	//bina_t localB[B_TILE_H * B_TILE_W_WRD];
	bina_t localB[SYSTLC_COL * B_TILE_H_WRD];

	assert(B_TILE_H * B_TILE_W_WRD == B_TILE_W * B_TILE_H_WRD && "raster_trans_B: B_TILE_H * B_TILE_W_WRD needs to equal B_TILE_W * B_TILE_H_WRD");

	b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
		a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP

			if (!b_trans) {
				a_tr1_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
					bT3_loop: for (unsigned beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
						binb_t wordB = inFifo.read();
						outFifo.write(wordB);
					}
				}
			} else {
				a_tr2_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow += DF_FLAT_FAC) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_FLATTEN_TRIP max=A_TROW_FLATTEN_TRIP
					bDataflow_loop: for (unsigned f = 0; f < DF_FLAT_FAC; f++) { // to fix variable loop bound for DATAFLOW
#pragma HLS DATAFLOW
#pragma HLS LOOP_FLATTEN off
						bT1_loop: for (unsigned beat = 0; beat < SYSTLC_COL * B_TILE_H_WRD; beat++) {
#pragma HLS PIPELINE
							if (a_tRow + f < a_tPerCol) {
								binb_t wordB = inFifo.read();
								localB[beat] = wordB;
							}
						}
						bT2_loop: for (unsigned beat = 0, i = 0, j = 0; beat < SYSTLC_COL * B_TILE_H_WRD; beat++) {
#pragma HLS PIPELINE
							if (a_tRow + f < a_tPerCol) {
								binb_t wordB;
								wordB = localB[i * B_TILE_H_WRD + j];
								outFifo.write(wordB);
								if (i == SYSTLC_COL - 1) {
									i = 0;
									j++;
								} else {
									i++;
								}
							}
						}
					}
				}
			}

//			a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow += DF_FLAT_FAC) {
//#pragma HLS LOOP_TRIPCOUNT min=A_TROW_FLATTEN_TRIP max=A_TROW_FLATTEN_TRIP
//#pragma HLS LOOP_FLATTEN
//
//				bDataflow_loop: for (unsigned f = 0; f < DF_FLAT_FAC; f++) { // to fix variable loop bound for DATAFLOW
//#pragma HLS DATAFLOW
//#pragma HLS LOOP_FLATTEN off
//					bT1_loop: for (unsigned beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
//#pragma HLS PIPELINE
//						if (a_tRow + f < a_tPerCol) {
//							binb_t wordB = inFifo.read();
//							localB[beat] = wordB;
//						}
//					}
//					bT2_loop: for (unsigned beat = 0, i = 0, j = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
//#pragma HLS PIPELINE
//						if (a_tRow + f < a_tPerCol) {
//							binb_t wordB;
//							if (!b_trans) {
//								wordB = localB[beat];
//							} else {
//								wordB = localB[i * B_TILE_H_WRD + j];
//							}
//							outFifo.write(wordB);
//							if (i == B_TILE_W - 1) {
//								i = 0;
//								j++;
//							} else {
//								i++;
//							}
//						}
//					}
//				}
//			}

		}
	}
}

/*
 *
 */
void raster_trans_B_v7_1( //
		hls::stream<binb_t> &inFifo, //
		hls::stream<binb_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	//bina_t localB[B_TILE_H * B_TILE_W_WRD];
	bina_t localB[SYSTLC_COL * B_TILE_H_WRD];

	assert(B_TILE_H * B_TILE_W_WRD == B_TILE_W * B_TILE_H_WRD && "raster_trans_B: B_TILE_H * B_TILE_W_WRD needs to equal B_TILE_W * B_TILE_H_WRD");

	a_tr1_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP

				if (!b_trans) {
					bT3_loop: for (unsigned beat = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
						binb_t wordB = inFifo.read();
						outFifo.write(wordB);
					}
				} else {
					//a_tr2_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow += DF_FLAT_FAC)
					//#pragma HLS LOOP_TRIPCOUNT min=A_TROW_FLATTEN_TRIP max=A_TROW_FLATTEN_TRIP
					//bDataflow_loop: for (unsigned f = 0; f < DF_FLAT_FAC; f++)  // to fix variable loop bound for DATAFLOW
					//#pragma HLS DATAFLOW
					//#pragma HLS LOOP_FLATTEN off
					bT1_loop: for (unsigned beat = 0; beat < SYSTLC_COL * B_TILE_H_WRD; beat++) {
#pragma HLS PIPELINE
//                        if (a_tRow + f < a_tPerCol) {
						binb_t wordB = inFifo.read();
						localB[beat] = wordB;
//                        }
					}
					bT2_loop: for (unsigned beat = 0, i = 0, j = 0; beat < SYSTLC_COL * B_TILE_H_WRD; beat++) {
#pragma HLS PIPELINE
//                        if (a_tRow + f < a_tPerCol) {
						binb_t wordB;
						wordB = localB[i * B_TILE_H_WRD + j];
						outFifo.write(wordB);
						if (i == SYSTLC_COL - 1) {
							i = 0;
							j++;
						} else {
							i++;
						}
//                        }
					}
				}
			}
		}
	}
}

typedef struct bsysa_struct {
	dina_t d[SYSTLC_ROW];
} bsysa_t;

void word_to_systlc_A_dfloop( //
		hls::stream<bina_t> &inFifo, //
		hls::stream<bsysa_t> &outFifo, //
		unsigned b_tCol, //
		unsigned char a_cache) {
	bina_t wordA;
	bsysa_t sysWordA;
	const unsigned A_PART_FAC = (A_TILE_H - 1) / 2 + 1;
	dina_t localA[A_TILE_H][A_VCTR_SIZE];
#pragma HLS ARRAY_PARTITION variable=localA cyclic factor=A_PART_FAC dim=1
#pragma HLS ARRAY_PARTITION variable=localA complete dim=2

	aT1_loop: for (unsigned i = 0; i < A_TILE_W_WRD; i++) {
#pragma HLS DATAFLOW
//pragma HLS LOOP_FLATTEN off
		aT2_loop: for (unsigned j = 0; j < A_TILE_H; j++) {
#pragma HLS PIPELINE II=1
//			if (!a_cache || b_tCol == 0) {
			wordA = inFifo.read();
			DBG_cout << "word_to_systlc_A_dfloop read beat " << i * A_TILE_H + j << std::endl;
			aV1_loop: for (unsigned k = 0; k < A_VCTR_SIZE; k++)
				localA[j][k] = wordA.d[k];
//			}
		}
		aV_loop: for (unsigned j = 0; j < A_VCTR_SIZE; j++) {
#pragma HLS PIPELINE II=1
//			if (!a_cache || b_tCol == 0) {
			aH_loop: for (unsigned k = 0; k < A_TILE_H; k++) {
				sysWordA.d[k] = localA[k][j];
			}
			outFifo.write(sysWordA);
//			}
		}
	}
}

/*
 *
 */
void word_to_systlc_A( //
		hls::stream<bina_t> &inFifo, //
		hls::stream<bsysa_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

//	bina_t wordA;
//	bsysa_t sysWordA;

//	const unsigned A_PART_FAC = (A_TILE_H - 1) / 2 + 1;
////	bina_t localA[A_TILE_H];
//	dina_t localA[A_TILE_H][A_VCTR_SIZE];
//#pragma HLS ARRAY_PARTITION variable=localA cyclic factor=A_PART_FAC dim=1
//#pragma HLS ARRAY_PARTITION variable=localA complete dim=2

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_FLATTEN
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				if (!a_cache || b_tCol == 0) {
					word_to_systlc_A_dfloop(inFifo, outFifo, b_tCol, a_cache);
				}
//				aT1_loop: for (unsigned i = 0; i < A_TILE_W_WRD; i++) {
//#pragma HLS DATAFLOW
//#pragma HLS LOOP_FLATTEN off
//					aT2_loop: for (unsigned j = 0; j < A_TILE_H; j++) {
//#pragma HLS PIPELINE II=1
//						if (!a_cache || b_tCol == 0) {
//							wordA = inFifo.read();
//							aV1_loop: for (unsigned k = 0; k < A_VCTR_SIZE; k++)
//								localA[j][k] = wordA.d[k];
//						}
//					}
//					aV_loop: for (unsigned j = 0; j < A_VCTR_SIZE; j++) {
//#pragma HLS PIPELINE II=1
//						if (!a_cache || b_tCol == 0) {
//							aH_loop: for (unsigned k = 0; k < A_TILE_H; k++) {
//								sysWordA.d[k] = localA[k][j];
//							}
//							outFifo.write(sysWordA);
//						}
//					}
//				}
			}
		}
	}
}

/*
 *
 */
void word_to_systlc_A_v7( //
		hls::stream<bina_t> &inFifo, //
		hls::stream<bsysa_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

//	bina_t wordA;
//	bsysa_t sysWordA;

//	const unsigned A_PART_FAC = (A_TILE_H - 1) / 2 + 1;
////	bina_t localA[A_TILE_H];
//	dina_t localA[A_TILE_H][A_VCTR_SIZE];
//#pragma HLS ARRAY_PARTITION variable=localA cyclic factor=A_PART_FAC dim=1
//#pragma HLS ARRAY_PARTITION variable=localA complete dim=2

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_FLATTEN
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				if (!a_cache || b_tCol == 0) {
					word_to_systlc_A_dfloop(inFifo, outFifo, b_tCol, a_cache);
				}
//				aT1_loop: for (unsigned i = 0; i < A_TILE_W_WRD; i++) {
//#pragma HLS DATAFLOW
//#pragma HLS LOOP_FLATTEN off
//					aT2_loop: for (unsigned j = 0; j < A_TILE_H; j++) {
//#pragma HLS PIPELINE II=1
//						if (!a_cache || b_tCol == 0) {
//							wordA = inFifo.read();
//							aV1_loop: for (unsigned k = 0; k < A_VCTR_SIZE; k++)
//								localA[j][k] = wordA.d[k];
//						}
//					}
//					aV_loop: for (unsigned j = 0; j < A_VCTR_SIZE; j++) {
//#pragma HLS PIPELINE II=1
//						if (!a_cache || b_tCol == 0) {
//							aH_loop: for (unsigned k = 0; k < A_TILE_H; k++) {
//								sysWordA.d[k] = localA[k][j];
//							}
//							outFifo.write(sysWordA);
//						}
//					}
//				}
			}
		}
	}
}

/*
 *
 */
void cache_systlc_A( //
		hls::stream<bsysa_t> &inFifo, //
		hls::stream<bsysa_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;

	bsysa_t cacheA[A_TILE_W * A_LCL_TILES];

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				aW_loop: for (unsigned i = 0; i < A_TILE_W; i++) {
#pragma HLS PIPELINE
					bsysa_t wordA;
					if (!a_cache) {
						wordA = inFifo.read();
					} else if (b_tCol == 0) {
						wordA = inFifo.read();
						cacheA[a_tCol * A_TILE_W + i] = wordA;
					} else {
						wordA = cacheA[a_tCol * A_TILE_W + i];
					}
					outFifo.write(wordA);
				}
			}
		}
	}
}

/*
 *
 */
void cache_systlc_A_v7( //
		hls::stream<bsysa_t> &inFifo, //
		hls::stream<bsysa_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	bsysa_t cacheA[A_TILE_W * A_LCL_TILES];

	assert((!a_cache || (a_cache && (a_tPerRow <= A_LCL_TILES))) && "Caching A only works if a_tPerRow <= A_LCL_TILES"); // assertion needed on host as well

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				aW_loop: for (unsigned i = 0; i < A_TILE_W; i++) {
#pragma HLS PIPELINE
					bsysa_t wordA;
					if (!a_cache) {
						wordA = inFifo.read();
					} else if (b_tCol == 0) {
						wordA = inFifo.read();
						cacheA[a_tCol * A_TILE_W + i] = wordA;
					} else {
						wordA = cacheA[a_tCol * A_TILE_W + i];
					}
					outFifo.write(wordA);
				}
			}
		}
	}
}

typedef struct bsysb_struct {
	dinb_t d[SYSTLC_COL];
} bsysb_t;

/*
 *
 */
void word_to_systlc_B( //
		hls::stream<binb_t> &inFifo, //
		hls::stream<bsysb_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;

	assert(B_TILE_H * B_TILE_W_WRD == B_TILE_W * B_TILE_H_WRD && "raster_trans_B: B_TILE_H * B_TILE_W_WRD needs to equal B_TILE_W * B_TILE_H_WRD");

	const unsigned B_PART_FAC1 = B_VCTR_SIZE;
	dinb_t lineB[B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=lineB cyclic factor=B_PART_FAC1 dim=1

	const unsigned B_PART_FAC2 = (SYSTLC_COL - 1) / 2 + 1;
//	binb_t localB[B_TILE_W];
	dinb_t localB[B_TILE_W][B_VCTR_SIZE];
#pragma HLS ARRAY_PARTITION variable=localB cyclic factor=B_PART_FAC2 dim=1
#pragma HLS ARRAY_PARTITION variable=localB complete dim=2

	const unsigned BH1_PIPE_DPTH = (B_SYS_COL_MUL > B_TILE_W_WRD ? B_SYS_COL_MUL : B_TILE_W_WRD);

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
#pragma HLS LOOP_FLATTEN
				if (!b_trans) {
					bH1_loop: for (unsigned i = 0; i < B_TILE_H; i++) {
//#pragma HLS DATAFLOW
#pragma HLS PIPELINE II=BH1_PIPE_DPTH
						bW1_loop: for (unsigned j = 0; j < B_TILE_W_WRD; j++) {
//#pragma HLS PIPELINE
							binb_t wordB = inFifo.read();
							bV1_loop: for (unsigned k = 0; k < B_VCTR_SIZE; k++) {
								lineB[j * B_VCTR_SIZE + k] = wordB.d[k];
							}
						}
						bS1_loop: for (unsigned j = 0; j < B_SYS_COL_MUL; j++) {
//#pragma HLS PIPELINE
							bsysb_t sysWordB;
							bV2_loop: for (unsigned k = 0; k < SYSTLC_COL; k++) {
								sysWordB.d[k] = lineB[j * SYSTLC_COL + k];
							}
							outFifo.write(sysWordB);
						}
					}
				} else {
					bH2_loop: for (unsigned i = 0; i < B_TILE_H_WRD; i++) {
#pragma HLS DATAFLOW
						bW2_loop: for (unsigned j = 0; j < B_TILE_W; j++) {
#pragma HLS PIPELINE
							binb_t wordB = inFifo.read();
							DBG_cout << "DBG word_to_systlc_B in: ";
							bV5_loop: for (unsigned k = 0; k < B_VCTR_SIZE; k++) {
								localB[j][k] = wordB.d[k];
								DBG_cout << clHalfToHalf(wordB.d[k]) << ", ";
							}
							DBG_cout << std::endl;
						}
						bV3_loop: for (unsigned j = 0; j < B_VCTR_SIZE; j++) {
							bS2_loop: for (unsigned k = 0; k < B_SYS_COL_MUL; k++) {
#pragma HLS PIPELINE
								bsysb_t sysWordB;
								DBG_cout << "DBG word_to_systlc_B out: ";
								bV4_loop: for (unsigned d = 0; d < SYSTLC_COL; d++) {
									sysWordB.d[d] = localB[k * SYSTLC_COL + d][j];
									DBG_cout << clHalfToHalf(sysWordB.d[d]) << ", ";
								}
								DBG_cout << std::endl;
								outFifo.write(sysWordB);
							}
						}
					}
				}
			}
		}
	}
}

/*
 *
 */
void word_to_systlc_B_v7( //
		hls::stream<binb_t> &inFifo, //
		hls::stream<bsysb_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	assert(B_TILE_H * B_TILE_W_WRD == B_TILE_W * B_TILE_H_WRD && "raster_trans_B: B_TILE_H * B_TILE_W_WRD needs to equal B_TILE_W * B_TILE_H_WRD");

	const unsigned B_PART_FAC1 = B_VCTR_SIZE;
	dinb_t lineB[B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=lineB cyclic factor=B_PART_FAC1 dim=1

	const unsigned B_PART_FAC2 = (SYSTLC_COL - 1) / 2 + 1;
//	binb_t localB[B_TILE_W];
	dinb_t localB[SYSTLC_COL][B_VCTR_SIZE];
#pragma HLS ARRAY_PARTITION variable=localB cyclic factor=B_PART_FAC2 dim=1
#pragma HLS ARRAY_PARTITION variable=localB complete dim=2

	const unsigned BH1_PIPE_DPTH = (B_SYS_COL_MUL > B_TILE_W_WRD ? B_SYS_COL_MUL : B_TILE_W_WRD);

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
#pragma HLS LOOP_FLATTEN
				if (!b_trans) {
					bH1_loop: for (unsigned i = 0; i < B_TILE_H; i++) {
//#pragma HLS DATAFLOW
#pragma HLS PIPELINE II=BH1_PIPE_DPTH
						bW1_loop: for (unsigned j = 0; j < B_TILE_W_WRD; j++) {
//#pragma HLS PIPELINE
							binb_t wordB = inFifo.read();
							bV1_loop: for (unsigned k = 0; k < B_VCTR_SIZE; k++) {
								lineB[j * B_VCTR_SIZE + k] = wordB.d[k];
							}
						}
						bS1_loop: for (unsigned j = 0; j < B_SYS_COL_MUL; j++) {
//#pragma HLS PIPELINE
							bsysb_t sysWordB;
							bV2_loop: for (unsigned k = 0; k < SYSTLC_COL; k++) {
								sysWordB.d[k] = lineB[j * SYSTLC_COL + k];
							}
							outFifo.write(sysWordB);
						}
					}
				} else {
					bH2_loop: for (unsigned i = 0; i < B_TILE_H_WRD; i++) {
#pragma HLS DATAFLOW
						bW2_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
#pragma HLS PIPELINE
							binb_t wordB = inFifo.read();
							DBG_cout << "DBG word_to_systlc_B in: ";
							bV5_loop: for (unsigned k = 0; k < B_VCTR_SIZE; k++) {
								localB[j][k] = wordB.d[k];
								DBG_cout << clHalfToHalf(wordB.d[k]) << ", ";
							}
							DBG_cout << std::endl;
						}
						bV3_loop: for (unsigned j = 0; j < B_VCTR_SIZE; j++) {
							//bS2_loop: for (unsigned k = 0; k < B_SYS_COL_MUL; k++) {
#pragma HLS PIPELINE
							bsysb_t sysWordB;
							DBG_cout << "DBG word_to_systlc_B out: ";
							bV4_loop: for (unsigned d = 0; d < SYSTLC_COL; d++) {
								sysWordB.d[d] = localB[d][j];
								DBG_cout << clHalfToHalf(sysWordB.d[d]) << ", ";
							}
							DBG_cout << std::endl;
							outFifo.write(sysWordB);
							//}
						}
					}
				}
			}
		}
	}
}

typedef struct bsum_struct {
	accm_t d[SYSTLC_COL];
} baccm_t;

/*
 *
 */
void compute_stream_v5( //
		hls::stream<bsysa_t> &aFifo, //
		hls::stream<bsysb_t> &bFifo, //
		hls::stream<baccm_t> psumFifo[SYSTLC_ROW],        //
		unsigned a_row,        //
		unsigned a_col,        //
		unsigned b_col,        //
		unsigned char a_cache,        //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;

	const unsigned TREE_FAC = ((TREE_SIZE - 1) / B_SYS_COL_MUL + 1);
//	accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][B_SYS_COL_MUL][TREE_SIZE];
	accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][B_SYS_COL_MUL * TREE_FAC];
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2

	bsysa_t wordA;
	bsysb_t wordB;
	baccm_t sumWord;

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP

				sys1_loop: for (unsigned beat = 0, k = 0, m = 0; beat < A_TILE_W * B_SYS_COL_MUL; beat++) {
//			sys1_loop: for (unsigned beat = 0, k = 0, m = 0; beat < a_col * B_SYS_COL_MUL; beat++) {
//#pragma HLS LOOP_TRIPCOUNT min=256 max=256

#pragma HLS PIPELINE II=1
					if (m == 0)
						wordA = aFifo.read();
					wordB = bFifo.read();

					DBG_cout << "dbgA: " << std::endl;
					dbgA_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++)
						DBG_cout << clHalfToHalf(wordA.d[i]) << ", ";
					DBG_cout << std::endl;
					DBG_cout << "dbgB: " << std::endl;
					dbgB_loop: for (unsigned i = 0; i < SYSTLC_COL; i++)
						DBG_cout << clHalfToHalf(wordB.d[i]) << ", ";
					DBG_cout << std::endl;

					unsigned a_tCol_offset = a_tCol * A_TILE_W;
					unsigned global_k = a_tCol_offset + k;
					sys2_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
						sys3_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							unsigned global_i = a_tRow * A_TILE_H + i;
							unsigned global_j = b_tCol * B_TILE_W + m * SYSTLC_COL + j;
							accm_t last;
							if (global_k < TREE_FAC) {
								last = 0;
							} else {
//								last = sum_p[i][j][beat % B_SYS_COL_MUL][k % TREE_SIZE];
								last = sum_p[i][j][beat % (B_SYS_COL_MUL * TREE_FAC)];
							}

							dina_t a_val;
							dinb_t b_val;
							if (global_i < a_row && global_k < a_col) {
								a_val = wordA.d[i];
							} else {
								a_val = 0;
							}
							if (global_k < b_row && global_j < b_col) {
								b_val = wordB.d[j];
							} else {
								b_val = 0;
							}

							mult_t rmul = clHalfToHalf(a_val) * clHalfToHalf(b_val);
							accm_t radd;
							radd = last + rmul;
							if (global_k < a_col) {
//								sum_p[i][j][beat % B_SYS_COL_MUL][k % TREE_SIZE] = radd;
								sum_p[i][j][beat % (B_SYS_COL_MUL * TREE_FAC)] = radd;
							}
							sumWord.d[j] = radd;
						}
						if (global_k >= a_col - TREE_FAC && global_k < a_col)
							psumFifo[i].write(sumWord);
					}
					if (m == B_SYS_COL_MUL - 1) {
						m = 0;
						k++;
					} else {
						m++;
					}
				} // end k
			} // end a_tc_loop
		} // end b_tc_loop
	} // end a_tr_loop
} // end compute_stream

/*
 *
 */
void compute_stream_v6( //
		hls::stream<bsysa_t> &aFifo, //
		hls::stream<bsysb_t> &bFifo, //
		hls::stream<baccm_t> &psumFifo, //
		unsigned a_row,        //
		unsigned a_col,        //
		unsigned b_col,        //
		unsigned char a_cache,        //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;

	const unsigned TREE_FAC = ((TREE_SIZE - 1) / B_SYS_COL_MUL + 1);
	accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][B_SYS_COL_MUL * TREE_FAC];
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2

	bsysa_t wordA;
	bsysb_t wordB;
	baccm_t sumWord;

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP

				sys1_loop: for (unsigned beat = 0, k = 0, m = 0; beat < A_TILE_W * B_SYS_COL_MUL; beat++) {
//			sys1_loop: for (unsigned beat = 0, k = 0, m = 0; beat < a_col * B_SYS_COL_MUL; beat++) 
//#pragma HLS LOOP_TRIPCOUNT min=256 max=256

#pragma HLS PIPELINE II=1
					if (m == 0)
						wordA = aFifo.read();
					wordB = bFifo.read();

					DBG_cout << "dbgA: " << std::endl;
					dbgA_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++)
						DBG_cout << clHalfToHalf(wordA.d[i]) << ", ";
					DBG_cout << std::endl;
					DBG_cout << "dbgB: " << std::endl;
					dbgB_loop: for (unsigned i = 0; i < SYSTLC_COL; i++)
						DBG_cout << clHalfToHalf(wordB.d[i]) << ", ";
					DBG_cout << std::endl;

					unsigned a_tCol_offset = a_tCol * A_TILE_W;
					unsigned global_k = a_tCol_offset + k;
					sys2_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
						sys3_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							unsigned global_i = a_tRow * A_TILE_H + i;
							unsigned global_j = b_tCol * B_TILE_W + m * SYSTLC_COL + j;
							accm_t last;
							if (global_k < TREE_FAC/* && a_tCol == 0*/) {
								last = 0;
							} else {
//								last = sum_p[i][j][beat % B_SYS_COL_MUL][k % TREE_SIZE];
								last = sum_p[i][j][beat % (B_SYS_COL_MUL * TREE_FAC)];
							}

							dina_t a_val;
							dinb_t b_val;
							if (global_i < a_row && global_k < a_col) {
								//a_val = localA[i][a_tCol_offset_optn + k];
								a_val = wordA.d[i];
							} else {
								a_val = 0;
							}
							if (global_k < b_row && global_j < b_col) {
								//b_val = localB[k][j];
								b_val = wordB.d[j];
							} else {
								b_val = 0;
							}

							mult_t rmul = clHalfToHalf(a_val) * clHalfToHalf(b_val);
							accm_t radd;
							radd = last + rmul;
							if (global_k < a_col) {
//								sum_p[i][j][beat % B_SYS_COL_MUL][k % TREE_SIZE] = radd;
								sum_p[i][j][beat % (B_SYS_COL_MUL * TREE_FAC)] = radd;
							}
							//sumWord.d[j] = radd;
						}
						//if (global_k >= a_col - TREE_FAC && global_k < a_col)
						//	psumFifo[i].write(sumWord);
					}
					if (m == B_SYS_COL_MUL - 1) {
						m = 0;
						k++;
					} else {
						m++;
					}
				}        //end k
			} // end a_tc_loop
			sum_loop: for (unsigned m = 0; m < B_SYS_COL_MUL * TREE_FAC; m++) {
				sumr_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
#pragma HLS PIPELINE
					sumc_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
						sumWord.d[j] = sum_p[i][j][m];
					}
					psumFifo.write(sumWord);
				}
			}
		} // end b_tc_loop
	} // end a_tr_loop
} // end compute_stream

/*
 *
 */
void compute_stream_v7( //
		hls::stream<bsysa_t> &aFifo, //
		hls::stream<bsysb_t> &bFifo, //
		hls::stream<baccm_t> &psumFifo, //
		unsigned a_row,        //
		unsigned a_col,        //
		unsigned b_col,        //
		unsigned char a_cache,        //
		unsigned char b_trans) {
	unsigned b_row = a_col;
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	const unsigned TREE_FAC = ((TREE_SIZE - 1) / B_SYS_COL_MUL + 1);
	accm_t sum_p[SYSTLC_ROW][SYSTLC_COL][B_SYS_COL_MUL * TREE_FAC];
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=1
#pragma HLS ARRAY_PARTITION variable=sum_p complete dim=2

	bsysa_t wordA;
	bsysb_t wordB;
	baccm_t sumWord;

	unsigned SYS_TRIP = (b_trans ? A_TILE_W : A_TILE_W * B_SYS_COL_MUL);
	const unsigned MIN_TRIP = A_TILE_W;
	const unsigned MAX_TRIP = A_TILE_W * B_SYS_COL_MUL;

	unsigned k_init_bound = (b_trans ? B_SYS_COL_MUL * TREE_FAC : TREE_FAC);

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
				sys1_loop: for (unsigned beat = 0, k = 0, m = 0; beat < SYS_TRIP; beat++) {
#pragma HLS LOOP_TRIPCOUNT min=MIN_TRIP max=MAX_TRIP
//			sys1_loop: for (unsigned beat = 0, k = 0, m = 0; beat < a_col * B_SYS_COL_MUL; beat++) 
//#pragma HLS LOOP_TRIPCOUNT min=256 max=256

#pragma HLS PIPELINE II=1
					if (m == 0)
						wordA = aFifo.read();
					wordB = bFifo.read();

					DBG_cout << "dbgA: " << std::endl;
					dbgA_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++)
						DBG_cout << clHalfToHalf(wordA.d[i]) << ", ";
					DBG_cout << std::endl;
					DBG_cout << "dbgB: " << std::endl;
					dbgB_loop: for (unsigned i = 0; i < SYSTLC_COL; i++)
						DBG_cout << clHalfToHalf(wordB.d[i]) << ", ";
					DBG_cout << std::endl;

					unsigned a_tCol_offset = a_tCol * A_TILE_W;
					unsigned global_k = a_tCol_offset + k;
					sys2_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
						sys3_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							unsigned global_i = a_tRow * A_TILE_H + i;
							unsigned global_j = b_tCol * B_TILE_W + m * SYSTLC_COL + j;
							accm_t last;
							if (global_k < k_init_bound) {
								last = 0;
							} else {
								last = sum_p[i][j][beat % (B_SYS_COL_MUL * TREE_FAC)];
							}

							dina_t a_val;
							dinb_t b_val;
							if (global_i < a_row && global_k < a_col) {
								a_val = wordA.d[i];
							} else {
								a_val = 0;
							}
							if (global_k < b_row && global_j < b_col) {
								b_val = wordB.d[j];
							} else {
								b_val = 0;
							}

							mult_t rmul = clHalfToHalf(a_val) * clHalfToHalf(b_val);
							accm_t radd;
							radd = last + rmul;
							if (global_k < a_col) {
								sum_p[i][j][beat % (B_SYS_COL_MUL * TREE_FAC)] = radd;
							}
							//sumWord.d[j] = radd;
						}
						//if (global_k >= a_col - TREE_FAC && global_k < a_col)
						//	psumFifo[i].write(sumWord);
					}
					if (b_trans) {
						k++;
					} else {
						if (m == B_SYS_COL_MUL - 1) {
							m = 0;
							k++;
						} else {
							m++;
						}
					}
				}        //end k
			} // end a_tc_loop
			sum_loop: for (unsigned m = 0; m < B_SYS_COL_MUL * TREE_FAC; m++) {
				sumr_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
#pragma HLS PIPELINE
					sumc_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
						sumWord.d[j] = sum_p[i][j][m];
					}
					psumFifo.write(sumWord);
				}
			}
		} // end b_tc_loop
	} // end a_tr_loop
} // end compute_stream

/*
 *
 */
void accm_stream_v5( //
		hls::stream<baccm_t> psumFifo[SYSTLC_ROW], //
		hls::stream<baccm_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;

	const unsigned TREE_FAC = ((TREE_SIZE - 1) / B_SYS_COL_MUL + 1);

	const unsigned B_PART_FAC = B_VCTR_SIZE;
	accm_t localC[SYSTLC_ROW][B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC cyclic factor=B_PART_FAC dim=2

	baccm_t retWord;

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			tree_loop: for (unsigned k = 0; k < TREE_FAC; k++) {
				bS_loop: for (unsigned m = 0; m < B_SYS_COL_MUL; m++) {
					sysr_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
#pragma HLS PIPELINE II=1
						baccm_t sumWord = psumFifo[i].read();
//						baccm_t retWord;
						sysc_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							if (k == 0)
								localC[i][m * SYSTLC_COL + j] = sumWord.d[j];
							else {
								accm_t last = localC[i][m * SYSTLC_COL + j];
								accm_t radd = last + sumWord.d[j];
								localC[i][m * SYSTLC_COL + j] = radd;
//								retWord.d[j] = radd;
							}
						}
//						if (k == TREE_SIZE - 1)
//							outFifo.write(retWord);
					}
				}
			}
			sysr2_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
				bS2_loop: for (unsigned m = 0; m < B_SYS_COL_MUL; m++) {
#pragma HLS PIPELINE II=1
					sysc2_loop: for (unsigned j = 0; j < SYSTLC_COL; j++)
						retWord.d[j] = localC[i][m * SYSTLC_COL + j];
					outFifo.write(retWord);
				}
			}
		}
	}
}

/*
 *
 */
void accm_stream_v6( //
		hls::stream<baccm_t> &psumFifo, //
		hls::stream<baccm_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;

	const unsigned TREE_FAC = ((TREE_SIZE - 1) / B_SYS_COL_MUL + 1);

	const unsigned B_PART_FAC = B_VCTR_SIZE;
	accm_t localC[SYSTLC_ROW][B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC cyclic factor=B_PART_FAC dim=2

	baccm_t retWord;

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			tree_loop: for (unsigned k = 0; k < TREE_FAC; k++) {
				bS_loop: for (unsigned m = 0; m < B_SYS_COL_MUL; m++) {
					sysr_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
#pragma HLS PIPELINE II=1
						baccm_t sumWord = psumFifo.read();
						sysc_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
							if (k == 0)
								localC[i][m * SYSTLC_COL + j] = sumWord.d[j];
							else {
								accm_t last = localC[i][m * SYSTLC_COL + j];
								accm_t radd = last + sumWord.d[j];
								localC[i][m * SYSTLC_COL + j] = radd;
							}
						}
					}
				}
			}
			sysr2_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
				bS2_loop: for (unsigned m = 0; m < B_SYS_COL_MUL; m++) {
#pragma HLS PIPELINE II=1
					sysc2_loop: for (unsigned j = 0; j < SYSTLC_COL; j++)
						retWord.d[j] = localC[i][m * SYSTLC_COL + j];
					outFifo.write(retWord);
				}
			}
		}
	}
}

/*
 *
 */
void accm_stream_v7( //
		hls::stream<baccm_t> &psumFifo, //
		hls::stream<baccm_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	const unsigned TREE_FAC = ((TREE_SIZE - 1) / B_SYS_COL_MUL + 1);

	const unsigned B_PART_FAC = B_VCTR_SIZE;
	accm_t localC[SYSTLC_ROW][B_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC cyclic factor=B_PART_FAC dim=2

	baccm_t retWord;

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_TRIP
			//tree_loop: for (unsigned k = 0; k < TREE_FAC; k++) 
			//	bS_loop: for (unsigned m = 0; m < B_SYS_COL_MUL; m++) 
			tree_loop: for (unsigned beat = 0, m = 0, k = 0; beat < TREE_FAC * B_SYS_COL_MUL; beat++) {
				sysr_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
#pragma HLS PIPELINE II=1
					baccm_t sumWord = psumFifo.read();
					sysc_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
						if (k == 0)
							localC[i][m * SYSTLC_COL + j] = sumWord.d[j];
						else {
							accm_t last = localC[i][m * SYSTLC_COL + j];
							accm_t radd = last + sumWord.d[j];
							localC[i][m * SYSTLC_COL + j] = radd;
						}
					}
				}
				if (b_trans) {
					k++;
				} else {
					if (m == B_SYS_COL_MUL - 1) {
						m = 0;
						k++;
					} else {
						m++;
					}
				}
			}
			sysr2_loop: for (unsigned i = 0; i < SYSTLC_ROW; i++) {
				unsigned m_bound = (b_trans ? 1 : B_SYS_COL_MUL);
				const unsigned MAX_TRIP = B_SYS_COL_MUL;
				bS2_loop: for (unsigned m = 0; m < m_bound; m++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_TRIP
#pragma HLS PIPELINE II=1
					sysc2_loop: for (unsigned j = 0; j < SYSTLC_COL; j++)
						retWord.d[j] = localC[i][m * SYSTLC_COL + j];
					outFifo.write(retWord);
				}
			}
		}
	}
}

/*
 *
 */
void systlc_to_word_C( //
		hls::stream<baccm_t> &inFifo, //
		hls::stream<bout_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;

	baccm_t sumWord;

	const unsigned C_PART_FAC = C_VCTR_SIZE;
	dout_t lineC[C_TILE_W];
#pragma HLS ARRAY_PARTITION variable=lineC cyclic factor=C_PART_FAC dim=1

	const unsigned C_PIPE_DPTH = B_SYS_COL_MUL;

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP

			cH_loop: for (unsigned i = 0; i < C_TILE_H; i++) {
//pragma HLS DATAFLOW
#pragma HLS PIPELINE II=C_PIPE_DPTH
				cS_loop: for (unsigned j = 0; j < B_SYS_COL_MUL; j++) {
//#pragma HLS PIPELINE II=1
					sumWord = inFifo.read();
					cV1_loop: for (unsigned k = 0; k < SYSTLC_COL; k++) {
						lineC[j * SYSTLC_COL + k] = halfToClHalf(sumWord.d[k]);
					}
				}
				cW_loop: for (unsigned j = 0; j < C_TILE_W_WRD; j++) {
//#pragma HLS PIPELINE II=1
					bout_t outWord;
					cV2_loop: for (unsigned k = 0; k < C_VCTR_SIZE; k++) {
						outWord.d[k] = lineC[j * C_VCTR_SIZE + k];
					}
					outFifo.write(outWord);
				}
			}
		}
	}
}

/*
 *
 */
void systlc_to_word_C_v7( //
		hls::stream<baccm_t> &inFifo, //
		hls::stream<bout_t> &outFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	//unsigned b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	baccm_t sumWord;

	const unsigned C_PART_FAC = C_VCTR_SIZE;
	dout_t lineC[C_TILE_W];
#pragma HLS ARRAY_PARTITION variable=lineC cyclic factor=C_PART_FAC dim=1

	const unsigned C_PART_FAC2 = SYSTLC_COL;
	dout_t localC[C_TILE_H][C_TILE_W];
#pragma HLS ARRAY_PARTITION variable=localC cyclic factor=C_PART_FAC2 dim=2

	const unsigned C_PIPE_DPTH = B_SYS_COL_MUL;

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
			if (!b_trans) {
				cH_loop: for (unsigned i = 0; i < C_TILE_H; i++) {
//pragma HLS DATAFLOW
#pragma HLS PIPELINE II=C_PIPE_DPTH
					cS_loop: for (unsigned j = 0; j < B_SYS_COL_MUL; j++) {
//#pragma HLS PIPELINE II=1
						sumWord = inFifo.read();
						cV1_loop: for (unsigned k = 0; k < SYSTLC_COL; k++) {
							lineC[j * SYSTLC_COL + k] = halfToClHalf(sumWord.d[k]);
						}
					}
					cW1_loop: for (unsigned j = 0; j < C_TILE_W_WRD; j++) {
//#pragma HLS PIPELINE II=1
						bout_t outWord;
						cV2_loop: for (unsigned k = 0; k < C_VCTR_SIZE; k++) {
							outWord.d[k] = lineC[j * C_VCTR_SIZE + k];
						}
						outFifo.write(outWord);
					}
				}
			} else {
				unsigned col_offset = (b_tCol % B_SYS_COL_MUL) * SYSTLC_COL;
				cH2_loop: for (unsigned i = 0; i < C_TILE_H; i++) {
#pragma HLS PIPELINE II=1
					sumWord = inFifo.read();
					cV3_loop: for (unsigned j = 0; j < SYSTLC_COL; j++) {
						localC[i][col_offset + j] = halfToClHalf(sumWord.d[j]);
					}
				}
				if ((b_tCol % B_SYS_COL_MUL == B_SYS_COL_MUL - 1) || b_tCol == b_tPerRow - 1) {
					cH3_loop: for (unsigned i = 0; i < C_TILE_H; i++) {
						cW2_loop: for (unsigned j = 0; j < C_TILE_W_WRD; j++) {
#pragma HLS PIPELINE II=1
							bout_t outWord;
							cV4_loop: for (unsigned k = 0; k < C_VCTR_SIZE; k++) {
								outWord.d[k] = localC[i][j * C_VCTR_SIZE + k];
							}
							outFifo.write(outWord);
						}
					}
				}
			}
		}
	}
}

/*
 * original arch where a, b are passed to compute each through a single fifo
 */
void mmult_arch_v2( //
		const bina_t *a, //
		const binb_t *b, //
		bout_t *c, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
#pragma HLS INLINE
	hls::stream<bina_t> aFifo;
	hls::stream<binb_t> bFifo;
	hls::stream<bout_t> cFifo;

	read_gmem(a, b, aFifo, bFifo, a_row, a_col, b_col, a_cache, b_trans);
	compute_stream_v2(aFifo, bFifo, cFifo, a_row, a_col, b_col, a_cache, b_trans);
	write_gmem(cFifo, c, a_row, a_col, b_col);
}

/*
 * FADD_LAT,    SYSTLC_ROW, type, clk, BRAM, DSP, FF, LUT
 * 6(actual 5), 8,          half, 5.56, 4, 8, 16, 26
 * 6,           8,          half, 6.27, 4, 8, 15, 26
 * 5,           8,          half, 6.27, 4, 8, 15, 26
 * 7(5),        8,          half, 3.65, 4, 7, 16, 25
 * 7(5),        8,          half, 3.65, 43,7, 18, 22 same as above but datapack reshapeA, reshapeB fifos
 */
void mmult_arch_v3( //
		const bina_t *a, //
		const binb_t *b, //
		bout_t *c, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
#pragma HLS INLINE
	const unsigned A_RSHP_DPTH = A_TILE_W_WRD;
	const unsigned B_RSHP_DPTH = B_TILE_H_WRD;

	hls::stream<bina_t> reshapeA[A_TILE_H];
//pragma HLS DATA_PACK variable=reshapeA
#pragma HLS STREAM variable=reshapeA depth=A_RSHP_DPTH dim=1
	hls::stream<binb_t> reshapeB[B_TILE_W];
//pragma HLS DATA_PACK variable=reshapeB
#pragma HLS STREAM variable=reshapeB depth=B_RSHP_DPTH dim=1
	hls::stream<bout_t> cFifo;
#pragma HLS DATA_PACK variable=cFifo

	read_gmem_buckets(a, b, reshapeA, reshapeB, a_row, a_col, b_col, a_cache, b_trans);
//  compute_stream_v3(reshapeA, reshapeB, cFifo, a_row, a_col, b_col, a_cache, b_trans);
	compute_stream_v3_1(reshapeA, reshapeB, cFifo, a_row, a_col, b_col, a_cache, b_trans);
	write_gmem(cFifo, c, a_row, a_col, b_col);
}

/*
 * FADD_LAT, SYSTLC_ROW, type, clk, BRAM, DSP, FF, LUT
 * 5,        8,          half, 6.27, 12, 8, 15, 27
 * 6,        8,          half, 5.56, 12, 8, 16, 27
 * 6,        8,          half, 5.56, 12, 8, 15, 20 no data-flow
 * 7(5),     8,          half, 3.65, 12, 8, 16, 20 no data-flow, 180MHz P&R
 * 7(5),     8,          half, 5.59, 4, 15, 18, 18 same config as above but in 2017.1, so used fp_struct cast
 * N/A,      8,          half, 4.92, 4, 15, 13, 28 compute v4_1, fixed-point accumulate, stat from 2017.1
 */
void mmult_arch_v4( //
		const bina_t *a, //
		const binb_t *b, //
		bout_t *c, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
#pragma HLS INLINE
	const unsigned A_RSHP_DPTH = A_TILE_H;
	const unsigned B_RSHP_DPTH = B_TILE_W;

	hls::stream<bina_t> reshapeA[A_TILE_W_WRD];
#pragma HLS DATA_PACK variable=reshapeA
#pragma HLS STREAM variable=reshapeA depth=A_RSHP_DPTH dim=1
	hls::stream<binb_t> reshapeB[B_TILE_H_WRD];
#pragma HLS DATA_PACK variable=reshapeB
#pragma HLS STREAM variable=reshapeB depth=B_RSHP_DPTH dim=1
	hls::stream<bout_t> cFifo;
#pragma HLS DATA_PACK variable=cFifo

	read_gmem_buckets_v4(a, b, reshapeA, reshapeB, a_row, a_col, b_col, a_cache, b_trans);
	compute_stream_v4(reshapeA, reshapeB, cFifo, a_row, a_col, b_col, a_cache, b_trans);
//  compute_stream_v4_0_1(reshapeA, reshapeB, cFifo, a_row, a_col, b_col, a_cache, b_trans);
//  compute_stream_v4_1<accm_t>(reshapeA, reshapeB, cFifo, a_row, a_col, b_col, a_cache, b_trans);
	write_gmem(cFifo, c, a_row, a_col, b_col);
}

/*
 * FADD_LAT, SYSTLC_ROW, type, clk, BRAM, DSP, FF, LUT
 * 7(5),	 8,			 half, 4.48, 6,   9,   18, 24
 */
void mmult_arch_v5( //
		const bina_t *a, //
		const binb_t *b, //
		bout_t *c, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
#pragma HLS INLINE

	const unsigned AFIFO_DPTH = 64;
	static hls::stream<bina_t> aFifo("aFifo");
#pragma HLS DATA_PACK variable=aFifo
#pragma HLS STREAM variable=aFifo depth=AFIFO_DPTH dim=1

	static hls::stream<bina_t> aTransFifo("aTransFifo");
#pragma HLS DATA_PACK variable=aTransFifo

	const unsigned BFIFO_DPTH = 64;
	static hls::stream<binb_t> bFifo("bFifo");
#pragma HLS DATA_PACK variable=bFifo
#pragma HLS STREAM variable=bFifo depth=BFIFO_DPTH dim=1

	static hls::stream<binb_t> bTransFifo("bTransFifo");
#pragma HLS DATA_PACK variable=bTransFifo

	static hls::stream<bsysa_t> aSysFifo("aSysFifo");
#pragma HLS DATA_PACK variable=aSysFifo

	static hls::stream<bsysa_t> aCacheFifo("aCacheFifo");
#pragma HLS DATA_PACK variable=aCacheFifo

	static hls::stream<bsysb_t> bSysFifo("bSysFifo");
#pragma HLS DATA_PACK variable=bSysFifo

	const unsigned PSUM_DPTH = B_SYS_COL_MUL;
	static hls::stream<baccm_t> psumFifo[SYSTLC_ROW];
//pragma HLS DATA_PACK variable=psumFifo
#pragma HLS STREAM variable=psumFifo depth=PSUM_DPTH dim=1

	static hls::stream<baccm_t> sumFifo("sumFifo");
#pragma HLS DATA_PACK variable=sumFifo

	const unsigned CFIFO_DPTH = 64;
	static hls::stream<bout_t> cFifo("cFifo");
#pragma HLS DATA_PACK variable=cFifo
#pragma HLS STREAM variable=cFifo depth=CFIFO_DPTH dim=1

	read_gmem(a, b, aFifo, bFifo, a_row, a_col, b_col, a_cache, b_trans);
	raster_trans_A(aFifo, aTransFifo, a_row, a_col, b_col, a_cache);
	raster_trans_B(bFifo, bTransFifo, a_row, a_col, b_col, b_trans);
	word_to_systlc_A(aTransFifo, aSysFifo, a_row, a_col, b_col, a_cache);
	word_to_systlc_B(bTransFifo, bSysFifo, a_row, a_col, b_col, b_trans);
	cache_systlc_A(aSysFifo, aCacheFifo, a_row, a_col, b_col, a_cache);
	compute_stream_v5(aCacheFifo, bSysFifo, psumFifo, a_row, a_col, b_col, a_cache, b_trans);
	accm_stream_v5(psumFifo, sumFifo, a_row, a_col, b_col);
	systlc_to_word_C(sumFifo, cFifo, a_row, a_col, b_col);
	write_gmem(cFifo, c, a_row, a_col, b_col);
}

void mmult_arch_v6( //
		const bina_t *a, //
		const binb_t *b, //
		bout_t *c, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
#pragma HLS INLINE

	//const unsigned AFIFO_DPTH = 16;
	static hls::stream<bina_t> aFifo("aFifo");
#pragma HLS DATA_PACK variable=aFifo
//pragma HLS STREAM variable=aFifo depth=AFIFO_DPTH dim=1

	static hls::stream<bina_t> aTransFifo("aTransFifo");
#pragma HLS DATA_PACK variable=aTransFifo

	//const unsigned BFIFO_DPTH = 16;
	static hls::stream<binb_t> bFifo("bFifo");
#pragma HLS DATA_PACK variable=bFifo
//pragma HLS STREAM variable=bFifo depth=BFIFO_DPTH dim=1

	static hls::stream<binb_t> bTransFifo("bTransFifo");
#pragma HLS DATA_PACK variable=bTransFifo

	static hls::stream<bsysa_t> aSysFifo("aSysFifo");
#pragma HLS DATA_PACK variable=aSysFifo

	static hls::stream<bsysa_t> aCacheFifo("aCacheFifo");
#pragma HLS DATA_PACK variable=aCacheFifo

	static hls::stream<bsysb_t> bSysFifo("bSysFifo");
#pragma HLS DATA_PACK variable=bSysFifo

	//const unsigned PSUM_DPTH = B_SYS_COL_MUL;
	static hls::stream<baccm_t> psumFifo("psumFifo");
#pragma HLS DATA_PACK variable=psumFifo
//pragma HLS STREAM variable=psumFifo depth=PSUM_DPTH dim=1

	static hls::stream<baccm_t> sumFifo("sumFifo");
#pragma HLS DATA_PACK variable=sumFifo

	//const unsigned CFIFO_DPTH = 16;
	static hls::stream<bout_t> cFifo("cFifo");
#pragma HLS DATA_PACK variable=cFifo
//pragma HLS STREAM variable=cFifo depth=CFIFO_DPTH dim=1

	read_gmem(a, b, aFifo, bFifo, a_row, a_col, b_col, a_cache, b_trans);
	raster_trans_A(aFifo, aTransFifo, a_row, a_col, b_col, a_cache);
	raster_trans_B(bFifo, bTransFifo, a_row, a_col, b_col, b_trans);
	word_to_systlc_A(aTransFifo, aSysFifo, a_row, a_col, b_col, a_cache);
	word_to_systlc_B(bTransFifo, bSysFifo, a_row, a_col, b_col, b_trans);
	cache_systlc_A(aSysFifo, aCacheFifo, a_row, a_col, b_col, a_cache);
	compute_stream_v6(aCacheFifo, bSysFifo, psumFifo, a_row, a_col, b_col, a_cache, b_trans);
	accm_stream_v6(psumFifo, sumFifo, a_row, a_col, b_col);
	systlc_to_word_C(sumFifo, cFifo, a_row, a_col, b_col);
	write_gmem(cFifo, c, a_row, a_col, b_col);
}

void mmult_arch_v7( //
		const bina_t *a, //
		const binb_t *b, //
		bout_t *c, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
#pragma HLS INLINE

	static hls::stream<bina_t> aFifo("aFifo");
//#pragma HLS DATA_PACK variable=aFifo
#pragma HLS STREAM variable=aFifo depth=32 dim=1

	static hls::stream<bina_t> aTransFifo("aTransFifo");
#pragma HLS DATA_PACK variable=aTransFifo

	static hls::stream<binb_t> bFifo("bFifo");
//#pragma HLS DATA_PACK variable=bFifo
#pragma HLS STREAM variable=bFifo depth=32 dim=1

	static hls::stream<binb_t> bTransFifo("bTransFifo");
#pragma HLS DATA_PACK variable=bTransFifo

	static hls::stream<bsysa_t> aSysFifo("aSysFifo");
#pragma HLS DATA_PACK variable=aSysFifo

	static hls::stream<bsysa_t> aCacheFifo("aCacheFifo");
#pragma HLS DATA_PACK variable=aCacheFifo

	static hls::stream<bsysb_t> bSysFifo("bSysFifo");
#pragma HLS DATA_PACK variable=bSysFifo

	static hls::stream<baccm_t> psumFifo("psumFifo");
#pragma HLS DATA_PACK variable=psumFifo
//pragma HLS STREAM variable=psumFifo depth=PSUM_DPTH dim=1

	static hls::stream<baccm_t> sumFifo("sumFifo");
#pragma HLS DATA_PACK variable=sumFifo

	static hls::stream<bout_t> cFifo("cFifo");
//#pragma HLS DATA_PACK variable=cFifo
#pragma HLS STREAM variable=cFifo depth=32 dim=1

	read_gmem_v7(a, b, aFifo, bFifo, a_row, a_col, b_col, a_cache, b_trans);
	raster_trans_A_v7(aFifo, aTransFifo, a_row, a_col, b_col, a_cache, b_trans);
	//raster_trans_B_v7(bFifo, bTransFifo, a_row, a_col, b_col, b_trans);
	raster_trans_B_v7_1(bFifo, bTransFifo, a_row, a_col, b_col, b_trans);
	word_to_systlc_A_v7(aTransFifo, aSysFifo, a_row, a_col, b_col, a_cache, b_trans);
	word_to_systlc_B_v7(bTransFifo, bSysFifo, a_row, a_col, b_col, b_trans);
	cache_systlc_A_v7(aSysFifo, aCacheFifo, a_row, a_col, b_col, a_cache, b_trans);
	compute_stream_v7(aCacheFifo, bSysFifo, psumFifo, a_row, a_col, b_col, a_cache, b_trans);
	accm_stream_v7(psumFifo, sumFifo, a_row, a_col, b_col, b_trans);
	systlc_to_word_C_v7(sumFifo, cFifo, a_row, a_col, b_col, b_trans);
	write_gmem(cFifo, c, a_row, a_col, b_col);
}

void compute_dummy( //
        hls::stream<bina_t> &aFifo, //
		hls::stream<binb_t> &bFifo, //
		hls::stream<bout_t> &cFifo, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
	unsigned a_tPerCol = (a_row - 1) / A_TILE_H + 1;
	unsigned a_tPerRow = (a_col - 1) / A_TILE_W + 1;

	unsigned b_tPerRow;
	if (!b_trans)
		b_tPerRow = (b_col - 1) / B_TILE_W + 1;
	else
		b_tPerRow = (b_col - 1) / SYSTLC_COL + 1;

	bina_t localA[A_TILE_H * A_TILE_W_WRD];
	bout_t localC[B_TILE_H * B_TILE_W_WRD];
    bina_t wordA;
    binb_t wordB;
    bout_t wordC;
    bout_t rdWordC;
    bout_t outC;

	a_tr_loop: for (unsigned a_tRow = 0; a_tRow < a_tPerCol; a_tRow++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TROW_TRIP max=A_TROW_TRIP
		b_tc_loop: for (unsigned b_tCol = 0; b_tCol < b_tPerRow; b_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=B_TCOL_TRIP max=B_TCOL_MAX_TRIP
			a_tc_loop: for (unsigned a_tCol = 0; a_tCol < a_tPerRow; a_tCol++) {
#pragma HLS LOOP_TRIPCOUNT min=A_TCOL_TRIP max=A_TCOL_TRIP
                if (!a_cache || b_tCol == 0) {
                    a_loop: for (unsigned beat = 0; beat < A_TILE_H * A_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
                        localA[beat] = aFifo.read();
                    }
                }
                b_loop: for (unsigned beat = 0, aidx = 0; beat < B_TILE_H * B_TILE_W_WRD; beat++) {
#pragma HLS PIPELINE
                    wordB = bFifo.read();
                    wordA = localA[aidx];
                    vctr_loop: for (unsigned d = 0; d < A_VCTR_SIZE; d++) {
                        wordC.d[d] = wordA.d[d] * wordB.d[d];
                    }

                    if (a_tCol == a_tPerRow - 1 && beat < C_TILE_H * C_TILE_W_WRD) {
                        rdWordC = localC[beat];
                        vctr2_loop: for (unsigned d = 0; d < A_VCTR_SIZE; d++) {
                            outC.d[d] = wordC.d[d] * rdWordC.d[d];
                        }
                        cFifo.write(outC);
                    } else {
                        localC[beat] = wordC;
                    }

                    if (aidx == A_TILE_H * A_TILE_W_WRD - 1) {
                        aidx = 0;
                    } else {
                        aidx++;
                    }
                }
            }
        }
    }
}

void mmult_arch_v7_dummy( //
		const bina_t *a, //
		const binb_t *b, //
		bout_t *c, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
#pragma HLS INLINE

	static hls::stream<bina_t> aFifo("aFifo");
//#pragma HLS DATA_PACK variable=aFifo
#pragma HLS STREAM variable=aFifo depth=32 dim=1

	static hls::stream<binb_t> bFifo("bFifo");
//#pragma HLS DATA_PACK variable=bFifo
#pragma HLS STREAM variable=bFifo depth=32 dim=1

//	static hls::stream<bina_t> aTransFifo("aTransFifo");
//#pragma HLS DATA_PACK variable=aTransFifo
//
//	static hls::stream<binb_t> bTransFifo("bTransFifo");
//#pragma HLS DATA_PACK variable=bTransFifo
////#pragma HLS STREAM variable=bFifo depth=32 dim=1
//
//	static hls::stream<bsysa_t> aSysFifo("aSysFifo");
//#pragma HLS DATA_PACK variable=aSysFifo
//
//	static hls::stream<bsysb_t> bSysFifo("bSysFifo");
//#pragma HLS DATA_PACK variable=bSysFifo
//
//	static hls::stream<bsysa_t> aCacheFifo("aCacheFifo");
//#pragma HLS DATA_PACK variable=aCacheFifo
//
//	static hls::stream<baccm_t> psumFifo("psumFifo");
//#pragma HLS DATA_PACK variable=psumFifo
////pragma HLS STREAM variable=psumFifo depth=PSUM_DPTH dim=1
//
//	static hls::stream<baccm_t> sumFifo("sumFifo");
//#pragma HLS DATA_PACK variable=sumFifo

	static hls::stream<bout_t> cFifo("cFifo");
//#pragma HLS DATA_PACK variable=cFifo
#pragma HLS STREAM variable=cFifo depth=32 dim=1

	read_gmem_v7(a, b, aFifo, bFifo, a_row, a_col, b_col, a_cache, b_trans);
	//raster_trans_A_v7(aFifo, aTransFifo, a_row, a_col, b_col, a_cache, b_trans);
	//raster_trans_B_v7(bFifo, bTransFifo, a_row, a_col, b_col, b_trans);
	//raster_trans_B_v7_1(bFifo, bTransFifo, a_row, a_col, b_col, b_trans);
	//word_to_systlc_A_v7(aTransFifo, aSysFifo, a_row, a_col, b_col, a_cache, b_trans);
	//word_to_systlc_B_v7(bTransFifo, bSysFifo, a_row, a_col, b_col, b_trans);
	//cache_systlc_A_v7(aSysFifo, aCacheFifo, a_row, a_col, b_col, a_cache, b_trans);
	//compute_stream_v7(aCacheFifo, bSysFifo, psumFifo, a_row, a_col, b_col, a_cache, b_trans);
	//accm_stream_v7(psumFifo, sumFifo, a_row, a_col, b_col, b_trans);
	//systlc_to_word_C_v7(sumFifo, cFifo, a_row, a_col, b_col, b_trans);
    compute_dummy(aFifo, bFifo, cFifo, a_row, a_col, b_col, a_cache, b_trans);
	write_gmem(cFifo, c, a_row, a_col, b_col);
}

/*
 * a_cache: caching A feasible only if localA fits whole row
 * b_trans: specifies B is transposed in gmem, s.t. we can burst read columns
 */
extern "C" void mmult( //
		const bina_t *a, //
		const binb_t *b, //
		bout_t *c, //
		unsigned a_row, //
		unsigned a_col, //
		unsigned b_col, //
		unsigned char a_cache, //
		unsigned char b_trans) {
#pragma HLS DATA_PACK variable=a
#pragma HLS DATA_PACK variable=b
#pragma HLS DATA_PACK variable=c
#pragma HLS DATAFLOW
#pragma HLS INTERFACE m_axi port=a offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=b offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=c offset=slave bundle=gmem
#pragma HLS INTERFACE s_axilite port=a bundle=control
#pragma HLS INTERFACE s_axilite port=b bundle=control
#pragma HLS INTERFACE s_axilite port=c bundle=control
#pragma HLS INTERFACE s_axilite port=a_row bundle=control
#pragma HLS INTERFACE s_axilite port=a_col bundle=control
#pragma HLS INTERFACE s_axilite port=b_col bundle=control
#pragma HLS INTERFACE s_axilite port=a_cache bundle=control
#pragma HLS INTERFACE s_axilite port=b_trans bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	assert((!b_trans || (b_trans && (B_TILE_H % B_VCTR_SIZE == 0))) && "B_TILE_H should be integer multiple of B_VCTR_SIZE if B is transposed in gmem");
	assert(
			(A_VCTR_SIZE * sizeof(bina_t) == B_VCTR_SIZE * sizeof(binb_t)) && "Width of A and B bus interface has to be the same since they share the same axi4 master interface");
	assert(
			(A_VCTR_SIZE * sizeof(bina_t) == C_VCTR_SIZE * sizeof(bout_t)) && "Width of A and C bus interface has to be the same since they share the same axi4 master interface");
	assert((C_TILE_W % C_VCTR_SIZE == 0) && "C_TILE_W should be integer multiple of C_VCTR_SIZE");

//  mmult_arch_v2(a, b, c, a_row, a_col, b_col, a_cache, b_trans);
//  mmult_arch_v3(a, b, c, a_row, a_col, b_col, a_cache, b_trans);
//	mmult_arch_v4(a, b, c, a_row, a_col, b_col, a_cache, b_trans);
//	mmult_arch_v5(a, b, c, a_row, a_col, b_col, a_cache, b_trans);
//	mmult_arch_v6(a, b, c, a_row, a_col, b_col, a_cache, b_trans);
//	mmult_arch_v7(a, b, c, a_row, a_col, b_col, a_cache, b_trans);
	mmult_arch_v7_dummy(a, b, c, a_row, a_col, b_col, a_cache, b_trans);
}

