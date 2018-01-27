
//#define DEBUG

#include <assert.h>
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

typedef struct sysA_struct {
    datA_t d[SYS_H];
} sysA_t;

typedef struct sysB_struct {
    datB_t d[SYS_W];
} sysB_t;

typedef struct sysC_struct {
    datC_t d[SYS_H];
} sysC_t;


typedef struct ushort34_struct {
    unsigned short d[34];
} ushort34_t;

#define MAX_STRIDE 4
#define RD_BURST 16
#define MAX_WTILE 8
#define MAX_WDTH (MAX_WTILE * VCTR_SIZE_B)
#define SHFT_WDTH (VCTR_SIZE_B * 2)
#define INBUF_DPTH (MAX_WTILE * RD_BURST)

typedef struct lineBuf_struct {
    datB_t d[MAX_WDTH];
} lineBuf_t;

typedef struct shftReg_struct {
    datB_t d[SHFT_WDTH];
} shftReg_t;


void readA_v10(
        const busA_t *a,
        hls::stream<busA_t> &fifoA,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA)
{
    const int tileRowsA = ((rowsA - 1) >> VCTR_SIZE_A_LOG2) + 1;
    const int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
    loop_cb:
    for (int cb = 0; cb < tileColsB; cb++) {
        loop_ra:
        for (int ra = 0; ra < tileRowsA; ra++) {
            #pragma HLS LOOP_FLATTEN off
            if (!cacheA || cb == 0) {
                loop_ca:
                for (int ca = 0; ca < colsA; ca++) {
                    #pragma HLS PIPELINE II=1
                    unsigned locA = colsA * ra + ca;   
                    busA_t wordA = a[locA];
                    fifoA << wordA;
                } // ca
            }
        } // ra
    } // cb
} // readA_v10

void cacheA_v10(
        hls::stream<busA_t> &fifoIn,
        hls::stream<busA_t> &fifoOut,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheA)
{
    const int tileRowsA = ((rowsA - 1) >> VCTR_SIZE_A_LOG2) + 1;
    const int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
    busA_t wordA;
    busA_t bufA[BUF_SIZE_A];

    loop_cb:
    for (int cb = 0; cb < tileColsB; cb++) {
        loop_ra:
        for (int ra = 0; ra < tileRowsA; ra++) {
            loop_ca:
            for (int ca = 0; ca < colsA; ca++) {
                #pragma HLS LOOP_FLATTEN off
                #pragma HLS PIPELINE II=1
                if (!cacheA || cb == 0) {
                    fifoIn >> wordA;
                    if (cacheA)
                        bufA[ra * colsA + ca] = wordA;
                } else {
                    wordA = bufA[ra * colsA + ca];
                }
                fifoOut << wordA;
            }
        }
    }
} // cacheA_v10


void readB_v10(
        const busB_t *b,
        hls::stream<busB_t> &fifoB,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheB,
        char im2col_mode,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w
        )
{
    const int width_wrd = ((width - 1) >> VCTR_SIZE_B_LOG2) + 1;
    const int output_w_wrd = ((output_w - 1) >> VCTR_SIZE_B_LOG2) + 1;
    const int tileRowsA = ((rowsA - 1) >> VCTR_SIZE_A_LOG2) + 1;

    if (im2col_mode == 1) {
        assert(width <= VCTR_SIZE_B && "im2col mode 1, width must be <= 32");
        n_loop_mode1:
        for (int n = 0; n < number; n++) {
            orow_loop_mode1:
            for (char out_row = 0; out_row < output_h; out_row++) {
                loop_ra_mode1:
                for (int ra = 0; ra < tileRowsA; ra++) {
                    if (!cacheB || ra == 0) {
                        channel_loop_mode1:
                        for (int channel = 0; channel < channels; channel++) {
                            inrow_loop:
                            for (int in_row = 0; in_row < height; in_row++) {
                                #pragma HLS LOOP_FLATTEN off
                                #pragma HLS PIPELINE II=1
                                int rdAddr = (n * channels + channel)* height + in_row;
                                fifoB << b[rdAddr];
                            } // in_row
                        } // channel
                    } // !cacheB || out_row == 0
                } // ra
            } // out_row
        } // n
    } else if (im2col_mode == 2) {
        assert(kernel_h <= RD_BURST && "im2col mode 2, kernel_h must be <= 16");
        n_loop_mode2:
        for (int n = 0; n < number; n++) {
            orow_loop_mode2:
            for (int out_row = 0, in_rOff = -pad_h; out_row < output_h; out_row++, in_rOff += stride_h) {
                otc_loop:
                for (char output_tc = 0; output_tc < output_w_wrd; output_tc++) {
                    loop_ra_mode2:
                    for (int ra = 0; ra < tileRowsA; ra++) {
                        if (!cacheB || ra == 0) {
                            channel_loop_mode2:
                            for (int channel = 0; channel < channels; channel++) {
                                tc_loop:
                                for (char tc = 0; tc < width_wrd; tc++) { 
                                    r_loop:
                                    for (char r = 0; r < kernel_h; r++) {
                                        #pragma HLS LOOP_FLATTEN off
                                        #pragma HLS PIPELINE II=1
                                        int rOff = (in_rOff > 0 ? in_rOff : 0);
                                        int rdAddr = ((n * channels + channel)* width_wrd + tc)* height + rOff + r;
                                        fifoB << b[rdAddr];
                                    } // r
                                } // tc
                            } // channel
                        } // !cacheB || out_row == 0
                    } // ra
                } // output_tc
            } // out_row
        } // n
    } else { // im2col_mode != 1 && != 2
        int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_ra:
            for (int ra = 0; ra < tileRowsA; ra++) {
                #pragma HLS LOOP_FLATTEN off
                if (!cacheB || ra == 0) {
                    loop_rb:
                    for (int rb = 0; rb < colsA; rb++) {
                        #pragma HLS LOOP_FLATTEN off
                        #pragma HLS PIPELINE II=1
                        unsigned locB = colsA * cb + rb;   
                        busB_t wordB = b[locB];
                        fifoB << wordB;
                    } // rb
                } // !cacheB || ra == 0
            } // ra
        } // cb
    } // im2col_mode != 1 && != 2
}

void cacheB_v10(
        hls::stream<busB_t> &fifoIn,
        hls::stream<busB_t> &fifoOut,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheB,
        char im2col_mode,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w
        )
{
    const int width_wrd = ((width - 1) >> VCTR_SIZE_B_LOG2) + 1;
    const int output_w_wrd = ((output_w - 1) >> VCTR_SIZE_B_LOG2) + 1;
    const int tileRowsA = ((rowsA - 1) >> VCTR_SIZE_A_LOG2) + 1;
    busB_t wordB;
    busB_t bufB[BUF_SIZE_B];

    if (im2col_mode == 1) {
        assert(width <= VCTR_SIZE_B && "im2col mode 1, width must be <= 32");
        n_loop_mode1:
        for (int n = 0; n < number; n++) {
            orow_loop_mode1:
            for (char out_row = 0; out_row < output_h; out_row++) {
                loop_ra_mode1:
                for (int ra = 0; ra < tileRowsA; ra++) {
                    channel_loop_mode1:
                    for (int channel = 0; channel < channels; channel++) {
                        inrow_loop:
                        for (int in_row = 0; in_row < height; in_row++) {
                            #pragma HLS LOOP_FLATTEN off
                            #pragma HLS PIPELINE II=1
                            //int rdAddr = (n * channels + channel)* height + in_row;
                            //fifoB << b[rdAddr];
                            if (!cacheB || ra == 0) {
                                fifoIn >> wordB;
                                bufB[channel * height + in_row] = wordB;
                            } else {
                                wordB = bufB[channel * height + in_row];
                            } // !cacheB || out_row == 0
                            fifoOut << wordB;
                        } // in_row
                    } // channel
                } // ra
            } // out_row
        } // n
    } else if (im2col_mode == 2) {
        assert(kernel_h <= RD_BURST && "im2col mode 2, kernel_h must be <= 16");
        n_loop_mode2:
        for (int n = 0; n < number; n++) {
            orow_loop_mode2:
            for (int out_row = 0, in_rOff = -pad_h; out_row < output_h; out_row++, in_rOff += stride_h) {
                otc_loop:
                for (char output_tc = 0; output_tc < output_w_wrd; output_tc++) {
                    loop_ra_mode2:
                    for (int ra = 0; ra < tileRowsA; ra++) {
                        channel_loop_mode2:
                        for (int channel = 0; channel < channels; channel++) {
                            tc_loop:
                            for (char tc = 0; tc < width_wrd; tc++) { 
                                r_loop:
                                for (char r = 0; r < kernel_h; r++) {
                                    #pragma HLS LOOP_FLATTEN off
                                    #pragma HLS PIPELINE II=1
                                    int rOff = (in_rOff > 0 ? in_rOff : 0);
                                    //int rdAddr = ((n * channels + channel)* width_wrd + tc)* height + rOff + r;
                                    //fifoB << b[rdAddr];
                                    int rdAddr = (channel * width_wrd + tc)* height + rOff + r;
                                    if (!cacheB || ra == 0) {
                                        fifoIn >> wordB;
                                        bufB[rdAddr] = wordB;
                                    } else {
                                        wordB = bufB[rdAddr];
                                    } // !cacheB || out_row == 0
                                    fifoOut << wordB;
                                } // r
                            } // tc
                        } // channel
                    } // ra
                } // output_tc
            } // out_row
        } // n
    } else { // im2col_mode != 1 && != 2
        int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_ra:
            for (int ra = 0; ra < tileRowsA; ra++) {
                #pragma HLS LOOP_FLATTEN
                loop_rb:
                for (int rb = 0; rb < colsA; rb++) {
                    #pragma HLS LOOP_FLATTEN off
                    #pragma HLS PIPELINE II=1
                    if (!cacheB || ra == 0) {
                        fifoIn >> wordB;
                        bufB[rb] = wordB;
                    } else {
                        wordB = bufB[rb];
                    } // !cacheB || ra == 0
                    fifoOut << wordB;
                } // rb
            } // ra
        } // cb
    } // im2col_mode != 1 && != 2
} // cacheB_v10


void im2colB_v10(
        hls::stream<busB_t> &fifoIn,
        hls::stream<busB_t> &fifoOut,
        int rowsA,
        int colsA,
        int colsB,
        uchar_t cacheB,
        char im2col_mode,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w
        )
{
    const int width_wrd = ((width - 1) >> VCTR_SIZE_B_LOG2) + 1;
    const int output_w_wrd = ((output_w - 1) >> VCTR_SIZE_B_LOG2) + 1;
    const int tileRowsA = ((rowsA - 1) >> VCTR_SIZE_A_LOG2) + 1;
    const datB_t zero = 0;
    short input_row;
    short input_col;
    busB_t wordB;
    busB_t in_buf[INBUF_DPTH];
    busB_t wrd_col;
    #pragma HLS ARRAY_PARTITION variable=wrd_col complete dim=1
    busB_t wrd_im;
    #pragma HLS ARRAY_PARTITION variable=wrd_im complete dim=1

    if (im2col_mode == 1) {
        assert(height <= INBUF_DPTH && "im2col requires: height <= 128");
        assert(output_w <= VCTR_SIZE_B && "im2col requires: output_w <= 32");
        assert(pad_w >= 0 && pad_w <= 2 && "im2col requires: pad_w >= 0, <= 2");
        assert(dilation_w == 1 && stride_w == 1 && "im2col requires: dilatioin_w == 1, stride_w == 1");
        ushort34_t wrd_im_shft;
        #pragma HLS ARRAY_PARTITION variable=wrd_im_shft complete dim=1
        n_loop_mode1:
        for (int n = 0; n < number; n++) {
            orow_loop_mode1:
            for (char out_row = 0, in_rOff = -pad_h; out_row < output_h; out_row++, in_rOff += stride_h) {
                loop_ra_mode1:
                for (int ra = 0; ra < tileRowsA; ra++) {
                    channel_loop_mode1:
                    for (int channel = 0; channel < channels; channel++) {
                        #pragma HLS LOOP_FLATTEN off
                        inrow_loop:
                        for (int in_row = 0; in_row < height; in_row++) {
                            #pragma HLS PIPELINE II=1
                            fifoIn >> wordB;
                            in_buf[in_row] = wordB;
                        } // in_row
                        input_row = in_rOff;
                        krow_loop_mode1:
                        for (char kernel_row = 0; kernel_row < kernel_h; kernel_row++, input_row++) {
                            if (!(input_row >= 0 && input_row < height)) {
                                i_loop_mode1:
                                for (char i = 0; i < VCTR_SIZE_B; i++) {
                                    #pragma HLS UNROLL
                                    wrd_im.d[i] = zero;
                                }
                            } else {
                                wrd_im = in_buf[input_row];
                            }
                            init_loop:
                            for (char i = 0; i < VCTR_SIZE_B+2; i++) {
                                #pragma HLS UNROLL
                                wrd_im_shft.d[i] = 0;
                            }
                            pad_loop:
                            for (char i = 0; i < VCTR_SIZE_B; i++) {
                                #pragma HLS UNROLL
                                if (i < width) {
                                    if (pad_w == 1)
                                        wrd_im_shft.d[i+1] = wrd_im.d[i];
                                    else if (pad_w == 2)
                                        wrd_im_shft.d[i+2] = wrd_im.d[i];
                                    else
                                        wrd_im_shft.d[i] = wrd_im.d[i];
                                }
                            }
                            kcol_loop_mode1:
                            for (char kernel_col = 0; kernel_col < kernel_w; kernel_col++) {
                                #pragma HLS PIPELINE II=1
                                ocol_loop_mode1:
                                for (char out_col = 0; out_col < VCTR_SIZE_B; out_col++) {
                                    if (out_col < output_w) {
                                        wrd_col.d[out_col] = wrd_im_shft.d[out_col];
                                    }
                                }
                                shft_loop:
                                for (char i = 0; i < VCTR_SIZE_B+2; i++) {
                                    wrd_im_shft.d[i] = (i+1 < VCTR_SIZE_B+2 ? wrd_im_shft.d[i+1] : 0);
                                }
                                fifoOut << wrd_col;
                            } // kernel_col
                        } // kernel_row
                    } // channel
                } // ra
            } // out_row
        } // n
    } else if (im2col_mode == 2) {
        assert(width_wrd <= MAX_WTILE && "im2col requires: width_wrd <= 8");
        assert(kernel_h <= RD_BURST && "im2col requires: kernel_h <= 16");
        assert(dilation_h == 1 && "im2col requires: dilation_h == 1");
        assert(dilation_w == 1 && "im2col requires: dilation_w == 1");
        lineBuf_t line_im;
        #pragma HLS ARRAY_PARTITION variable=line_im cyclic factor=32 dim=1
        shftReg_t stride_im[MAX_STRIDE];
        #pragma HLS ARRAY_PARTITION variable=stride_im complete dim=0
        n_loop_mode2:
        for (int n = 0; n < number; n++) {
            orow_loop_mode2:
            for (int out_row = 0, in_rOff = -pad_h; out_row < output_h; out_row++, in_rOff += stride_h) {
                otc_loop:
                for (char output_tc = 0; output_tc < output_w_wrd; output_tc++) {
                    loop_ra_mode2:
                    for (int ra = 0; ra < tileRowsA; ra++) {
                        channel_loop_mode2:
                        for (int channel = 0; channel < channels; channel++) {
                            #pragma HLS LOOP_FLATTEN off
                            tc_loop:
                            for (char tc = 0; tc < width_wrd; tc++) { 
                                r_loop:
                                for (char r = 0; r < kernel_h; r++) {
                                    #pragma HLS PIPELINE II=1
                                    fifoIn >> wordB;
                                    in_buf[tc * RD_BURST + r] = wordB;
                                }
                            }
                            input_row = in_rOff;
                            krow_loop:
                            for (char kernel_row = 0; kernel_row < kernel_h; kernel_row++, input_row++) {
                                if (!(input_row >= 0 && input_row < height)) {
                                    i_loop_mode2:
                                    for (short i = 0; i < MAX_WDTH; i++) {
                                        #pragma HLS UNROLL factor=32
                                        line_im.d[i] = zero;
                                    }
                                } else {
                                    i_loop2:
                                    for (char i = 0; i < width_wrd; i++) {
                                        #pragma HLS PIPELINE II=1
                                        wrd_im = in_buf[i * RD_BURST + kernel_row];
                                        j_loop2:
                                        for (char j = 0; j < VCTR_SIZE_B; j++) {
                                            if (i * VCTR_SIZE_B + j < width) {
                                                line_im.d[i * VCTR_SIZE_B + j] = wrd_im.d[j];
                                            }
                                        }
                                    }
                                }
                                short lOff = output_tc * 4 * VCTR_SIZE_B;
                                i_loop_stride:
                                for (char i = 0; i < 6; i++) {
                                    #pragma HLS PIPELINE II=1
                                    if (output_tc * 4 + i < MAX_WTILE) {
                                        busB_t wrd_tmp;
                                        j_loop_stride1:
                                        for (char j = 0; j < VCTR_SIZE_B; j++) {
                                            wrd_tmp.d[j] = line_im.d[lOff + i * VCTR_SIZE_B + j];
                                        }
                                        busB_t stride_tmp[4];
                                        k_loop_stride2:
                                        for (char k = 0; k < 4; k++) {
                                            j_loop_stride2:
                                            for (char j = k; j < VCTR_SIZE_B; j+=4) {
                                                stride_tmp[k].d[(j>>2)] = wrd_tmp.d[j];
                                            }
                                        }
                                        k_loop_stride3:
                                        for (char k = 0; k < 4; k++) {
                                            j_loop_stride3:
                                            for (char j = 0; j < (VCTR_SIZE_B>>2); j++) {
                                                stride_im[k].d[i*(VCTR_SIZE_B>>2) + j] = stride_tmp[k].d[j];
                                            }
                                        } // k
                                    } // < MAX_WTILE
                                } // i
                                char sid = (pad_w == 0 ? 0 : (stride_w - pad_w));
                                kcol_loop_mode2:
                                for (char kernel_col = 0; kernel_col < kernel_w; kernel_col++) {
                                    #pragma HLS PIPELINE II=1
                                    ocol_loop_mode2:
                                    for (char out_col = 0; out_col < VCTR_SIZE_B; out_col++) {
                                        if (output_tc * VCTR_SIZE_B + out_col < output_w) {
                                            wrd_col.d[out_col] = 
                                                stride_im[sid].d[out_col];
                                        }
                                    }
                                    i_loop_shft:
                                    for (char i = 0; i < SHFT_WDTH; i++) {
                                        stride_im[sid].d[i] = (i+1 < SHFT_WDTH ? stride_im[sid].d[i+1] : 0);
                                    }
                                    fifoOut << wrd_col;
                                    if (sid == stride_w - 1) sid = 0;
                                    else sid++;
                                } // kernel_col
                            } // kernel_row
                        } // channel
                    } // ra
                } // output_tc
            } // out_row
        } // n
    } else { // im2col_mode != 1 && != 2
        int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
        loop_cb:
        for (int cb = 0; cb < tileColsB; cb++) {
            loop_ra:
            for (int ra = 0; ra < tileRowsA; ra++) {
                //#pragma HLS LOOP_FLATTEN
                loop_rb:
                for (int rb = 0; rb < colsA; rb++) {
                    #pragma HLS LOOP_FLATTEN off
                    #pragma HLS PIPELINE II=1
                    fifoIn >> wordB;
                    fifoOut << wordB;
                } // rb
            } // ra
        } // cb
    } // im2col_mode != 1 && != 2
} // im2colB_v10

void split_parts_A_v10(
        hls::stream<busA_t> &fifoIn,
        hls::stream<sysA_t> fifoOuts0[VCTR_SIZE_A/SYS_H],
        hls::stream<sysA_t> fifoOuts1[VCTR_SIZE_A/SYS_H],
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = ((rowsA - 1) >> VCTR_SIZE_A_LOG2) + 1;
    int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
    busA_t wordIn;

    loop_cb:
    for (int cb = 0; cb < tileColsB; cb++) {
        loop_ra:
        for (int ra = 0; ra < tileRowsA; ra++) {
            loop_ca:
            for (int ca = 0; ca < colsA; ca++) {
                #pragma HLS LOOP_FLATTEN off
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
} // split_parts_A_v10

void split_parts_B_v10(
        hls::stream<busB_t> &fifoIn,
        hls::stream<sysB_t> fifoOuts0[VCTR_SIZE_B/SYS_W],
        hls::stream<sysB_t> fifoOuts1[VCTR_SIZE_B/SYS_W],
        int rowsA,
        int colsA,
        int colsB)
{
    int tileRowsA = ((rowsA - 1) >> VCTR_SIZE_A_LOG2) + 1;
    int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
    busA_t wordIn;

    loop_cb:
    for (int cb = 0; cb < tileColsB; cb++) {
        loop_ra:
        for (int ra = 0; ra < tileRowsA; ra++) {
            loop_ca:
            for (int ca = 0; ca < colsA; ca++) {
                #pragma HLS LOOP_FLATTEN off
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
} // split_parts_B_v10

void compute_v10_int_rowMaj(
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
    int tileRowsA = ((rowsA - 1) >> VCTR_SIZE_A_LOG2) + 1;
    int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
    accm_t last;
    datA_t datA;
    datB_t datB;
    mult_t rmul;
    accm_t radd;
    mult_t castA;
    mult_t castB;
    sysA_t sysWordA;
    sysB_t sysWordB;
    ap_uint<5> shft = rshft;

    accm_t partSum[SYS_H][SYS_W];
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=1
    #pragma HLS ARRAY_PARTITION variable=partSum complete dim=2

    loop_cb:
    for (int cb = 0; cb < tileColsB; cb++) {
        loop_ra:
        for (int ra = 0; ra < tileRowsA; ra++) {
            #pragma HLS LOOP_FLATTEN off
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
                        //DEBUG_PRINT("DEBUG: ra %d, cb %d, k %d, i %d, j %d, elem A %f, elem B %f\n", 
                        //        ra, cb, k, i, j, (float)castDatFromBus(datA), (float)castDatFromBus(datB));
                        castA = castDatFromBus(datA);
                        castB = castDatFromBus(datB);
                        rmul = castA * castB;
                        radd = last + rmul;
                        partSum[i][j] = radd;
                        #ifdef DEBUG
                        if (iOff == 16 && jOff == 0 && i == 6 && j == 0) {
                            DEBUG_PRINT("DEBUG: castA=%d, castB=%d, rmul=%d, radd=%d\n",
                                    castA, castB, rmul, radd);
                        }
                        #endif
                    } // loop_j
                } // loop_i
            } // loop_itr
            sysC_t wordACC;
            #pragma HLS ARRAY_PARTITION variable=wordACC complete dim=1
            accm_t roundFrom;
            datC_t roundTo;
            loopAcc_i:
            for (int i = 0; i < SYS_H; i++) {
                #pragma HLS PIPELINE II=1
                loopAcc_j:
                for (int j = 0; j < SYS_W; j++) {
                    roundFrom = partSum[i][j];
                    //roundTo = RoundAccm(roundFrom);
                    accm_t tmp = roundFrom >> shft;
                    roundTo = RoundAccm(tmp);
                    wordACC.d[j] = roundTo;
                    //DEBUG_PRINT("DEBUG: i %d, j %d, partSum %d, roundTo %d\n", i, j, partSum[i][j], roundTo);
                } // loopAcc_i
                //if (VCTR_SIZE_B * cb + jOff + j < colsB) 
                if (VCTR_SIZE_A * ra + iOff + i < rowsA) {
                    fifoACC << wordACC;
                }
            } // loopAcc_j
        } // loop_cb
    } // loop_ra
} // compute_v10_int_rowMaj


void merge_parts_C_v10(
        hls::stream<sysC_t> fifoCparts[2][2],
        hls::stream<busC_t> &fifoC,
        int rowsA,
        int colsA,
        int colsB)
{
    //int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
    //int colsC = ((colsB - 1)/VCTR_SIZE_B + 1)*VCTR_SIZE_B
    busC_t wordC;
    sysC_t wordPartL;
    sysC_t wordPartM;

    //loop_ra:
    //for (int ra = 0; ra < tileRowsA; ra++) 
    loop_cb:
    for (int cb = 0; cb < tileColsB; cb++) {
        loop_cc:
        //for (int cc = 0, j = 0, idx = 0; cc < colsB; cc++) 
        for (int cc = 0, j = 0, idx = 0; cc < rowsA; cc++) {
            #pragma HLS LOOP_FLATTEN off
            #pragma HLS PIPELINE II=1
            fifoCparts[idx][0] >> wordPartL;
            fifoCparts[idx][1] >> wordPartM;
            loop_i:
            for (int i = 0; i < SYS_W; i++) {
                wordC.d[i] =  wordPartL.d[i];
                wordC.d[SYS_W + i] = wordPartM.d[i];
                //DEBUG_PRINT("DEBUG: cb %d, j %d, wordC.d[%d] %d, wordC.d[%d] %d\n", cb, j, i, wordC.d[i], SYS_W + i, wordC.d[SYS_W + i]);
            }
            fifoC << wordC;
            if (idx == 2 - 1 && j == SYS_H - 1) {
                j = 0; idx = 0;
            } else if (j == SYS_H - 1) {
                j = 0; idx++;
            } else {
                j++;
            }
        }
    }
} // merge_parts_C_v10


void writeC_v10(
        hls::stream<busC_t> &fifoC,
        busC_t *c,
        int rowsA,
        int colsA,
        int colsB)
{
    //int tileRowsA = (rowsA - 1)/VCTR_SIZE_A + 1;
    int tileColsB = ((colsB - 1) >> VCTR_SIZE_B_LOG2) + 1;
    //int colsC = ((colsB - 1)/VCTR_SIZE_B + 1)*VCTR_SIZE_B
    //loop_ra:
    //for (int ra = 0; ra < tileRowsA; ra++) 
    loop_cb:
    for (int cb = 0; cb < tileColsB; cb++) {
        loop_cc:
        //for (int cc = 0; cc < colsB; cc++) 
        for (int cc = 0; cc < rowsA; cc++) {
            #pragma HLS LOOP_FLATTEN off
            #pragma HLS PIPELINE II=1
            //unsigned locC = colsB * ra + cc;
            unsigned locC = rowsA * cb + cc;
            busC_t wordC;
            fifoC >> wordC;
            c[locC] = wordC;
            //DEBUG_PRINT("DEBUG: writeC locC %d\n", locC);
        }
    }
}


void mmultArch_v10(
        const busA_t *a,
        const busB_t *b,
        busC_t *c,
        int rowsA,
        int colsA,
        int colsB,
#ifdef USE_INT
        uchar_t rshft,
#endif
        uchar_t cacheA,
        uchar_t cacheB,
        uchar_t im2col,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w
        )
{
#pragma HLS INLINE
    static hls::stream<busA_t> fifoA("fifoA");
    #pragma HLS STREAM variable=fifoA depth=16 dim=1
    static hls::stream<busB_t> fifoB("fifoB");
    #pragma HLS STREAM variable=fifoB depth=16 dim=1
    static hls::stream<busC_t> fifoC("fifoC");
    #pragma HLS STREAM variable=fifoC depth=16 dim=1
    static hls::stream<busA_t> fifoCacheA("fifoCacheA");
    #pragma HLS STREAM variable=fifoCacheA depth=16 dim=1
    static hls::stream<busB_t> fifoCacheB("fifoCacheB");
    #pragma HLS STREAM variable=fifoCacheB depth=16 dim=1
    static hls::stream<busB_t> fifoIm2colB("fifoIm2colB");
    #pragma HLS STREAM variable=fifoIm2colB depth=16 dim=1
#ifndef USE_INT
    //static hls::stream<baccm_t> fifoACC("fifoACC");
#endif

    char im2col_mode = 0;
    if (im2col == 0) {
        im2col_mode = 0;
    } else if (width <= VCTR_SIZE_B && output_w <= VCTR_SIZE_B && pad_w <= 2 && stride_w == 1) {
        im2col_mode = 1;
    } else if (kernel_h <= RD_BURST && width <= MAX_WDTH && dilation_h == 1 &&
            dilation_w == 1 && pad_h == 0 && pad_w == 0 && stride_w == 4)  {
        im2col_mode = 2;
    } else {
        im2col_mode = -1;
        DEBUG_PRINT("No im2col mode matched.");
    }

    readA_v10(a, fifoA, rowsA, colsA, colsB, cacheA);
    cacheA_v10(fifoA, fifoCacheA, rowsA, colsA, colsB, cacheA);
    readB_v10(b, fifoB, rowsA, colsA, colsB, cacheB,
            im2col_mode,
            number, channels, height, width,
            kernel_h, kernel_w, pad_h, pad_w,
            stride_h, stride_w, dilation_h, dilation_w,
            output_h, output_w
            );
    cacheB_v10(fifoB, fifoCacheB, rowsA, colsA, colsB, cacheB,
            im2col_mode,
            number, channels, height, width,
            kernel_h, kernel_w, pad_h, pad_w,
            stride_h, stride_w, dilation_h, dilation_w,
            output_h, output_w
            );
    im2colB_v10(fifoCacheB, fifoIm2colB, rowsA, colsA, colsB, cacheB,
            im2col_mode,
            number, channels, height, width,
            kernel_h, kernel_w, pad_h, pad_w,
            stride_h, stride_w, dilation_h, dilation_w,
            output_h, output_w
            );

#ifndef USE_INT
    //compute_v9_1(fifoCacheA, fifoB, fifoACC, rowsA, colsA, colsB);
    //accm_v9(fifoACC, fifoC, rowsA, colsA, colsB);
    //writeC_v9(fifoC, c, rowsA, colsA, colsB);
#else
    static hls::stream<sysA_t> fifoAparts0[2];
    #pragma HLS STREAM variable=fifoAparts0 depth=16 dim=1
    static hls::stream<sysA_t> fifoAparts1[2];
    #pragma HLS STREAM variable=fifoAparts1 depth=16 dim=1
    static hls::stream<sysB_t> fifoBparts0[2];
    #pragma HLS STREAM variable=fifoBparts0 depth=16 dim=1
    static hls::stream<sysB_t> fifoBparts1[2];
    #pragma HLS STREAM variable=fifoBparts1 depth=16 dim=1
    static hls::stream<sysC_t> fifoCparts[2][2];
    #pragma HLS STREAM variable=fifoCparts depth=16 dim=1

    split_parts_A_v10(fifoCacheA, fifoAparts0, fifoAparts1, rowsA, colsA, colsB);
    split_parts_B_v10(fifoIm2colB, fifoBparts0, fifoBparts1, rowsA, colsA, colsB);

    compute_v10_int_rowMaj(fifoAparts0[0], fifoBparts0[0], fifoCparts[0][0], 0,     0,     rshft, rowsA, colsA, colsB);
    compute_v10_int_rowMaj(fifoAparts1[0], fifoBparts0[1], fifoCparts[0][1], 0,     SYS_W, rshft, rowsA, colsA, colsB);
    compute_v10_int_rowMaj(fifoAparts0[1], fifoBparts1[0], fifoCparts[1][0], SYS_H, 0,     rshft, rowsA, colsA, colsB);
    compute_v10_int_rowMaj(fifoAparts1[1], fifoBparts1[1], fifoCparts[1][1], SYS_H, SYS_W, rshft, rowsA, colsA, colsB);
    merge_parts_C_v10(fifoCparts, fifoC, rowsA, colsA, colsB);
    writeC_v10(fifoC, c, rowsA, colsA, colsB);
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
        uchar_t cacheA,
        uchar_t cacheB,
        uchar_t im2col,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w
        )
{
#pragma HLS DATAFLOW
    #pragma HLS DATA_PACK variable=a
    #pragma HLS DATA_PACK variable=b
    #pragma HLS DATA_PACK variable=c
    #pragma HLS INTERFACE m_axi port=a offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=b offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=c offset=slave bundle=gmem1
    #pragma HLS INTERFACE s_axilite port=a bundle=control
    #pragma HLS INTERFACE s_axilite port=b bundle=control
    #pragma HLS INTERFACE s_axilite port=c bundle=control
    #pragma HLS INTERFACE s_axilite port=rowsA bundle=control
    #pragma HLS INTERFACE s_axilite port=colsA bundle=control
    #pragma HLS INTERFACE s_axilite port=colsB bundle=control
    #pragma HLS INTERFACE s_axilite port=rshft bundle=control
    #pragma HLS INTERFACE s_axilite port=cacheA bundle=control
    #pragma HLS INTERFACE s_axilite port=cacheB bundle=control
    #pragma HLS INTERFACE s_axilite port=im2col bundle=control
    #pragma HLS INTERFACE s_axilite port=number     bundle=control
    #pragma HLS INTERFACE s_axilite port=channels   bundle=control
    #pragma HLS INTERFACE s_axilite port=height     bundle=control
    #pragma HLS INTERFACE s_axilite port=width      bundle=control
    #pragma HLS INTERFACE s_axilite port=kernel_h   bundle=control
    #pragma HLS INTERFACE s_axilite port=kernel_w   bundle=control
    #pragma HLS INTERFACE s_axilite port=pad_h      bundle=control
    #pragma HLS INTERFACE s_axilite port=pad_w      bundle=control
    #pragma HLS INTERFACE s_axilite port=stride_h   bundle=control
    #pragma HLS INTERFACE s_axilite port=stride_w   bundle=control
    #pragma HLS INTERFACE s_axilite port=dilation_h bundle=control
    #pragma HLS INTERFACE s_axilite port=dilation_w bundle=control
    #pragma HLS INTERFACE s_axilite port=output_h   bundle=control
    #pragma HLS INTERFACE s_axilite port=output_w   bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

	mmultArch_v10(a, b, c, rowsA, colsA, colsB, 
#ifdef USE_INT
            rshft, 
#endif
            cacheA, cacheB, im2col,
            number, channels, height, width,
            kernel_h, kernel_w, pad_h, pad_w,
            stride_h, stride_w, dilation_h, dilation_w,
            output_h, output_w
            );
}

