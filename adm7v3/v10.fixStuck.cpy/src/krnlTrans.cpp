
//#define DEBUG

#include "hls_stream.h"
#include "hkuKrnlTypes.h"
#include "hkuCommon.h"

#define VCTR_SIZE 32
typedef ushort32_t bus_t;
typedef unsigned short dat_t;

void transRead(
        const bus_t *in,
        hls::stream<bus_t> &fifoIn,
        int rowsIn,
        int colsIn,
        uchar_t colMajor)
{
    int tiles;
    int beats;
    bus_t word;

    if (colMajor) {
        tiles = (rowsIn-1)/VCTR_SIZE + 1;
        beats = colsIn;
    } else {
        tiles = (colsIn-1)/VCTR_SIZE + 1;
        beats = rowsIn;
    }

    loop_tr:
    for (int tr = 0; tr < tiles; tr++) {
        loop_c:
        for (int c = 0; c < beats; c++) {
#pragma HLS LOOP_FLATTEN off
            #pragma HLS PIPELINE II=1
            unsigned loc = beats * tr + c;
            word = in[loc];
            fifoIn << word;
        }
    }
}

void transExec(
        hls::stream<bus_t> &fifoIn,
        hls::stream<bus_t> &fifoOut,
        int rowsIn,
        int colsIn,
        uchar_t colMajor)
{
    int tilesIn;
    int tilesOut;
    int beatsIn;
    int beatsOut;
    bus_t wordIn;
    bus_t wordOut;
    dat_t buf[VCTR_SIZE][VCTR_SIZE];
    #pragma HLS ARRAY_PARTITION variable=buf complete dim=0

    if (colMajor) {
        tilesIn = (rowsIn-1)/VCTR_SIZE + 1;
        tilesOut = (colsIn-1)/VCTR_SIZE + 1;
        beatsIn = colsIn;
        beatsOut = rowsIn;
    } else {
        tilesIn = (colsIn-1)/VCTR_SIZE + 1;
        tilesOut = (rowsIn-1)/VCTR_SIZE + 1;
        beatsIn = rowsIn;
        beatsOut = colsIn;
    }

    loop_tr:
    for (int tr = 0; tr < tilesIn; tr++) {
        loop_tc:
        for (int tc = 0; tc < tilesOut; tc++) {
            loop_c:
            for (int c = 0; c < VCTR_SIZE; c++) {
                #pragma HLS PIPELINE II=1
                if (VCTR_SIZE * tc + c < beatsIn) {
                    fifoIn >> wordIn;
                }
                //buf[c] = wordIn;
                loop1_r:
                for (int r = 0; r < VCTR_SIZE; r++) {
                    buf[c][r] = wordIn.d[r];
                }
            }
            loop_r:
            for (int r = 0; r < VCTR_SIZE; r++) {
                #pragma HLS PIPELINE II=1
                loop2_c:
                for (int c = 0; c < VCTR_SIZE; c++) {
                    //wordOut.d[c] = buf[c].d[r];
                	wordOut.d[c] = buf[c][r];
                }
                if (VCTR_SIZE * tr + r < beatsOut) {
                    fifoOut << wordOut;
                }
            }
        }
    }
}

void transWrite(
        hls::stream<bus_t> &fifoOut,
        bus_t *out,
        int rowsIn,
        int colsIn,
        uchar_t colMajor)
{
    int tilesIn;
    int tilesOut;
    int beatsIn;
    int beatsOut;
    int bound;
    bus_t word;

    if (colMajor) {
        tilesIn = (rowsIn-1)/VCTR_SIZE + 1;
        tilesOut = (colsIn-1)/VCTR_SIZE + 1;
        beatsOut = rowsIn;
    } else {
        tilesIn = (colsIn-1)/VCTR_SIZE + 1;
        tilesOut = (rowsIn-1)/VCTR_SIZE + 1;
        beatsOut = colsIn;
    }

    int boundLast = beatsOut;

    loop_tr:
    for (int tr = 0; tr < tilesIn; tr++) {
        loop_tc:
        for (int tc = 0; tc < tilesOut; tc++) {
            if (tr == tilesIn - 1) bound = boundLast;
            else bound = VCTR_SIZE;

            loop_r:
            for (int r = 0; r < bound; r++) {
#pragma HLS LOOP_FLATTEN off
                #pragma HLS PIPELINE II=1
                fifoOut >> word;
                unsigned loc = beatsOut * tc + VCTR_SIZE * tr + r;
                out[loc] = word;
                DEBUG_PRINT("DEBUG: transWrite loc %d, beatsOut %d, tc %d, tr %d, r %d\n", loc, beatsOut, tc, tr, r);
            }
        }
        boundLast -= VCTR_SIZE;
    }
}

extern "C" void trans(
        const bus_t *in,
        bus_t *out,
        int rowsIn,
        int colsIn,
        uchar_t colMajor)
{
#pragma HLS DATAFLOW
    #pragma HLS DATA_PACK variable=in
    #pragma HLS DATA_PACK variable=out
    #pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem0
    #pragma HLS INTERFACE s_axilite port=in bundle=control
    #pragma HLS INTERFACE s_axilite port=out bundle=control
    #pragma HLS INTERFACE s_axilite port=rowsIn bundle=control
    #pragma HLS INTERFACE s_axilite port=colsIn bundle=control
    #pragma HLS INTERFACE s_axilite port=colMajor bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    static hls::stream<bus_t> fifoIn("fifoIn");
    #pragma HLS STREAM variable=fifoIn depth=32 dim=1

    static hls::stream<bus_t> fifoOut("fifoOut");
    #pragma HLS STREAM variable=fifoOut depth=32 dim=1

    transRead(in, fifoIn, rowsIn, colsIn, colMajor);
    transExec(fifoIn, fifoOut, rowsIn, colsIn, colMajor);
    transWrite(fifoOut, out, rowsIn, colsIn, colMajor);
}

