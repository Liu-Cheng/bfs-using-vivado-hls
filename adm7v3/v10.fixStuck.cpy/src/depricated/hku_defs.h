
//#define DEBUG

/*
 * ---- Preference macros ---- 
 */

#define A_VCTR_SIZE		32 // maximum is 16 for int/float, 32 for short/half
#define B_VCTR_SIZE		32 // A, B have to be same value if on same gmem
#define C_VCTR_SIZE		32
//#define SYSTLC_ROW	8
#define SYSTLC_ROW		16 // try arch_v5

// since this is equal to AXI4 master burst beat, maximum allowed is 256
// 1beat at 512bit = 64bytes, x16 tile height = 1024bytes, so burst 4beat = 4kbytes tile
// try to use 3 instead of 4 since 4 makes lots of BRAM FIFOs
#define A_TILE_W_WRD 	16

// integer for non-transposed B, 
// any ratio for transposed B, but affects C_TILE_W_WRD so cannot be fraction
//#define B_TILE_W_WRD	1
#define B_TILE_W_WRD	16 // try arch_v5

// at 4kbytes tile, 128 tiles = 512kB cache
// 2 tiles 4 beat burst = 32x 18k BRAM
// 128 tiles 4 beat burst = 272x 18k BRAM, 9 %, max dim 128x4x16=8192 for float, cache enable
// lower to 64 since adding FIFOs made BRAM to 41 %
#define A_LCL_TILES		16

//#define B_SYS_COL_MUL	1
#define B_SYS_COL_MUL	32 // try arch_v5

/*
 * ---- Dependent macros ----
 */
//#define SYSTLC_COL		(B_VCTR_SIZE * B_TILE_W_WRD)

#define A_TILE_H		(SYSTLC_ROW * 1)				// integer multiple of systolic row? TODO			
#define A_TILE_W		(A_VCTR_SIZE * A_TILE_W_WRD)	// multiple of vctr size for wide bus access, _WRD is burst beats
#define B_TILE_H		A_TILE_W						// arbitrary, or multiple of vctr size if B is transposed on host
//#define B_TILE_W		(SYSTLC_COL * 1)				// TODO
#define B_TILE_W		(B_VCTR_SIZE * B_TILE_W_WRD)	// TODO

#define SYSTLC_COL		(B_TILE_W / B_SYS_COL_MUL)

#define A_TILE_WRD		(A_TILE_H * A_TILE_W_WRD)
#define B_TILE_H_WRD	(B_TILE_H / B_VCTR_SIZE)		// used for b_trans case, so multiple words

#define C_TILE_H		A_TILE_H
#define C_TILE_W		B_TILE_W
#define C_TILE_W_WRD	(C_TILE_W / C_VCTR_SIZE)
#define C_TILE_WRD		(C_TILE_H * C_TILE_W_WRD)


/*
 * ---- Utility class verbatim copied from x_hls_utils.h since it cannot be included in kernel for SDAccel ----
 */
#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stdout, ##__VA_ARGS__)
#define DBG_cout std::cout
#else
#define DEBUG_PRINT(...) do {} while (0)
#define DBG_cout 0 && std::cout
#endif


