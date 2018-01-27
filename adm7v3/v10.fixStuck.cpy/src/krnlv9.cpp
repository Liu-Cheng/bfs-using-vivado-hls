
//#define DEBUG

#include "hls_stream.h"
#include "ap_int.h"

#include "hkuKrnlTypes.h"
#include "hkuCommon.h"

#ifndef VER_2016_1
#include "hls/utils/x_hls_utils.h"
#define halfToClHalf(x) fp_struct<half>(x).to_int()
#define clHalfToHalf(x) fp_struct<half>(x).to_ieee()
#define floatToClHalf(x) fp_struct<half>((half)(x)).to_int()
#endif

#ifdef USE_INT
typedef short32_t busA_t;
typedef short32_t busB_t;
typedef short32_t busC_t;
typedef short datA_t;
typedef short datB_t;
typedef short datC_t;
#define castDatFromBus(x) x
#define castDatToBus(x) x
typedef int mult_t;
typedef int accm_t;
typedef busC_t baccm_t;

// somehow Xilinx gemm example implied this
//#define RoundAccm(x) ((x>>16)&0xffff)

// variable shift defined in code below, just take the appropriate bits after shift
//#define RoundAccm(x) (x & 0xffff)
#define RoundAccm(x) ((x > 32767)? 32767 : (x < -32768)? -32768 : (x & 0xffff))

#else // !USE_INT

#ifdef VER_2016_1
// Can use half array on top IO port
typedef half32_t busA_t;
typedef half32_t busB_t;
typedef half32_t busC_t;
typedef half datA_t;
typedef half datB_t;
typedef half datC_t;
#define castDatFromBus(x) x
#define castDatToBus(x) x

#else // !VER_2016_1
// Use ushort on top IO port, cast data before use
typedef ushort32_t busA_t;
typedef ushort32_t busB_t;
typedef ushort32_t busC_t;
typedef unsigned short datA_t;
typedef unsigned short datB_t;
typedef unsigned short datC_t;
#define castDatFromBus(x) clHalfToHalf(x)
#define castDatToBus(x) halfToClHalf(x)

#endif // VER_2016_1
typedef half mult_t;
typedef half accm_t;
typedef half32_t baccm_t;

#endif // USE_INT

#define SYS_H       16
#define SYS_W       16

#define SYS_ROWS    (VCTR_SIZE_A/SYS_H)
#define SYS_COLS    (VCTR_SIZE_B/SYS_W)
#define SYS_MULT    (SYS_ROWS*SYS_COLS)
#define mod2(x,y) 	(x & (y-1))

#define FADD_LAT    5
#define TREE_SIZE   (FADD_LAT+1)
#define TREE_FAC    ((TREE_SIZE - 1) / SYS_MULT + 1)

typedef struct sysA_struct {
    datA_t d[SYS_H];
} sysA_t;

typedef struct sysB_struct {
    datB_t d[SYS_W];
} sysB_t;

typedef struct sysC_struct {
    datC_t d[SYS_H];
} sysC_t;

/*
 *
 */
void compute_v9(
        hls::stream<busA_t> &fifoA,
        hls::stream<busB_t> &fifoB,
        hls::stream<baccm_t> &fifoACC,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    busA_t wordA;
    #pragma HLS ARRAY_PARTITION variable=wordA complete dim=1
    busB_t wordB;
    #pragma HLS ARRAY_PARTITION variable=wordB complete dim=1
    accm_t last;
    datA_t datA;
    datB_t datB;
    mult_t rmul;
    //pragma HLS RESOURCE variable=rmul core=HMul_fulldsp
    accm_t radd;
    //pragma HLS RESOURCE variable=radd core=HAddSub_nodsp
    mult_t castA;
    mult_t castB;

    accm_t partSum[SYS_H][SYS_W][SYS_MULT * TREE_FAC];
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=1
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=2

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_itr:
            for (int itr = 0, sr = 0, sc = 0, k = 0; itr < colsA * SYS_MULT; itr++) {
                #pragma HLS PIPELINE II=1
                if (sr == 0 && sc == 0) {
                    fifoA >> wordA;
                    fifoB >> wordB; }
                loop_i:
                for (int i = 0; i < SYS_H; i++) {
                    unsigned global_i = VCTR_SIZE_A * ra + SYS_H * sr + i;
                    loop_j:
                    for (int j = 0; j < SYS_W; j++) {
                        unsigned global_j = VCTR_SIZE_B * cb + SYS_W * sc + j;
                        if (k < TREE_FAC) { 
                            last = 0; 
                        } else { 
                            last = partSum[i][j][itr % (SYS_MULT * TREE_FAC)]; 
                        }

                        if (global_i < rowsA) { 
                            datA = wordA.d[SYS_H * sr + i];
                        } else { 
                            datA = 0; 
                        }
                        if (global_j < colsB) { 
                            datB = wordB.d[SYS_W * sc + j];
                        } else { 
                            datB = 0; 
                        }
                        DEBUG_PRINT("DEBUG: ra %d, cb %d, k %d, sc %d, sr %d, i %d, j %d, elem A %f, elem B %f\n", ra, cb, k, sc, sr, i, j, (float)castDatFromBus(datA), (float)castDatFromBus(datB));
                        castA = castDatFromBus(datA);
                        castB = castDatFromBus(datB);
                        rmul = castA * castB;
                        radd = last + rmul;
                        partSum[i][j][itr % (SYS_MULT * TREE_FAC)] = radd;
                    }
                }
                if (sc == SYS_COLS - 1 && sr ==  SYS_ROWS - 1) {
                    sc = 0; sr = 0; k++;
                } else if (sr == SYS_ROWS - 1) {
                    sr = 0; sc++;
                } else {
                    sr++;
                }
            } // end loop_itr

            #if 1
            baccm_t wordACC;
            loopAcc_f:
            for (int f = 0; f < TREE_FAC; f++) {
                loopAcc_sc:
                for (int sc = 0; sc < SYS_COLS; sc++) {
                    loopAcc_j:
                    for (int j = 0; j < SYS_W; j++) {
                        #pragma HLS PIPELINE II=1
                        loopAcc_sr:
                        for (int sr = 0; sr < SYS_ROWS; sr++) {
                            loopAcc_i:
                            for (int i = 0; i < SYS_H; i++) {
                                wordACC.d[SYS_H*sr+i] = partSum[i][j][SYS_MULT*f+SYS_ROWS*sc+sr];
                            }
                        }
                        fifoACC << wordACC;
                    }
                }
            }
            #endif
        }
    }
}

/*
 *
 */
void compute_v9_1(
        hls::stream<busA_t> &fifoA,
        hls::stream<busB_t> &fifoB,
        hls::stream<baccm_t> &fifoACC,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    busA_t wordA;
    #pragma HLS ARRAY_PARTITION variable=wordA complete dim=1
    busB_t wordB;
    #pragma HLS ARRAY_PARTITION variable=wordB complete dim=1
    accm_t last;
    datA_t datA;
    datB_t datB;
    mult_t rmul;
    //pragma HLS RESOURCE variable=rmul core=HMul_fulldsp
    accm_t radd;
    //pragma HLS RESOURCE variable=radd core=HAddSub_nodsp
    mult_t castA;
    mult_t castB;
    sysA_t sysWordA;
    sysB_t sysWordB;

    accm_t partSum[SYS_H][SYS_W][SYS_MULT * TREE_FAC];
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=1
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=2

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_itr:
            for (int itr = 0, sr = 0, sc = 0, k = 0; itr < colsA * SYS_MULT; itr++) {
                #pragma HLS PIPELINE II=1
                if (sr == 0 && sc == 0) {
                    fifoA >> wordA;
                    fifoB >> wordB; 
                }
                loopA_i:
                for (int i = 0; i < SYS_H; i++)
                    sysWordA.d[i] = wordA.d[SYS_H * sr + i];
                loopB_i:
                for (int i = 0; i < SYS_W; i++)
                    sysWordB.d[i] = wordB.d[SYS_W * sc + i];
                
                loop_i:
                for (int i = 0; i < SYS_H; i++) {
                    unsigned global_i = VCTR_SIZE_A * ra + SYS_H * sr + i;
                    loop_j:
                    for (int j = 0; j < SYS_W; j++) {
                        unsigned global_j = VCTR_SIZE_B * cb + SYS_W * sc + j;
                        if (k < TREE_FAC) { 
                            last = 0; 
                        } else { 
                            last = partSum[i][j][itr % (SYS_MULT * TREE_FAC)]; 
                        }

                        if (global_i < rowsA) { 
                            //datA = wordA.d[SYS_H * sr + i]; 
                            datA = sysWordA.d[i]; 
                        } else { 
                            datA = 0; 
                        }
                        if (global_j < colsB) { 
                            //datB = wordB.d[SYS_W * sc + j];
                            datB = sysWordB.d[j];
                        } else { 
                            datB = 0; 
                        }
                        DEBUG_PRINT("DEBUG: ra %d, cb %d, k %d, sc %d, sr %d, i %d, j %d, elem A %f, elem B %f\n", ra, cb, k, sc, sr, i, j, (float)castDatFromBus(datA), (float)castDatFromBus(datB));
                        castA = castDatFromBus(datA);
                        castB = castDatFromBus(datB);
                        rmul = castA * castB;
                        radd = last + rmul;
                        partSum[i][j][itr % (SYS_MULT * TREE_FAC)] = radd;
                    }
                }
                if (sc == SYS_COLS - 1 && sr ==  SYS_ROWS - 1) {
                    sc = 0; sr = 0; k++;
                } else if (sr == SYS_ROWS - 1) {
                    sr = 0; sc++;
                } else {
                    sr++;
                }
            } // end loop_itr

            #if 1
            baccm_t wordACC;
            loopAcc_f:
            for (int f = 0; f < TREE_FAC; f++) {
                loopAcc_sc:
                for (int sc = 0; sc < SYS_COLS; sc++) {
                    loopAcc_j:
                    for (int j = 0; j < SYS_W; j++) {
                        #pragma HLS PIPELINE II=1
                        loopAcc_sr:
                        for (int sr = 0; sr < SYS_ROWS; sr++) {
                            loopAcc_i:
                            for (int i = 0; i < SYS_H; i++) {
                                wordACC.d[SYS_H*sr+i] = partSum[i][j][SYS_MULT*f+SYS_ROWS*sc+sr];
                            }
                        }
                        fifoACC << wordACC;
                    }
                }
            }
            #endif
        }
    }
}

/*
 *
 */
void compute_v9_1_int(
        hls::stream<busA_t> &fifoA,
        hls::stream<busB_t> &fifoB,
        hls::stream<baccm_t> &fifoACC,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    busA_t wordA;
    #pragma HLS ARRAY_PARTITION variable=wordA complete dim=1
    busB_t wordB;
    #pragma HLS ARRAY_PARTITION variable=wordB complete dim=1
    accm_t last;
    datA_t datA;
    datB_t datB;
    mult_t rmul;
    //pragma HLS RESOURCE variable=rmul latency=2
    accm_t radd;
    //pragma HLS RESOURCE variable=radd core=HAddSub_nodsp
    mult_t castA;
    mult_t castB;
    sysA_t sysWordA;
    sysB_t sysWordB;

#if SYS_MULT != 1
    accm_t partSum[SYS_H][SYS_W][SYS_MULT];
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=1
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=2
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=3
#else
    accm_t partSum[SYS_H][SYS_W];
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=1
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=2
#endif

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
#if SYS_MULT != 1
            loop_itr:
            for (int itr = 0, sr = 0, sc = 0, k = 0; itr < colsA * SYS_MULT; itr++) {
                #pragma HLS PIPELINE II=1
                if (sr == 0 && sc == 0) {
                    fifoA >> wordA;
                    fifoB >> wordB; 
                }
                loopA_i:
                for (int i = 0; i < SYS_H; i++)
                    sysWordA.d[i] = wordA.d[SYS_H * sr + i];
                loopB_i:
                for (int i = 0; i < SYS_W; i++)
                    sysWordB.d[i] = wordB.d[SYS_W * sc + i];
                loop_i:
                for (int i = 0; i < SYS_H; i++) {
                    unsigned global_i = VCTR_SIZE_A * ra + SYS_H * sr + i;
                    loop_j:
                    for (int j = 0; j < SYS_W; j++) {
                        unsigned global_j = VCTR_SIZE_B * cb + SYS_W * sc + j;
                        last = (k == 0 ? 0 : partSum[i][j][mod2(itr, SYS_MULT)]); 
                        datA = (global_i < rowsA ? sysWordA.d[i] : 0); 
                        datB = (global_j < colsB ? sysWordB.d[j] : 0);
                        DEBUG_PRINT("DEBUG: ra %d, cb %d, k %d, sc %d, sr %d, i %d, j %d, elem A %f, elem B %f\n", 
                                ra, cb, k, sc, sr, i, j, (float)castDatFromBus(datA), (float)castDatFromBus(datB));
                        castA = castDatFromBus(datA);
                        castB = castDatFromBus(datB);
                        rmul = castA * castB;
                        radd = last + rmul;
                        partSum[i][j][mod2(itr, SYS_MULT)] = radd;
                    } // loop_j
                } // loop_i
                if (sc == SYS_COLS - 1 && sr ==  SYS_ROWS - 1) {
                    sc = 0; sr = 0; k++;
                } else if (sr == SYS_ROWS - 1) {
                    sr = 0; sc++;
                } else {
                    sr++;
                }
            } // loop_itr
#else // SYS_MULT == 1
            loop_itr:
            for (int itr = 0, k = 0; itr < colsA; itr++, k++) {
                #pragma HLS PIPELINE II=1
                fifoA >> wordA;
                fifoB >> wordB; 
                loopA_i:
                for (int i = 0; i < SYS_H; i++)
                    sysWordA.d[i] = wordA.d[i];
                loopB_i:
                for (int i = 0; i < SYS_W; i++)
                    sysWordB.d[i] = wordB.d[i];
                loop_i:
                for (int i = 0; i < SYS_H; i++) {
                    unsigned global_i = VCTR_SIZE_A * ra + i;
                    loop_j:
                    for (int j = 0; j < SYS_W; j++) {
                        unsigned global_j = VCTR_SIZE_B * cb + j;
                        last = (k == 0 ? 0 : partSum[i][j]); 
                        datA = (global_i < rowsA ? sysWordA.d[i] : 0); 
                        datB = (global_j < colsB ? sysWordB.d[j] : 0);
                        DEBUG_PRINT("DEBUG: ra %d, cb %d, k %d, i %d, j %d, elem A %f, elem B %f\n", 
                                ra, cb, k, sc, sr, i, j, (float)castDatFromBus(datA), (float)castDatFromBus(datB));
                        castA = castDatFromBus(datA);
                        castB = castDatFromBus(datB);
                        rmul = castA * castB;
                        radd = last + rmul;
                        partSum[i][j] = radd;
                    } // loop_j
                } // loop_i
            } // loop_itr
#endif // SYS_MULT == 1
            baccm_t wordACC;
            accm_t roundFrom;
            datC_t roundTo;
            loopAcc_sc:
            for (int sc = 0; sc < SYS_COLS; sc++) {
                loopAcc_j:
                for (int j = 0; j < SYS_W; j++) {
                    #pragma HLS PIPELINE II=1
                    loopAcc_sr:
                    for (int sr = 0; sr < SYS_ROWS; sr++) {
                        loopAcc_i:
                        for (int i = 0; i < SYS_H; i++) {
                            #if SYS_MULT != 1
                            roundFrom = partSum[i][j][SYS_ROWS*sc+sr];
                            #else
                            roundFrom = partSum[i][j];
                            #endif
                            roundTo = RoundAccm(roundFrom);
                            wordACC.d[SYS_H*sr+i] = roundTo;
                        }
                    }
                    if (VCTR_SIZE_B * cb + SYS_W * sc + j < colsB)
                        fifoACC << wordACC;
                }
            }
        }
    }
}

/*
 *
 */
template<int SC_FROM, int SC_TO, int SYS_MULT_P>
void compute_v9_1_int_part(
        hls::stream<busA_t> &fifoA,
        hls::stream<busB_t> &fifoB,
        hls::stream<baccm_t> &fifoACC,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    busA_t wordA;
    #pragma HLS ARRAY_PARTITION variable=wordA complete dim=1
    busB_t wordB;
    #pragma HLS ARRAY_PARTITION variable=wordB complete dim=1
    accm_t last;
    datA_t datA;
    datB_t datB;
    mult_t rmul;
    //pragma HLS RESOURCE variable=rmul latency=2
    accm_t radd;
    //pragma HLS RESOURCE variable=radd core=HAddSub_nodsp
    mult_t castA;
    mult_t castB;
    sysA_t sysWordA;
    sysB_t sysWordB;

#if SYS_MULT_P != 1
    accm_t partSum[SYS_H][SYS_W][SYS_MULT_P];
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=1
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=2
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=3
#else
    accm_t partSum[SYS_H][SYS_W];
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=1
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=2
#endif

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
#if SYS_MULT_P != 1
            loop_itr:
            for (int itr = 0, sr = 0, sc = SC_FROM, k = 0; itr < colsA * SYS_MULT_P; itr++) {
                #pragma HLS PIPELINE II=1
                if (sr == 0 && sc == SC_FROM) {
                    fifoA >> wordA;
                    fifoB >> wordB; 
                }
                loopA_i:
                for (int i = 0; i < SYS_H; i++)
                    sysWordA.d[i] = wordA.d[SYS_H * sr + i];
                loopB_i:
                for (int i = 0; i < SYS_W; i++)
                    sysWordB.d[i] = wordB.d[SYS_W * sc + i];
                loop_i:
                for (int i = 0; i < SYS_H; i++) {
                    unsigned global_i = VCTR_SIZE_A * ra + SYS_H * sr + i;
                    loop_j:
                    for (int j = 0; j < SYS_W; j++) {
                        unsigned global_j = VCTR_SIZE_B * cb + SYS_W * sc + j;
                        last = (k == 0 ? 0 : partSum[i][j][mod2(itr, SYS_MULT_P)]); 
                        datA = (global_i < rowsA ? sysWordA.d[i] : 0); 
                        datB = (global_j < colsB ? sysWordB.d[j] : 0);
                        DEBUG_PRINT("DEBUG: ra %d, cb %d, k %d, sc %d, sr %d, i %d, j %d, elem A %f, elem B %f\n", 
                                ra, cb, k, sc, sr, i, j, (float)castDatFromBus(datA), (float)castDatFromBus(datB));
                        castA = castDatFromBus(datA);
                        castB = castDatFromBus(datB);
                        rmul = castA * castB;
                        radd = last + rmul;
                        partSum[i][j][mod2(itr, SYS_MULT_P)] = radd;
                    } // loop_j
                } // loop_i
                if (sc == SC_TO - 1 && sr ==  SYS_ROWS - 1) {
                    sc = SC_FROM; sr = 0; k++;
                } else if (sr == SYS_ROWS - 1) {
                    sr = 0; sc++;
                } else {
                    sr++;
                }
            } // loop_itr
#else // SYS_MULT == 1
            loop_itr:
            for (int itr = 0, k = 0; itr < colsA; itr++, k++) {
                #pragma HLS PIPELINE II=1
                fifoA >> wordA;
                fifoB >> wordB; 
                loopA_i:
                for (int i = 0; i < SYS_H; i++)
                    sysWordA.d[i] = wordA.d[i];
                loopB_i:
                for (int i = 0; i < SYS_W; i++)
                    sysWordB.d[i] = wordB.d[i];
                loop_i:
                for (int i = 0; i < SYS_H; i++) {
                    unsigned global_i = VCTR_SIZE_A * ra + i;
                    loop_j:
                    for (int j = 0; j < SYS_W; j++) {
                        unsigned global_j = VCTR_SIZE_B * cb + j;
                        last = (k == 0 ? 0 : partSum[i][j]); 
                        datA = (global_i < rowsA ? sysWordA.d[i] : 0); 
                        datB = (global_j < colsB ? sysWordB.d[j] : 0);
                        DEBUG_PRINT("DEBUG: ra %d, cb %d, k %d, i %d, j %d, elem A %f, elem B %f\n", 
                                ra, cb, k, i, j, (float)castDatFromBus(datA), (float)castDatFromBus(datB));
                        castA = castDatFromBus(datA);
                        castB = castDatFromBus(datB);
                        rmul = castA * castB;
                        radd = last + rmul;
                        partSum[i][j] = radd;
                    } // loop_j
                } // loop_i
            } // loop_itr
#endif // SYS_MULT == 1
            baccm_t wordACC;
            #pragma HLS ARRAY_PARTITION variable=wordACC complete dim=1
            accm_t roundFrom;
            datC_t roundTo;
            loopAcc_sc:
            for (int sc = 0; sc < SYS_COLS/2; sc++) {
                loopAcc_j:
                for (int j = 0; j < SYS_W; j++) {
                    #pragma HLS PIPELINE II=1
                    loopAcc_sr:
                    for (int sr = 0; sr < SYS_ROWS; sr++) {
                        loopAcc_i:
                        for (int i = 0; i < SYS_H; i++) {
                            #if SYS_MULT_P != 1
                            roundFrom = partSum[i][j][SYS_ROWS*sc + sr];
                            #else
                            roundFrom = partSum[i][j];
                            #endif
                            roundTo = RoundAccm(roundFrom);
                            wordACC.d[SYS_H*sr+i] = roundTo;
                        } // loopAcc_i
                    } // loopAcc_sr
                    if (VCTR_SIZE_B * cb + SYS_W * (sc + SC_FROM) + j < colsB) {
                        fifoACC << wordACC;
                    }
                } // loopAcc_j
            } // loopAcc_sc
        } // loop_cb
    } // loop_ra
} // compute_v9_1_int_part

void compute_v9_1_int_small(
        hls::stream<sysA_t> &fifoA,
        hls::stream<sysB_t> &fifoB,
        hls::stream<sysC_t> &fifoACC,
        const int iOff,
        const int jOff,
        uchar_t rshft,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    //sysA_t wordA;
    //#pragma HLS ARRAY_PARTITION variable=wordA complete dim=1
    //sysB_t wordB;
    //#pragma HLS ARRAY_PARTITION variable=wordB complete dim=1
    accm_t last;
    datA_t datA;
    datB_t datB;
    mult_t rmul;
    //pragma HLS RESOURCE variable=rmul latency=2
    accm_t radd;
    //pragma HLS RESOURCE variable=radd core=HAddSub_nodsp
    mult_t castA;
    mult_t castB;
    sysA_t sysWordA;
    sysB_t sysWordB;
    ap_uint<5> shft = rshft;

    accm_t partSum[SYS_H][SYS_W];
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=1
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=2

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_itr:
            for (int k = 0; k < colsA; k++) {
                #pragma HLS PIPELINE II=1
                fifoA >> sysWordA;
                fifoB >> sysWordB; 
                loop_i:
                for (int i = 0; i < SYS_H; i++) {
                    unsigned global_i = VCTR_SIZE_A * ra + iOff + i;
                    loop_j:
                    for (int j = 0; j < SYS_W; j++) {
                        unsigned global_j = VCTR_SIZE_B * cb + jOff + j;
                        last = (k == 0 ? 0 : partSum[i][j]); 
                        datA = (global_i < rowsA ? sysWordA.d[i] : 0); 
                        datB = (global_j < colsB ? sysWordB.d[j] : 0);
                        DEBUG_PRINT("DEBUG: ra %d, cb %d, k %d, i %d, j %d, elem A %f, elem B %f\n", 
                                ra, cb, k, i, j, (float)castDatFromBus(datA), (float)castDatFromBus(datB));
                        castA = castDatFromBus(datA);
                        castB = castDatFromBus(datB);
                        rmul = castA * castB;
                        radd = last + rmul;
                        partSum[i][j] = radd;
                    } // loop_j
                } // loop_i
            } // loop_itr
            sysC_t wordACC;
            #pragma HLS ARRAY_PARTITION variable=wordACC complete dim=1
            accm_t roundFrom;
            datC_t roundTo;
            loopAcc_j:
            for (int j = 0; j < SYS_W; j++) {
                #pragma HLS PIPELINE II=1
                loopAcc_i:
                for (int i = 0; i < SYS_H; i++) {
                    roundFrom = partSum[i][j];
                    //roundTo = RoundAccm(roundFrom);
                    accm_t tmp = roundFrom >> shft;
                    roundTo = RoundAccm(tmp);
                    wordACC.d[i] = roundTo;
                } // loopAcc_i
                if (VCTR_SIZE_B * cb + jOff + j < colsB) {
                    fifoACC << wordACC;
                }
            } // loopAcc_j
        } // loop_cb
    } // loop_ra
} // compute_v9_1_int_part

void accm_v9(
        hls::stream<baccm_t> &fifoACC,
        hls::stream<busC_t> &fifoC,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    baccm_t wordACC;
    baccm_t localC[SYS_COLS*SYS_W];
    busC_t wordC;
    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_k:
            for (int k = 0; k < TREE_FAC; k++) {
                loop_j:
                for (int j = 0; j < VCTR_SIZE_B; j++) {
                    #pragma HLS PIPELINE II=1
                    fifoACC >> wordACC;
                    if (k == 0)
                        localC[j] = wordACC;
                    else {
                        loop_i:
                        for (int i = 0; i < VCTR_SIZE_A; i++) {
                            accm_t last = localC[j].d[i];
                            localC[j].d[i] = last + wordACC.d[i];
                        }
                    }
                }
            }
            loopWr_j:
            for (int j = 0; j < VCTR_SIZE_B; j++) {
                #pragma HLS PIPELINE II=1
                loopWr_i:
                for (int i = 0; i < VCTR_SIZE_A; i++) {
                    wordC.d[i] = castDatToBus(localC[j].d[i]);
                }
                if (VCTR_SIZE_B * cb + j < colsB)
                    fifoC << wordC;
            }
        }
    }
}

void readA_v9(
        const busA_t *a,
        hls::stream<busA_t> &fifoA,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            if (!cacheA || cb == 0) {
                loop_ca:
                for (int ca = 0; ca < colsA; ca++) {
                    #pragma HLS PIPELINE II=1
                    unsigned locA = colsA * ra + ca;   
                    busA_t wordA = a[locA];
                    fifoA << wordA;
                    DEBUG_PRINT("DEBUG: readA locA %d\n", locA);
                    DEBUG_PRINT("DEBUG: ");
                    #ifdef DEBUG
                    for (int i = 0; i < VCTR_SIZE_A; i++) {
                        #ifndef USE_INT
                        DEBUG_PRINT("%f ", (float)castDatFromBus(wordA.d[i]));
                        #else
                        DEBUG_PRINT("%d ", wordA.d[i]);
                        #endif
                    }
                    #endif
                    DEBUG_PRINT("\n");
                }
            }
        }
    }
}

void cacheA_v9(
        hls::stream<busA_t> &fifoIn,
        hls::stream<busA_t> &fifoOut,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    busA_t wordA;
    busA_t bufA[BUF_SIZE_A];

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_ca:
            for (int ca = 0; ca < colsA; ca++) {
                #pragma HLS PIPELINE II=1
                if (!cacheA || cb == 0) {
                    fifoIn >> wordA;
                    bufA[ca] = wordA;
                } else {
                    wordA = bufA[ca];
                }
                fifoOut << wordA;
            }
        }
    }
}

template<int cpy>
void copy_parts(
        hls::stream<busA_t> &fifoA,
        hls::stream<busB_t> &fifoB,
        hls::stream<busA_t> fifoAcpy[cpy],
        hls::stream<busB_t> fifoBcpy[cpy],
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    busA_t wordA;
    busB_t wordB;

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_ca:
            for (int ca = 0; ca < colsA; ca++) {
                #pragma HLS PIPELINE II=1
                fifoA >> wordA;
                fifoB >> wordB;
                loop_i:
                for (int i = 0; i < cpy; i++) {
                    fifoAcpy[i] << wordA;
                    fifoBcpy[i] << wordB;
                }
            }
        }
    }
}

void split_parts_A(
        hls::stream<busA_t> &fifoIn,
        hls::stream<sysA_t> fifoOuts0[VCTR_SIZE_A/SYS_H],
        hls::stream<sysA_t> fifoOuts1[VCTR_SIZE_A/SYS_H],
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    busA_t wordIn;

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_ca:
            for (int ca = 0; ca < colsA; ca++) {
                #pragma HLS PIPELINE II=1
                fifoIn >> wordIn;
                loop_j:
                for (int j = 0; j < VCTR_SIZE_A/SYS_H; j++) {
                    sysA_t wordOut;
                    loop_i:
                    for (int i = 0; i < SYS_H; i++) {
                        wordOut.d[i] = wordIn.d[SYS_H * j + i];
                    }
                    fifoOuts0[j] << wordOut;
                    fifoOuts1[j] << wordOut;
                }
            }
        }
    }
}

void split_parts_B(
        hls::stream<busB_t> &fifoIn,
        hls::stream<sysB_t> fifoOuts0[VCTR_SIZE_B/SYS_W],
        hls::stream<sysB_t> fifoOuts1[VCTR_SIZE_B/SYS_W],
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    busA_t wordIn;

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_ca:
            for (int ca = 0; ca < colsA; ca++) {
                #pragma HLS PIPELINE II=1
                fifoIn >> wordIn;
                loop_j:
                for (int j = 0; j < VCTR_SIZE_B/SYS_W; j++) {
                    sysB_t wordOut;
                    loop_i:
                    for (int i = 0; i < SYS_W; i++) {
                        wordOut.d[i] = wordIn.d[SYS_W * j + i];
                    }
                    fifoOuts0[j] << wordOut;
                    fifoOuts1[j] << wordOut;
                }
            }
        }
    }
}

void readB_v9(
        const busB_t *b,
        hls::stream<busB_t> &fifoB,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = (colsB - 1)/VCTR_SIZE_B + 1;
    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            #pragma HLS LOOP_FLATTEN
            loop_rb:
            for (int rb = 0; rb < colsA; rb++) {
                #pragma HLS LOOP_FLATTEN off
                #pragma HLS PIPELINE II=1
                unsigned locB = colsA * cb + rb;   
                busB_t wordB = b[locB];
                fifoB << wordB;
            }
        }
    }
}

void writeC_v9(
        hls::stream<busC_t> &fifoC,
        busC_t *c,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    //int colsC = ((colsB - 1)/VCTR_SIZE_B + 1)*VCTR_SIZE_B
    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cc:
        for (int cc = 0; cc < colsB; cc++) {
            #pragma HLS LOOP_FLATTEN off
            #pragma HLS PIPELINE II=1
            unsigned locC = colsB * ra + cc;
            busC_t wordC;
            fifoC >> wordC;
            c[locC] = wordC;
            DEBUG_PRINT("DEBUG: writeC locC %d\n", locC);
        }
    }
}

template<int parts>
void merge_parts(
        hls::stream<busC_t> fifoCparts[parts],
        hls::stream<busC_t> &fifoC,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    //int colsC = ((colsB - 1)/VCTR_SIZE_B + 1)*VCTR_SIZE_B
    busC_t wordC;

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cc:
        for (int cc = 0, j = 0, idx = 0; cc < colsB; cc++) {
            #pragma HLS LOOP_FLATTEN off
            #pragma HLS PIPELINE II=1
            //unsigned locC = colsB * ra + cc;
            //fifoC >> wordC;
            //c[locC] = wordC;
            //DEBUG_PRINT("DEBUG: writeC locC %d\n", locC);
            fifoCparts[idx] >> wordC;
            fifoC << wordC;
            if (idx == parts - 1 && j == SYS_W - 1) {
                j = 0; idx = 0;
            } else if (j == SYS_W - 1) {
                j = 0; idx++;
            } else {
                j++;
            }
        }
    }
}

void merge_parts_C(
        hls::stream<sysC_t> fifoCparts[2][2],
        hls::stream<busC_t> &fifoC,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    //int colsC = ((colsB - 1)/VCTR_SIZE_B + 1)*VCTR_SIZE_B
    busC_t wordC;
    sysC_t wordPartL;
    sysC_t wordPartM;

    loop_ra:
    for (int ra = 0; ra < tileRowsA; ra++) {
        loop_cc:
        for (int cc = 0, j = 0, idx = 0; cc < colsB; cc++) {
            #pragma HLS LOOP_FLATTEN off
            #pragma HLS PIPELINE II=1
            fifoCparts[0][idx] >> wordPartL;
            fifoCparts[1][idx] >> wordPartM;
            for (int i = 0; i < SYS_H; i++) {
                wordC.d[i] =  wordPartL.d[i];
                wordC.d[SYS_H + i] = wordPartM.d[i];
            }
            fifoC << wordC;
            if (idx == 2 - 1 && j == SYS_W - 1) {
                j = 0; idx = 0;
            } else if (j == SYS_W - 1) {
                j = 0; idx++;
            } else {
                j++;
            }
        }
    }
}

void mmultArch_v9(
        const busA_t *a,
        const busB_t *b,
        busC_t *c,
        int rowsA,
        int colsA,
        int colsB,
#ifdef USE_INT
        uchar_t rshft,
#endif
        uchar_t cacheA)
{
#pragma HLS INLINE
    static hls::stream<busA_t> fifoA("fifoA");
    #pragma HLS STREAM variable=fifoA depth=32 dim=1
    static hls::stream<busB_t> fifoB("fifoB");
    #pragma HLS STREAM variable=fifoB depth=32 dim=1
    static hls::stream<busC_t> fifoC("fifoC");
    #pragma HLS STREAM variable=fifoC depth=16 dim=1
    static hls::stream<busA_t> fifoCacheA("fifoCacheA");

#ifndef USE_INT
    static hls::stream<baccm_t> fifoACC("fifoACC");
#endif

    readA_v9(a, fifoA, rowsA, colsA, colsB, cacheA);
    readB_v9(b, fifoB, rowsA, colsA, colsB);
    cacheA_v9(fifoA, fifoCacheA, rowsA, colsA, colsB, cacheA);
    //compute_v9(fifoCacheA, fifoB, fifoACC, rowsA, colsA, colsB);
#ifndef USE_INT
    compute_v9_1(fifoCacheA, fifoB, fifoACC, rowsA, colsA, colsB);
    accm_v9(fifoACC, fifoC, rowsA, colsA, colsB);
    writeC_v9(fifoC, c, rowsA, colsA, colsB);
#else
    #if 0
    compute_v9_1_int(fifoCacheA, fifoB, fifoC, rowsA, colsA, colsB);
    writeC_v9(fifoC, c, rowsA, colsA, colsB);
    #elif 0
    static hls::stream<busA_t> fifoAcpy[2];
    static hls::stream<busB_t> fifoBcpy[2];
    static hls::stream<busC_t> fifoCparts[2];
    #pragma HLS STREAM variable=fifoCparts depth=16 dim=1
    copy_parts<2>(fifoCacheA, fifoB, fifoAcpy, fifoBcpy, rowsA, colsA, colsB);
    compute_v9_1_int_part<0, 1, SYS_MULT/2>(fifoAcpy[0], fifoBcpy[0], fifoCparts[0], rowsA, colsA, colsB);
    compute_v9_1_int_part<1, 2, SYS_MULT/2>(fifoAcpy[1], fifoBcpy[1], fifoCparts[1], rowsA, colsA, colsB);
    merge_parts<2>(fifoCparts, fifoC, rowsA, colsA, colsB);
    writeC_v9(fifoC, c, rowsA, colsA, colsB);
    #else
    static hls::stream<sysA_t> fifoAparts0[2];
    #pragma HLS STREAM variable=fifoAparts0 depth=2 dim=1
    static hls::stream<sysA_t> fifoAparts1[2];
    #pragma HLS STREAM variable=fifoAparts1 depth=2 dim=1
    static hls::stream<sysB_t> fifoBparts0[2];
    #pragma HLS STREAM variable=fifoBparts0 depth=2 dim=1
    static hls::stream<sysB_t> fifoBparts1[2];
    #pragma HLS STREAM variable=fifoBparts1 depth=2 dim=1
    static hls::stream<sysC_t> fifoCparts[2][2];
    #pragma HLS STREAM variable=fifoCparts depth=16 dim=1
    split_parts_A(fifoCacheA, fifoAparts0, fifoAparts1, rowsA, colsA, colsB);
    split_parts_B(fifoB, fifoBparts0, fifoBparts1, rowsA, colsA, colsB);
    compute_v9_1_int_small(fifoAparts0[0], fifoBparts0[0], fifoCparts[0][0], 0,     0,     rshft, rowsA, colsA, colsB);
    compute_v9_1_int_small(fifoAparts1[0], fifoBparts0[1], fifoCparts[0][1], 0,     SYS_W, rshft, rowsA, colsA, colsB);
    compute_v9_1_int_small(fifoAparts0[1], fifoBparts1[0], fifoCparts[1][0], SYS_H, 0,     rshft, rowsA, colsA, colsB);
    compute_v9_1_int_small(fifoAparts1[1], fifoBparts1[1], fifoCparts[1][1], SYS_H, SYS_W, rshft, rowsA, colsA, colsB);
    merge_parts_C(fifoCparts, fifoC, rowsA, colsA, colsB);
    writeC_v9(fifoC, c, rowsA, colsA, colsB);
    #endif
#endif
}

extern "C" void mmult(
        const busA_t *a,
        const busB_t *b,
        busC_t *c,
        int rowsA,
        int colsA,
        int colsB,
#ifdef USE_INT
        uchar_t rshft,
#endif
        uchar_t cacheA)
{
#pragma HLS DATAFLOW
    #pragma HLS DATA_PACK variable=a
    #pragma HLS DATA_PACK variable=b
    #pragma HLS DATA_PACK variable=c
    #pragma HLS INTERFACE m_axi port=a offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=b offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=c offset=slave bundle=gmem0
    #pragma HLS INTERFACE s_axilite port=a bundle=control
    #pragma HLS INTERFACE s_axilite port=b bundle=control
    #pragma HLS INTERFACE s_axilite port=c bundle=control
    #pragma HLS INTERFACE s_axilite port=rowsA bundle=control
    #pragma HLS INTERFACE s_axilite port=colsA bundle=control
    #pragma HLS INTERFACE s_axilite port=colsB bundle=control
    #pragma HLS INTERFACE s_axilite port=rshft bundle=control
    #pragma HLS INTERFACE s_axilite port=cacheA bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

	mmultArch_v9(a, b, c, rowsA, colsA, colsB, rshft, cacheA);
}

