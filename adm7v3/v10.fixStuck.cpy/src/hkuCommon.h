/*
 * Only for code snippets used in both host and kernel code
 */
#include <cstdio>
#include <iostream>

// settings for mmult kernel
//deprecated
//#define TILE_H          16
//#define TILE_W_WRD_A    64
//#define VCTR_SIZE_C     32
//#define TILE_W          (TILE_W_WRD_A * VCTR_SIZE_A)
//#define CACHE_TILES_A   4
//#define CACHE_W         (CACHE_TILES_A * TILE_W)

#define VCTR_SIZE_A     32
#define VCTR_SIZE_B     32
#define VCTR_SIZE_A_LOG2 5
#define VCTR_SIZE_B_LOG2 5
#define BUF_SIZE_A      2048
#define BUF_SIZE_B      6144

#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stdout, ##__VA_ARGS__)
#define DBG_cout std::cout
#else
#define DEBUG_PRINT(...) do {} while (0)
#define DBG_cout 0 && std::cout
#endif

