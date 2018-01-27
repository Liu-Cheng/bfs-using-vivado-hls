
//#define DEBUG

#include <assert.h>
#include "hls_stream.h"
#include "ap_int.h"

#include "hkuKrnlTypes.h"
#include "hkuCommon.h"

#define VCTR_SIZE 32
#define VCTR_SIZE_SUB1 (VCTR_SIZE - 1)
#define VCTR_SIZE_LOG2 5
typedef ushort32_t bus_t;
typedef unsigned short dat_t;

typedef struct ushort34_struct {
    unsigned short d[34];
} ushort34_t;

// assume max kernel_h 16, width 256/VCTR_SIZE = 8 words, 16x8=128
//#define BUF_DEPTH 128
//#define BUF_SIZE (VCTR_SIZE * BUF_DEPTH)

//const int output_h = (height + 2 * pad_h
//        - (dilation_h * (kernel_h - 1) + 1)) / stride_h + 1;
//const int output_w = (width + 2 * pad_w 
//        - (dilation_w * (kernel_w - 1) + 1)) / stride_w + 1;

#define MAX_STRIDE 4
#define RD_BURST 16
//#define RD_BURST_SUB1 15

#define MAX_WTILE 8
#define MAX_WDTH (MAX_WTILE * VCTR_SIZE)

typedef struct lineBuf_struct {
    dat_t d[MAX_WDTH];
} lineBuf_t;

//#define STRIDE_TILES (MAX_WTILE / MAX_STRIDE)
//#define SHFT_WDTH (VCTR_SIZE * STRIDE_TILES + 2)
#define SHFT_WDTH (VCTR_SIZE * 2)

typedef struct shftReg_struct {
    dat_t d[SHFT_WDTH];
} shftReg_t;

#define INBUF_DPTH (MAX_WTILE * RD_BURST)

//#define CACHE_DPTH 64
//#define CACHE_DPTH_SUB1 63
//#define CACHE_DPTH_LOG2 6

void im2col_read_v2(
        const bus_t *data_im,
        hls::stream<bus_t> &strm_im,
        const char im2col_mode,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w)
{
    const int width_wrd = ((width - 1) >> VCTR_SIZE_LOG2) + 1;
    const int output_w_wrd = ((output_w - 1) >> VCTR_SIZE_LOG2) + 1;

    if (im2col_mode == 1) {
        assert(width <= VCTR_SIZE && "im2col mode 1, width must be <= 32");
        n_loop_mode1:
        for (int n = 0; n < number; n++) {
            orow_loop_mode1:
            for (char out_row = 0; out_row < output_h; out_row++) {
                channel_loop_mode1:
                for (int channel = 0; channel < channels; channel++) {
                    inrow_loop:
                    for (int in_row = 0; in_row < height; in_row++) {
#pragma HLS LOOP_FLATTEN off
                        // Assuming width <= VCTR_SIZE = 32, just burst read for height cycles
                        #pragma HLS PIPELINE II=1
                        int rdAddr = (n * channels + channel)* height + in_row;
                        //in_buf[in_row] = data_im[rdAddr];
                        strm_im << data_im[rdAddr];
                    } // in_row
                } // channel
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
                                //in_buf[tc * RD_BURST + r] = data_im[rdAddr];
                                strm_im << data_im[rdAddr];
                            } // r
                        } // tc
                    } // channel
                } // output_tc
            } // out_row
        } // n
    } else { // im2col_mode != 1 && != 2
    }
}

void im2col_rearrange_v2(
        hls::stream<bus_t> &strm_im,
        hls::stream<bus_t> &strm_col,
        const char im2col_mode,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w)
{
    const int width_wrd = ((width - 1) >> VCTR_SIZE_LOG2) + 1;
    const int output_w_wrd = ((output_w - 1) >> VCTR_SIZE_LOG2) + 1;
    const dat_t zero = 0;
    short input_row;
    short input_col;

    bus_t in_buf[INBUF_DPTH];
    bus_t wrd_col;
    #pragma HLS ARRAY_PARTITION variable=wrd_col complete dim=1
    bus_t wrd_im;
    #pragma HLS ARRAY_PARTITION variable=wrd_im complete dim=1

    if (im2col_mode == 1) {
        assert(height <= INBUF_DPTH && "im2col requires: height <= 128");
        assert(output_w <= VCTR_SIZE && "im2col requires: output_w <= 32");
        assert(pad_w >= 0 && pad_w <= 2 && "im2col requires: pad_w >= 0, <= 2");
        assert(dilation_w == 1 && stride_w == 1 && "im2col requires: dilatioin_w == 1, stride_w == 1");
        ushort34_t wrd_im_shft;
        #pragma HLS ARRAY_PARTITION variable=wrd_im_shft complete dim=1
        n_loop_mode1:
        for (int n = 0; n < number; n++) {
            orow_loop_mode1:
            for (char out_row = 0, in_rOff = -pad_h; out_row < output_h; out_row++, in_rOff += stride_h) {
                channel_loop_mode1:
                for (int channel = 0; channel < channels; channel++) {
                    inrow_loop:
                    for (int in_row = 0; in_row < height; in_row++) {
                        #pragma HLS PIPELINE II=1
                        //int rdAddr = (n * channels + channel)* height + in_row;
                        //in_buf[in_row] = data_im[rdAddr];
                        bus_t wrd;
                        strm_im >> wrd;
                        in_buf[in_row] = wrd;
                    } // in_row
                    input_row = in_rOff;
                    krow_loop_mode1:
                    for (char kernel_row = 0; kernel_row < kernel_h; kernel_row++, input_row++) {
                        //input_row = -pad_h + kernel_row * dilation_h + out_row * stride_h;
                        if (!(input_row >= 0 && input_row < height)) {
                            i_loop_mode1:
                            for (char i = 0; i < VCTR_SIZE; i++) {
                                #pragma HLS UNROLL
                                wrd_im.d[i] = zero;
                            }
                        } else {
                            wrd_im = in_buf[input_row];
                        }
                        init_loop:
                        for (char i = 0; i < VCTR_SIZE+2; i++) {
                            #pragma HLS UNROLL
                            wrd_im_shft.d[i] = 0;
                        }
                        pad_loop:
                        for (char i = 0; i < VCTR_SIZE; i++) {
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
                            for (char out_col = 0; out_col < VCTR_SIZE; out_col++) {
                                if (out_col < output_w) {
                                    wrd_col.d[out_col] = wrd_im_shft.d[out_col];
                                }
                            }
                            shft_loop:
                            for (char i = 0; i < VCTR_SIZE+2; i++) {
                                wrd_im_shft.d[i] = (i+1 < VCTR_SIZE+2 ? wrd_im_shft.d[i+1] : 0);
                            }
                            strm_col << wrd_col;
                        } // kernel_col
                    } // kernel_row
                } // channel
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
                    channel_loop_mode2:
                    for (int channel = 0; channel < channels; channel++) {
                        tc_loop:
                        for (char tc = 0; tc < width_wrd; tc++) { 
                            // TODO: dont need width_wrd, stride_w+1 wrd is enough
                            // TODO: read kernel_h beat is too much, rely on cache
                            // TODO: read upto kernel_h can be over cl_mem boundary
                            r_loop:
                            for (char r = 0; r < kernel_h; r++) {
                                #pragma HLS PIPELINE II=1
                                bus_t wrd;
                                strm_im >> wrd;
                                in_buf[tc * RD_BURST + r] = wrd;
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
                                    for (char j = 0; j < VCTR_SIZE; j++) {
                                        if (i * VCTR_SIZE + j < width) {
                                            line_im.d[i * VCTR_SIZE + j] = wrd_im.d[j];
                                        }
                                    }
                                }
                            }
                            short lOff = output_tc * 4 * VCTR_SIZE;
                            i_loop_stride:
                            for (char i = 0; i < 6; i++) {
                                #pragma HLS PIPELINE II=1
                                if (output_tc * 4 + i < MAX_WTILE) {
                                    bus_t wrd_tmp;
                                    j_loop_stride1:
                                    for (char j = 0; j < VCTR_SIZE; j++) {
                                        wrd_tmp.d[j] = line_im.d[lOff + i * VCTR_SIZE + j];
                                    }
                                    bus_t stride_tmp[4];
                                    k_loop_stride2:
                                    for (char k = 0; k < 4; k++) {
                                        j_loop_stride2:
                                        for (char j = k; j < VCTR_SIZE; j+=4) {
                                            stride_tmp[k].d[(j>>2)] = wrd_tmp.d[j];
                                        }
                                    }
                                    k_loop_stride3:
                                    for (char k = 0; k < 4; k++) {
                                        j_loop_stride3:
                                        for (char j = 0; j < (VCTR_SIZE>>2); j++) {
                                            stride_im[k].d[i*(VCTR_SIZE>>2) + j] = stride_tmp[k].d[j];
                                        }
                                    }
                                }
                            }
                            char sid = (pad_w == 0 ? 0 : (stride_w - pad_w));
                            kcol_loop_mode2:
                            for (char kernel_col = 0; kernel_col < kernel_w; kernel_col++) {
                                #pragma HLS PIPELINE II=1
                                ocol_loop_mode2:
                                for (char out_col = 0; out_col < VCTR_SIZE; out_col++) {
                                    if (output_tc * VCTR_SIZE + out_col < output_w) {
                                        wrd_col.d[out_col] = 
                                            stride_im[sid].d[out_col];
                                    }
                                }
                                i_loop_shft:
                                for (char i = 0; i < SHFT_WDTH; i++) {
                                    stride_im[sid].d[i] = (i+1 < SHFT_WDTH ? stride_im[sid].d[i+1] : 0);
                                }
                                strm_col << wrd_col;
                                if (sid == stride_w - 1) sid = 0;
                                else sid++;
                            } // kernel_col
                        } // kernel_row
                    } // channel
                } // output_tc
            } // out_row
        } // n
    } else { // im2col_mode != 1 && != 2
    }
}

void im2col_write_v2(
        hls::stream<bus_t> &strm_col,
        bus_t *data_col,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w)
{
    bus_t wrd_col;
    #pragma HLS ARRAY_PARTITION variable=wrd_col complete dim=1

    const int output_w_wrd = ((output_w - 1) >> VCTR_SIZE_LOG2) + 1;

    n_loop:
    for (int n = 0; n < number; n++) {
        orow_loop:
        for (char output_tc = 0; output_tc < output_h * output_w_wrd; output_tc++) {
            channel_loop:
            for (int channel = 0; channel < channels; channel++) {
                k_loop:
                for (short k = 0; k < kernel_h * kernel_w; k++) {
#pragma HLS LOOP_FLATTEN off
                        #pragma HLS PIPELINE II=1
                        strm_col >> wrd_col;
                        int wrAddr = 
                            (((n * output_h * output_w_wrd + output_tc) * channels +  channel) * kernel_h * kernel_w) + k;
                        data_col[wrAddr] = wrd_col;
                } // k
            } // channel
        } // output_tc
    } // n
}


extern "C" void im2col(
        const bus_t *data_im,
        bus_t *data_col,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w)
{
#pragma HLS DATAFLOW
    #pragma HLS DATA_PACK variable=data_im
    #pragma HLS DATA_PACK variable=data_col
    #pragma HLS INTERFACE m_axi port=data_im offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=data_col offset=slave bundle=gmem0
    #pragma HLS INTERFACE s_axilite port=data_im bundle=control
    #pragma HLS INTERFACE s_axilite port=data_col bundle=control
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

    static hls::stream<bus_t> strm_im("strm_im");
    #pragma HLS STREAM variable=strm_im depth=32 dim=1
    static hls::stream<bus_t> strm_col("strm_col");
    #pragma HLS STREAM variable=strm_col depth=32 dim=1

    char im2col_mode = 0;
    if (width <= VCTR_SIZE && output_w <= VCTR_SIZE && pad_w <= 2 && stride_w == 1) {
        im2col_mode = 1;
    } else if (kernel_h <= RD_BURST && width <= MAX_WDTH && dilation_h == 1 &&
            dilation_w == 1 && pad_h == 0 && pad_w == 0 && stride_w == 4)  {
        im2col_mode = 2;
    } else {
        im2col_mode = -1;
        DEBUG_PRINT("No im2col mode matched.");
    }

    im2col_read_v2(
            data_im,
            strm_im,
            im2col_mode,
            number, channels, height, width,
            kernel_h, kernel_w, pad_h, pad_w,
            stride_h, stride_w, dilation_h, dilation_w,
            output_h, output_w);
    im2col_rearrange_v2(
            strm_im,
            strm_col,
            im2col_mode,
            number, channels, height, width,
            kernel_h, kernel_w, pad_h, pad_w,
            stride_h, stride_w, dilation_h, dilation_w,
            output_h, output_w);
    im2col_write_v2(
            strm_col,
            data_col,
            number, channels, height, width,
            kernel_h, kernel_w, pad_h, pad_w,
            stride_h, stride_w, dilation_h, dilation_w,
            output_h, output_w);

} // im2col

//ARGS="--test 2 -imn 100 -imc 1 -imh 28 -imw 28 -kh 5 -kw 5 -ph 0 -pw 0 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # lenet conv1 out_h/w (28-5)/1+1=24
//ARGS="--test 2 -imn 100 -imc 20 -imh 12 -imw 12 -kh 5 -kw 5 -ph 0 -pw 0 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # lenet conv2
//ARGS="--test 2 -imn 100 -imc 3 -imh 32 -imw 32 -kh 5 -kw 5 -ph 2 -pw 2 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # cifar10 conv1 (32+2*2-5)/1+1=32
//ARGS="--test 2 -imn 100 -imc 32 -imh 16 -imw 16 -kh 5 -kw 5 -ph 2 -pw 2 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # cifar10 conv2
//ARGS="--test 2 -imn 100 -imc 32 -imh 8 -imw 8 -kh 5 -kw 5 -ph 2 -pw 2 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # cifar10 conv3
//
//ARGS="--test 2 -imn 50 -imc 3 -imh 227 -imw 227 -kh 11 -kw 11 -ph 0 -pw 0 -sh 4 -sw 4 -dh 1 -dw 1" ./run.sh check # caffenet conv1 (227-11)/4+1=55
//
//ARGS="--test 2 -imn 50 -imc 96 -imh 27 -imw 27 -kh 5 -kw 5 -ph 2 -pw 2 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # caffenet conv2 (27+2*2-5)/1+1=27
//ARGS="--test 2 -imn 50 -imc 256 -imh 13 -imw 13 -kh 3 -kw 3 -ph 1 -pw 1 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # caffenet conv3 (13+2*1-3)/1+1=13
//ARGS="--test 2 -imn 50 -imc 384 -imh 13 -imw 13 -kh 3 -kw 3 -ph 1 -pw 1 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # caffenet conv4, conv5

