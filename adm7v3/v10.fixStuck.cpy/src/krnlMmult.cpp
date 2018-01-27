/*
 * Kernel version 8, added support of transA, 
 * computeDummyV8 does matrix addition C = A + B,
 * only supports rowsA == rowsB && colsA == colsB
 */
#define DEBUG

#include "hls_stream.h"
#include "hkuKrnlTypes.h"
#include "hkuCommon.h"

#ifdef VER_2016_1
typedef half32_t busA_t;
typedef half32_t busB_t;
typedef half32_t busC_t;
typedef half datA_t;
typedef half datB_t;
typedef half datC_t;
#else
typedef ushort32_t busA_t;
typedef ushort32_t busB_t;
typedef ushort32_t busC_t;
typedef unsigned short datA_t;
typedef unsigned short datB_t;
typedef unsigned short datC_t;
#endif

// other settings in hkuCommon.h
#define TILE_W_WRD_B        (TILE_W / VCTR_SIZE_B)
#define TILE_W_WRD_C        (TILE_W / VCTR_SIZE_C)
//#define TILE_W_DIV_H      (TILE_W / TILE_H)
#define DATAFLOW_FAC        8
#define SYS_ROW             TILE_H
#define SYS_COL             16
#define TILE_W_DIV_SYS_COL  (TILE_W / SYS_COL)
#define TILE_W_DIV_SYS_ROW  (TILE_W / SYS_ROW)


typedef struct bsysA_struct {
    datA_t d[SYS_ROW];
} bsysA_t;

typedef struct bsysB_struct {
    datB_t d[SYS_COL];
} bsysB_t;

void readA(
        const busA_t *a,
        hls::stream<busA_t> &fifoA,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA, 
        uchar_t transA, // whether A or A^T is pointed to by *a
        uchar_t transB) // whether B or B^T is pointed to by *b
{
    int tileRowsA = (rowsA - 1)/(transA ? TILE_W : TILE_H) + 1;
    int tileColsB = (colsB - 1)/(transB ? TILE_H : TILE_W) + 1;
    int tileColsA = (colsA - 1)/(transA ? TILE_H : TILE_W) + 1;
    int itrMax = tileRowsA * tileColsB * tileColsA;
    int rowWrdA = (colsA - 1)/VCTR_SIZE_A + 1;
    int colWrdA = (rowsA - 1)/VCTR_SIZE_A + 1;

    loop_itr:
    for (int itr = 0, ra = 0, cb = 0, ca = 0; itr < itrMax; itr++) {
        DEBUG_PRINT("readA: ra %d, cb %d, ca %d\n", ra, cb, ca);
        #pragma HLS LOOP_TRIPCOUNT min=1 max=1
        if (!cacheA || cb == 0) {
            loop_i:
            for (int i = 0; i < TILE_H; i++) {
                unsigned locOff;
                if (transA) 
                    locOff = (TILE_H * ca + i) * colWrdA + TILE_W_WRD_A * ra;
                else        
                    locOff = (TILE_H * ra + i) * rowWrdA + TILE_W_WRD_A * ca;
                DEBUG_PRINT("readA loc: ");
                loop_j:
                for (int j = 0; j < TILE_W_WRD_A; j++) {
                    #pragma HLS PIPELINE II=1
                    unsigned loc = locOff + j;
                    busA_t wordA = a[loc];
                    fifoA << wordA;
                    DEBUG_PRINT("%d ", loc);
                }
                DEBUG_PRINT("\n");
            }
        }
        if (cb == tileColsB - 1 && ca == tileColsA - 1) { cb = 0; ca = 0; ra++; }
        else if (ca == tileColsA - 1) { ca = 0; cb++; }
        else { ca++; }
    }
}

void readA_df1(
        hls::stream<int> &rAstrm,
        hls::stream<int> &cBstrm,
        hls::stream<int> &cAstrm,
        hls::stream<int> &cBfwd,
        int itr,
        int df,
        int itrMax,
        uchar_t cacheA,
        uchar_t transA,
        int colWrdA,
        int rowWrdA,
        const busA_t *a,
        busA_t localA[TILE_H * TILE_W_WRD_A])
{
    int cAcpy, cBcpy, rAcpy;
    unsigned locOff, loc;
    busA_t wordA;

    rAstrm >> rAcpy;
    cBstrm >> cBcpy;
    cAstrm >> cAcpy;
    cBfwd << cBcpy;
    DEBUG_PRINT("In DF, rAcpy %d, cBcpy %d, cAcpy %d\n", rAcpy, cBcpy, cAcpy);
    int itrOff = itr + df;

    if ((!cacheA || cBcpy == 0) && (itrOff < itrMax)) {
        loop_i:
        for (int i = 0; i < TILE_H; i++) {
            if (transA) 
                locOff = (TILE_H * cAcpy + i) * colWrdA + TILE_W_WRD_A * rAcpy;
            else        
                locOff = (TILE_H * rAcpy + i) * rowWrdA + TILE_W_WRD_A * cAcpy;
            DEBUG_PRINT("readA loc: ");
            loop_j:
            for (int j = 0; j < TILE_W_WRD_A; j++) {
                #pragma HLS PIPELINE II=1
                loc = locOff + j;
                wordA = a[loc];
                localA[TILE_W_WRD_A*i + j] = wordA;
                DEBUG_PRINT("%d ", loc);
            }
            DEBUG_PRINT("\n");
        }
    }
}

void readA_df2(
        hls::stream<int> &cBfwd,
        int itr,
        int df,
        int itrMax,
        uchar_t cacheA,
        uchar_t transA,
        busA_t localA[TILE_H * TILE_W_WRD_A],
        hls::stream<busA_t> &fifoA)
{
    int cBcpy2;
    busA_t wordA;

    cBfwd >> cBcpy2;
    int itrOff2 = itr + df;

    if ((!cacheA || cBcpy2 == 0) && (itrOff2 < itrMax)) {
        loop2_i:
        for (int i = 0; i < TILE_W_WRD_A; i++) {
            loop2_j:
            for (int j = 0; j < TILE_H; j++) {
                #pragma HLS PIPELINE II=1
                if (transA)
                    wordA = localA[TILE_H*i + j];
                else
                    wordA = localA[TILE_W_WRD_A*j + i]; // to Col major
                fifoA << wordA;
            }
        }
    }
}

void readA_v8_1(
        const busA_t *a,
        hls::stream<busA_t> &fifoA,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA, 
        uchar_t transA, // whether A or A^T is pointed to by *a
        uchar_t transB) // whether B or B^T is pointed to by *b
{
    int tileRowsA = (rowsA - 1)/(transA ? TILE_W : TILE_H) + 1;
    int tileColsB = (colsB - 1)/(transB ? TILE_H : TILE_W) + 1;
    int tileColsA = (colsA - 1)/(transA ? TILE_H : TILE_W) + 1;
    int itrMax = tileRowsA * tileColsB * tileColsA;
    int rowWrdA = (colsA - 1)/VCTR_SIZE_A + 1;
    int colWrdA = (rowsA - 1)/VCTR_SIZE_A + 1;
    busA_t localA[TILE_H * TILE_W_WRD_A];
    DEBUG_PRINT("readA: tileRowsA %d, tileColsB %d, tileColsA %d\n", tileRowsA, tileColsB, tileColsA);

    const int strmDpth = DATAFLOW_FAC;
    hls::stream<int> rAstrm("rAstrm");
    #pragma HLS STREAM variable=rAstrm depth=strmDpth dim=1
    hls::stream<int> cBstrm("cBstrm");
    #pragma HLS STREAM variable=cBstrm depth=strmDpth dim=1
    hls::stream<int> cAstrm("cAstrm");
    #pragma HLS STREAM variable=cAstrm depth=strmDpth dim=1
    hls::stream<int> cBfwd("cBfwd");

    loop_itr:
    for (int itr = 0, ra = 0, cb = 0, ca = 0; itr < itrMax; itr += DATAFLOW_FAC) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=1
        loop_dfinit:
        for (int df = 0; df < DATAFLOW_FAC; df++) {
            #pragma HLS PIPELINE II=1
            rAstrm << ra;
            cBstrm << cb;
            cAstrm << ca;
            if (cb == tileColsB - 1 && ca == tileColsA) { cb = 0; ca = 0; ra++; }
            else if (ca == tileColsA - 1) { ca = 0; cb++; }
            else { ca++; }
        }
        loop_df:
        for (int df = 0; df < DATAFLOW_FAC; df++) {
            #pragma HLS DATAFLOW
            readA_df1(rAstrm, cBstrm, cAstrm, cBfwd, itr, df, itrMax, cacheA, transA, colWrdA, rowWrdA, a, localA);
            readA_df2(cBfwd, itr, df, itrMax, cacheA, transA, localA, fifoA);
        }
    }
}

void readB(
        const busB_t *b,
        hls::stream<busB_t> &fifoB,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t transA, // whether A or A^T is pointed to by *a
        uchar_t transB) // whether B or B^T is pointed to by *b
{
    int tileRowsA = (rowsA - 1)/(transA ? TILE_W : TILE_H) + 1;
    int tileColsB = (colsB - 1)/(transB ? TILE_H : TILE_W) + 1;
    int tileRowsB = (colsA - 1)/(transB ? TILE_W : TILE_H) + 1;
    int itrMax = tileRowsA * tileColsB * tileRowsB;
    int rowWrdB = (colsB - 1)/VCTR_SIZE_B + 1;
    int colWrdB = (colsA - 1)/VCTR_SIZE_B + 1;

    loop_itr:
    for (int itr = 0, ra = 0, cb = 0, rb = 0; itr < itrMax; itr++) {
        DEBUG_PRINT("readB: ra %d, cb %d, rb %d\n", ra, cb, rb);
        #pragma HLS LOOP_TRIPCOUNT min=1 max=1
        loop_i:
        for (int i = 0; i < TILE_H; i++) {
            unsigned locOff;
            if (transB) 
                locOff = (TILE_H * cb + i) * colWrdB + TILE_W_WRD_B * rb;
            else        
                locOff = (TILE_H * rb + i) * rowWrdB + TILE_W_WRD_B * cb;
            DEBUG_PRINT("readB loc: ");
            loop_j:
            for (int j = 0; j < TILE_W_WRD_B; j++) {
                #pragma HLS PIPELINE II=1
                unsigned loc = locOff + j;
                busB_t wordB = b[loc];
                fifoB << wordB;
                DEBUG_PRINT("%d ", loc);
            }
            DEBUG_PRINT("\n");
        }
        if (cb == tileColsB - 1 && rb == tileRowsB - 1) { cb = 0; rb = 0; ra++; }
        else if (rb == tileRowsB - 1) { rb = 0; cb++; }
        else { rb++; }
    }
}

void readB_df1(
        //hls::stream<int> &rAstrm,
        hls::stream<int> &cBstrm,
        hls::stream<int> &rBstrm,
        int itr,
        int df,
        int itrMax,
        uchar_t transB,
        int colWrdB,
        int rowWrdB,
        const busB_t *b,
        busB_t localB[TILE_H * TILE_W_WRD_B])
{
    int rBcpy, cBcpy, rAcpy;
    unsigned locOff, loc;
    busB_t wordB;

    //rAstrm >> rAcpy;
    cBstrm >> cBcpy;
    rBstrm >> rBcpy;
    int itrOff = itr + df;

    if (itrOff < itrMax) {
        loop_i:
        for (int i = 0; i < TILE_H; i++) {
            if (transB) 
                locOff = (TILE_H * cBcpy + i) * colWrdB + TILE_W_WRD_B * rBcpy;
            else        
                locOff = (TILE_H * rBcpy + i) * rowWrdB + TILE_W_WRD_B * cBcpy;
            DEBUG_PRINT("readB loc: ");
            loop_j:
            for (int j = 0; j < TILE_W_WRD_B; j++) {
                #pragma HLS PIPELINE II=1
                loc = locOff + j;
                wordB = b[loc];
                localB[TILE_W_WRD_B*i + j] = wordB;
                DEBUG_PRINT("%d ", loc);
            }
            DEBUG_PRINT("\n");
        }
    }
}

void readB_df2(
        int itr,
        int df,
        int itrMax,
        uchar_t transB,
        busB_t localB[TILE_H * TILE_W_WRD_B],
        hls::stream<busB_t> &fifoB)
{
    busB_t wordB;
    int itrOff2 = itr + df; 

    if (itrOff2 < itrMax) {
        loop2_i:
        for (int i = 0; i < TILE_W_WRD_B; i++) {
            loop2_j:
            for (int j = 0; j < TILE_H; j++) {
                #pragma HLS PIPELINE II=1
                if (transB)
                    wordB = localB[TILE_W_WRD_B*j + i]; // to Col major
                else
                    wordB = localB[TILE_H*i + j];
                fifoB << wordB;
            }
        }
    }
}

void readB_v8_1(
        const busB_t *b,
        hls::stream<busB_t> &fifoB,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t transA, // whether A or A^T is pointed to by *a
        uchar_t transB) // whether B or B^T is pointed to by *b
{
    int tileRowsA = (rowsA - 1)/(transA ? TILE_W : TILE_H) + 1;
    int tileColsB = (colsB - 1)/(transB ? TILE_H : TILE_W) + 1;
    int tileRowsB = (colsA - 1)/(transB ? TILE_W : TILE_H) + 1;
    int itrMax = tileRowsA * tileColsB * tileRowsB;
    int rowWrdB = (colsB - 1)/VCTR_SIZE_B + 1;
    int colWrdB = (colsA - 1)/VCTR_SIZE_B + 1;
    busB_t localB[TILE_H * TILE_W_WRD_B];
    DEBUG_PRINT("readB: tileRowsA %d, tileColsB %d, tileRowsB %d\n", tileRowsA, tileColsB, tileRowsB);

    const int strmDpth = DATAFLOW_FAC;
    //hls::stream<int> rAstrm("rAstrm");
    //pragma HLS STREAM variable=rAstrm depth=strmDpth dim=1
    hls::stream<int> cBstrm("cBstrm");
    #pragma HLS STREAM variable=cBstrm depth=strmDpth dim=1
    hls::stream<int> rBstrm("rBstrm");
    #pragma HLS STREAM variable=rBstrm depth=strmDpth dim=1

    loop_itr:
    for (int itr = 0, ra = 0, cb = 0, rb = 0; itr < itrMax; itr += DATAFLOW_FAC) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=1
        loop_dfinit:
        for (int df = 0; df < DATAFLOW_FAC; df++) {
            #pragma HLS PIPELINE II=1
            //rAstrm << ra;
            cBstrm << cb;
            rBstrm << rb;
            if (cb == tileColsB - 1 && rb == tileRowsB - 1) { cb = 0; rb = 0; ra++; }
            else if (rb == tileRowsB - 1) { rb = 0; cb++; }
            else { rb++; }
        }
        loop_df:
        for (int df = 0; df < DATAFLOW_FAC; df++) {
            #pragma HLS DATAFLOW
            readB_df1(cBstrm, rBstrm, itr, df, itrMax, transB, colWrdB, rowWrdB, b, localB);
            readB_df2(itr, df, itrMax, transB, localB, fifoB);
        }
    }
}

void writeC(
        hls::stream<busC_t> &fifoC,
        busC_t *c,
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsC = (rowsA - 1)/TILE_H + 1;
    int tileColsC = (colsB - 1)/TILE_W + 1;
    int itrMax = tileRowsC * tileColsC;
    int rowWrdC = (colsB - 1)/VCTR_SIZE_C + 1;
    #ifdef DEBUG
    int readCnt = 0;
    #endif

    loop_itr:
    for (int itr = 0, rc = 0, cc = 0; itr < itrMax; itr++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=1
        DEBUG_PRINT("writeC: rc %d, cc %d\n", rc, cc);
        loop_i:
        for (int i = 0; i < TILE_H; i++) {
            unsigned locOff = (TILE_H * rc + i) * rowWrdC + TILE_W_WRD_C * cc;
            DEBUG_PRINT("writeC loc: ");
            loop_j:
            for (int j = 0; j < TILE_W_WRD_C; j++) {
                #pragma HLS PIPELINE II=1
                busC_t wordC;
                #ifdef DEBUG
                DEBUG_PRINT("writeC: readCnt %d\n", ++readCnt);
                #endif
                fifoC >> wordC;
                unsigned loc = locOff + j;
                c[loc] = wordC;
                DEBUG_PRINT("%d ", loc);
            }
            DEBUG_PRINT("\n");
        }
        if (cc == tileColsC - 1) { cc = 0; rc++; }
        else { cc++; }
    }
}

void toSystlcA(
        hls::stream<busA_t> &fifoIn,
        hls::stream<bsysA_t> &fifoOut,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA,
        uchar_t transA)
{
    int tileRowsA = (rowsA - 1)/(transA ? TILE_W : TILE_H) + 1;
    int tileColsB = (colsB - 1)/(transB ? TILE_H : TILE_W) + 1;
    int tileColsA = (colsA - 1)/(transA ? TILE_H : TILE_W) + 1;
    int itrMax = tileRowsA * tileColsB * tileColsA;
    
    datA_t lineA[TILE_W];
    const int partFac1 = VCTR_SIZE_A;
    #pragma HLS ARRAY_PARTITION variable=lineA cyclic factor=partFac1 dim=1

    datA_t localA[TILE_H][VCTR_SIZE_A];
    const int partFac2 = (TILE_H-1)/2 + 1;
    #pragma HLS ARRAY_PARTITION variable=localA cyclic factor=partFac2 dim=1
    #pragma HLS ARRAY_PARTITION variable=localA complete dim=2

    const int pipeDpth = (TILE_W_DIV_SYS_ROW > TILE_W_WRD_A ? TILE_W_DIV_SYS_ROW : TILE_W_WRD_A);

    loop_itr:
    for (int itr = 0, ra = 0, cb = 0, ca = 0; itr < itrMax; itr++) {
        if (!cacheA || cb == 0) {
            if (transA) {
                loopLine_i:
                for (int i = 0; i < TILE_H; i++) {
                    #pragma HLS PIPELINE II=pipeDpth
                    loopWrLine_j:
                    for (int j = 0; j < TILE_W_WRD_A; j++) {
                        busA_t wordA;
                        fifoIn >> wordA;
                        loopWrLine_k:
                        for (int k = 0; k < VCTR_SIZE_A; k++) {
                            lineA[VCTR_SIZE_A*j + k] = wordA.d[k];
                        }
                    }
                    loopRdLine_j:
                    for (int j = 0; j < TILE_W_DIV_SYS_ROW; j++) {
                        bsysA_t sysWordA;
                        loopRdLine_k:
                        for (int k = 0; k < SYS_ROW; k++) {
                            sysWordA.d[k] = lineA[SYS_ROW*j + k];
                        }
                        fifoOut << sysWordA;
                    }
                }
            } else {
                loopBuf_i:
                for (int i = 0; i < TILE_W_WRD_A; i++) {
                    #pragma HLS DATAFLOW
                    loopWrBuf_j:
                    for (int j = 0; j < TILE_H; j++) {
                        #pragma HLS PIPELINE II=1
                        busA_t wordA;
                        fifoIn >> wordA;
                        loopWrBuf_k:
                        for (int k = 0; k < VCTR_SIZE_A; k++) {
                            localA[j][k] = wordA.d[k];
                        }
                    }
                    loopRdBuf_j:
                    for (int j = 0; j < VCTR_SIZE_A; j++) {
                        #pragma HLS PIPELINE II=1
                        bsysA_t sysWordA;
                        loopRdBuf_k: // implies TILE_H == SYS_ROW
                        for (int k = 0; k < SYS_ROW; k++) {
                            sysWordA.d[k] = localA[k][j];
                        }
                        fifoOut << sysWordA;
                    }
                }
            }
        }
        if (cb == tileColsB - 1 && ca == tileColsA - 1) { cb = 0; ca = 0; ra++; }
        else if (ca == tileColsA - 1) { ca = 0; cb++; }
        else { ca++; }
    }
}

void toSystlcB(
        hls::stream<busB_t> &fifoIn,
        hls::stream<bsysB_t> &fifoOut,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t transB)
{
    int tileRowsA = (rowsA - 1)/(transA ? TILE_W : TILE_H) + 1;
    int tileColsB = (colsB - 1)/(transB ? TILE_H : TILE_W) + 1;
    int tileRowsB = (colsA - 1)/(transB ? TILE_W : TILE_H) + 1;
    int itrMax = tileRowsA * tileColsB * tileRowsB;

    datB_t lineB[TILE_W];
    const int partFac1 = VCTR_SIZE_B;
    #pragma HLS ARRAY_PARTITION variable=lineB cyclic factor=partFac1 dim=1

    datB_t localB[TILE_H][VCTR_SIZE_B];
    const int partFac2 = (TILE_H-1)/2 + 1;
    #pragma HLS ARRAY_PARTITION variable=localB cyclic factor=partFac2 dim=1
    #pragma HLS ARRAY_PARTITION variable=localB complete dim=2
    
    const int pipeDpth = (TILE_W_DIV_SYS_COL > TILE_W_WRD_B ? TILE_W_DIV_SYS_COL : TILE_W_WRD_B);

    loop_itr:
    for (int itr = 0; itr < itrMax; itr++) {
        if (transB) {
            loopBuf_i:
            for (int i = 0; i < TILE_W_WRD_B; i++) {
                #pragma HLS DATAFLOW
                loopWrBuf_j:
                for (int j = 0; j < TILE_H; j++) {
                    #pragma HLS PIPELINE II=1
                    busB_t wordB;
                    fifoIn >> wordB;
                    loopWrBuf_k:
                    for (int k = 0; k < VCTR_SIZE_B; k++) {
                        localB[j][k] = wordB.d[k];
                    }
                }
                loopRdBuf_j:
                for (int j = 0; j < VCTR_SIZE_B; j++) {
                    #pragma HLS PIPELINE II=1
                    bsysB_t sysWordB;
                    loopRdBuf_k:
                    for (int k = 0; k < SYS_COL; k++) {
                        sysWordB.d[k] = localB[k][j];
                    }
                    fifoOut << sysWordB;
                }
            }
        } else {
            loopLine_i:
            for (int i = 0; i < TILE_H; i++) {
                #pragma HLS PIPELINE II=pipeDpth
                loopWrLine_j:
                for (int j = 0; j < TILE_W_WRD_B; j++) {
                    busB_t wordB;
                    fifoIn >> wordB;
                    loopWrLine_k:
                    for (int k = 0; k < VCTR_SIZE_B; k++) {
                        lineB[VCTR_SIZE_B*j + k] = wordB.d[k];
                    }
                }
                loopRdLine_j:
                for (int j = 0; j < TILE_W_DIV_SYS_COL; j++) {
                    bsysB_t sysWordB;
                    loopRdLine_k:
                    for (int k = 0; k < SYS_COL; k++) {
                        sysWordB.d[k] = lineB[SYS_COL*j + k];
                    }
                    fifoOut << sysWordB;
                }
            }
        }
    }
}

void computeDummyV8(
        hls::stream<busA_t> &fifoA,
        hls::stream<busB_t> &fifoB,
        hls::stream<busC_t> &fifoC,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA,
        uchar_t transA,
        uchar_t transB)
{
    int tileRowsA = (rowsA - 1)/(transA ? TILE_W : TILE_H) + 1;
    int tileColsB = (colsB - 1)/(transB ? TILE_H : TILE_W) + 1;
    int tileColsA = (colsA - 1)/(transA ? TILE_H : TILE_W) + 1;
    int tileRowsB = (colsA - 1)/(transB ? TILE_W : TILE_H) + 1;
    int itrMax = tileRowsA * tileColsB;
    busA_t localA[TILE_H * TILE_W_WRD_A];
    busB_t localB[TILE_H * TILE_W_WRD_B];
    #ifdef DEBUG
    int writeCnt = 0;
    #endif

    loop_itr:
    for (int itr = 0, ra = 0, cb = 0; itr < itrMax; itr++) {
        DEBUG_PRINT("computeDummyV8: ra %d, cb %d\n", ra, cb);
        /* Only for !transA && !transB case for now */
        loop_ca:
        for (int ca = 0; ca < tileColsA; ca++) {
            loopA_beat:
            for (int beat = 0; beat < TILE_H * TILE_W_WRD_A; beat++) {
                #pragma HLS PIPELINE II=1
                busA_t wordA;
                fifoA >> wordA;
                if (ca == cb)
                    localA[beat] = wordA;
            }
            const int TILE_W_DIV_H = (TILE_W / TILE_H);
            loop_m:
            for (int m = 0; m < TILE_W_DIV_H; m++) {
                int rb = TILE_W_DIV_H * ca + m;
                DEBUG_PRINT("computeDummyV8: ca %d, rb %d\n", ca, rb);
                if (rb < tileRowsB) {
                    loopB_beat:
                    for (int beat = 0; beat < TILE_H * TILE_W_WRD_B; beat++) {
                        #pragma HLS PIPELINE II=1
                        busB_t wordB;
                        fifoB >> wordB;
                        if (rb == ra)
                            localB[beat] = wordB;
                        /* only works assuming rowsA == rowsB, colsA == colsB */
                        if (ca == tileColsA - 1 && rb == tileRowsB - 1) {
                            busA_t wordA = localA[beat];
                            busB_t rdWordB = localB[beat];
                            busC_t wordC;
                            loop_d:
                            for (int d = 0; d < VCTR_SIZE_A; d++) {
                                int i = beat / TILE_W_WRD_B;
                                if (TILE_W*ca+d < colsA && TILE_H*ra+i < rowsA)
                                    wordC.d[d] = wordA.d[d] + rdWordB.d[d];
                                else
                                    wordC.d[d] = 0;
                            }
                            fifoC << wordC;
                            #ifdef DEBUG
                            DEBUG_PRINT("computeDummyV8: writeCnt %d\n", ++writeCnt);
                            #endif
                        }
                    }
                }
            }
        }
        if (cb == tileColsB - 1) { cb = 0; ra++; }
        else { cb++; }
    }
}

void mmultDummyV8(
        const busA_t *a,
        const busB_t *b,
        busC_t *c,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA,
        uchar_t transA,
        uchar_t transB)
{
    #pragma HLS INLINE
    
    static hls::stream<busA_t> fifoA("fifoA");
    #pragma HLS STREAM variable=fifoA depth=16 dim=1
    static hls::stream<busB_t> fifoB("fifoB");
    #pragma HLS STREAM variable=fifoB depth=16 dim=1
    static hls::stream<busC_t> fifoC("fifoC");

    #define USE_DF 1
    #if USE_DF == 0
    readA(a, fifoA, rowsA, colsA, colsB, cacheA, transA, transB);
    readB(b, fifoB, rowsA, colsA, colsB, transA, transB);
    #else
    readA_v8_1(a, fifoA, rowsA, colsA, colsB, cacheA, transA, transB);
    readB_v8_1(b, fifoB, rowsA, colsA, colsB, transA, transB);
    #endif
    computeDummyV8(fifoA, fifoB, fifoC, rowsA, colsA, colsB, cacheA, transA, transB);
    writeC(fifoC, c, rowsA, colsA, colsB);
}

extern "C" void mmult(
        const busA_t *a,
        const busB_t *b,
        busC_t *c,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA,
        uchar_t transA,
        uchar_t transB)
{
    #pragma HLS DATA_PACK variable=a
    #pragma HLS DATA_PACK variable=b
    #pragma HLS DATA_PACK variable=c
    #pragma HLS DATAFLOW
    #pragma HLS INTERFACE m_axi port=a offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=b offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=c offset=slave bundle=gmem0
    #pragma HLS INTERFACE s_axilite port=a bundle=control
    #pragma HLS INTERFACE s_axilite port=b bundle=control
    #pragma HLS INTERFACE s_axilite port=c bundle=control
    #pragma HLS INTERFACE s_axilite port=rowsA bundle=control
    #pragma HLS INTERFACE s_axilite port=colsA bundle=control
    #pragma HLS INTERFACE s_axilite port=colsB bundle=control
    #pragma HLS INTERFACE s_axilite port=cacheA bundle=control
    #pragma HLS INTERFACE s_axilite port=transA bundle=control
    #pragma HLS INTERFACE s_axilite port=transB bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

	mmultDummyV8(a, b, c, rowsA, colsA, colsB, cacheA, transA, transB);
}

