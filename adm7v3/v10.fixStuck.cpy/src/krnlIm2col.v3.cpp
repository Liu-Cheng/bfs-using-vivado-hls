
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
#define KH_MAX 16

//const int output_h = (height + 2 * pad_h
//        - (dilation_h * (kernel_h - 1) + 1)) / stride_h + 1;
//const int output_w = (width + 2 * pad_w 
//        - (dilation_w * (kernel_w - 1) + 1)) / stride_w + 1;


#define CACHE_DPTH 64
#define CACHE_DPTH_SUB1 63
#define CACHE_DPTH_LOG2 6

void cache_im(
        int addr, 
        const bus_t *data_im, 
        bus_t &wrdOut,
        //int prev_addr[CACHE_DPTH], 
        ap_int<27> prev_addr[CACHE_DPTH], 
        bus_t cache[CACHE_DPTH]) 
{
#pragma HLS INLINE
    bus_t rdWrd;
    #pragma HLS ARRAY_PARTITION variable=rdWrd complete dim=1

    if (prev_addr[(addr & CACHE_DPTH_SUB1)] == (addr >> CACHE_DPTH_LOG2)) {
        wrdOut = cache[(addr & CACHE_DPTH_SUB1)];
    } else {
        rdWrd = data_im[addr];
        wrdOut = cache[(addr & CACHE_DPTH_SUB1)] = rdWrd;
        prev_addr[(addr & CACHE_DPTH_SUB1)] = (addr >> CACHE_DPTH_LOG2);
        DEBUG_PRINT("fetch addr=%d from global data_im\n", addr);
    }
}

void im2col_read(
        const bus_t *data_im,
        hls::stream<bus_t> &strm_col,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w)
{
    const int out_itrs = output_h * output_w;
    const int out_tiles = ((out_itrs - 1) >> VCTR_SIZE_LOG2) + 1;
    //const int out_tiles = output_h * (((output_w - 1) >> VCTR_SIZE_LOG2) + 1);
    const int width_wrd = ((width - 1) >> VCTR_SIZE_LOG2) + 1;
    const int channel_size = height * width_wrd; // channel size in words for bus addr_im

    int addr_im = 0; // in word
    unsigned addr_col = 0; // in elem
    bus_t wrd_col;
    #pragma HLS ARRAY_PARTITION variable=wrd_col complete dim=1
    bus_t wrd_im;
    #pragma HLS ARRAY_PARTITION variable=wrd_im complete dim=1

    int output_row = 0;
    int output_col = 0;
    int prev_output_row = 0;
    int prev_output_col = 0;

    int input_row;
    int input_col;
    
    //int prev_addr[CACHE_DPTH];
    ap_int<27> prev_addr[CACHE_DPTH];
    #pragma HLS ARRAY_PARTITION variable=prev_addr complete dim=1
    bus_t cache[CACHE_DPTH];

    init_loop:
    for (int i = 0; i < CACHE_DPTH; i++) {
        #pragma HLS PIPELINE II=1
        prev_addr[i] = -1;
    }

    n_loop:
    for (int n = 0; n < number; n++) {
        prev_output_row = 0;
        prev_output_col = 0;
        out_tc_loop:
        for (int out_tc = 0; out_tc < out_tiles; out_tc++) {
            channel_loop:
            for (int channel = 0; channel < channels; channel++) {
                krow_loop:
                for (char kernel_row = 0; kernel_row < kernel_h; kernel_row++) {
                    kcol_loop:
                    for (char kernel_col = 0; kernel_col < kernel_w; kernel_col++) {
#pragma HLS LOOP_FLATTEN off
                        output_row = prev_output_row;
                        output_col = prev_output_col;
                        input_row = -pad_h + kernel_row * dilation_h + prev_output_row * stride_h;
                        input_col = -pad_w + kernel_col * dilation_w + prev_output_col * stride_w;
                        itr_loop:
                        for (int itr = 0; itr < VCTR_SIZE; itr++) {
                            #pragma HLS PIPELINE II=1
                            if (VCTR_SIZE * out_tc + itr < out_itrs) {
                                if (!(input_row >= 0 && input_row < height) || 
                                        !(input_col >= 0 && input_col < width)) {
                                    wrd_col.d[(addr_col++) & VCTR_SIZE_SUB1] = 0;
                                } else {
                                    int in_tc = input_col >> VCTR_SIZE_LOG2;
                                    addr_im = (channels * n + channel) * channel_size +
                                        height * in_tc + input_row; // bus style
                                    #if 0
                                    wrd_im = data_im[addr_im];
                                    #else
                                    cache_im(addr_im, data_im, wrd_im, prev_addr, cache);
                                    #endif
                                    wrd_col.d[(addr_col++) & VCTR_SIZE_SUB1] = 
                                        wrd_im.d[(input_col & VCTR_SIZE_SUB1)];
                                    #ifdef DEBUG
                                    //if (n == 0 && out_tc == out_tiles - 1 && channel == 0 && kernel_row == 0 && kernel_col == 1)
                                    //    DEBUG_PRINT("addr_im=%d, input_col=%d, wrd_im.d[]=%d, addr_col=%d\n", 
                                    //            addr_im, input_col, (unsigned short)wrd_im.d[(input_col & VCTR_SIZE_SUB1)], addr_col);
                                    //if (n == 0)
                                    //    DEBUG_PRINT("channel=%d, kernel_row=%d, kernel_col=%d, addr_im=%d\n", 
                                    //            channel, kernel_row, kernel_col, addr_im);
                                    #endif
                                }
                                if (output_col == output_w - 1) {
                                    output_col = 0; 
                                    output_row++; 
                                    input_col = -pad_w + kernel_col * dilation_w;
                                    input_row += stride_h;
                                } else {
                                    output_col++; 
                                    input_col += stride_w;
                                }
                            } else {
                                wrd_col.d[(addr_col++) & VCTR_SIZE_SUB1] = 0;
                            } // >= out_itrs
                        } // itr
                        //data_col[(addr_col - 1) >> VCTR_SIZE_LOG2] = wrd_col;
                        strm_col << wrd_col;
                    } // kernel_col
                } // kernel_row
            } // channel
            prev_output_row = output_row;
            prev_output_col = output_col;
        } // out_tc
    } // n
}

#define INBUF_DPTH 32

void im2col_read_v2(
        const bus_t *data_im,
        hls::stream<bus_t> &strm_col,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w)
{
    const int width_wrd = ((width - 1) >> VCTR_SIZE_LOG2) + 1;

    bus_t in_buf[INBUF_DPTH];

    short input_row;
    short input_col;

    bus_t wrd_col;
    #pragma HLS ARRAY_PARTITION variable=wrd_col complete dim=1
    bus_t wrd_im;
    #pragma HLS ARRAY_PARTITION variable=wrd_im complete dim=1
    ushort34_t wrd_im_shft;
    #pragma HLS ARRAY_PARTITION variable=wrd_im_shft complete dim=1

    n_loop:
    for (int n = 0; n < number; n++) {
        channel_loop:
        for (int channel = 0; channel < channels; channel++) {
            inrow_loop:
            for (int in_row = 0; in_row < height; in_row++) {
                // Assuming width <= VCTR_SIZE = 32, just burst read for height cycles
                #pragma HLS PIPELINE II=1
                int rdAddr = (n * channels + channel)* height + in_row;
                in_buf[in_row] = data_im[rdAddr];
            }

            orow_loop:
            for (char out_row = 0; out_row < output_h; out_row++) {
                krow_loop:
                for (char kernel_row = 0; kernel_row < kernel_h; kernel_row++) {
                    input_row = -pad_h + kernel_row * dilation_h + out_row * stride_h;

                    if (!(input_row >= 0 && input_row < height)) {
                        i_loop:
                        for (char i = 0; i < VCTR_SIZE; i++) {
                            #pragma HLS UNROLL
                            wrd_im.d[i] = 0;
                        }
                    } else {
                        wrd_im = in_buf[input_row];
                    }

                    assert(pad_w >= 0 && pad_w <= 2 && "im2col pad_w only support 0 to 2");
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

                    kcol_loop:
                    for (char kernel_col = 0; kernel_col < kernel_w; kernel_col++) {
//#pragma HLS LOOP_FLATTEN off
                        #pragma HLS PIPELINE II=1

                        //input_col = -pad_w + kernel_col * dilation_w;
                        //ocol_loop:
                        //for (char out_col = 0; out_col < VCTR_SIZE; out_col++) {
                        //    if (out_col < output_w) {
                        //        if (input_col >=0 && input_col < width)
                        //            wrd_col.d[out_col] = wrd_im.d[input_col];
                        //        else
                        //            wrd_col.d[out_col] = 0;
                        //        #ifdef DEBUG
                        //        //if (n == 1 && out_row == 0 && kernel_col == 0 && out_col == 1)
                        //        //    DEBUG_PRINT("wrd_col.d[1]=%d\n", wrd_col.d[out_col]);
                        //        #endif
                        //    }
                        //    input_col += stride_w;
                        //} // out_col

                        ocol_loop:
                        for (char out_col = 0; out_col < VCTR_SIZE; out_col++) {
                            if (out_col < output_w) {
                                wrd_col.d[out_col] = wrd_im_shft.d[out_col];
                            }
                        }
                        assert(dilation_w == 1 && stride_w == 1 && "im2col only dilatioin_w=1 and stride_w=1 for shifting");
                        shft_loop:
                        for (char i = 0; i < VCTR_SIZE+2; i++) {
                            wrd_im_shft.d[i] = (i+1 < VCTR_SIZE+2 ? wrd_im_shft.d[i+1] : 0);
                        }

                        //int wrAddr = ((out_row * channels +  channel) * kernel_h + kernel_row) * kernel_w + kernel_col;
                        //data_col[wrAddr] = wrd_col;
                        strm_col << wrd_col;
                    } // kernel_col
                } // kernel_row
            } // out_row
        } // channel
    } // n
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

    n_loop:
    for (int n = 0; n < number; n++) {
        channel_loop:
        for (int channel = 0; channel < channels; channel++) {
            orow_loop:
            for (char out_row = 0; out_row < output_h; out_row++) {
                //krow_loop:
                //for (char kernel_row = 0; kernel_row < kernel_h; kernel_row++) 
                //    kcol_loop:
                //    for (char kernel_col = 0; kernel_col < kernel_w; kernel_col++) 
                k_loop:
                for (short k = 0; k < kernel_h * kernel_w; k++) {
#pragma HLS LOOP_FLATTEN off
                        #pragma HLS PIPELINE II=1
                        strm_col >> wrd_col;
                        //int wrAddr = ((out_row * channels +  channel) * kernel_h + kernel_row) * kernel_w + kernel_col;
                        int wrAddr = (((n * output_h + out_row) * channels +  channel) * kernel_h * kernel_w) + k;
                        data_col[wrAddr] = wrd_col;
                }
            }
        }
    }
}

void im2col_write(
        hls::stream<bus_t> &strm_col,
        bus_t *data_col,
        int number, int channels, int height, int width,
        char kernel_h, char kernel_w,
        char pad_h, char pad_w,
        char stride_h, char stride_w,
        char dilation_h, char dilation_w,
        int output_h, int output_w)
{
    const int out_itrs = output_h * output_w;
    const int out_tiles = ((out_itrs - 1) >> VCTR_SIZE_LOG2) + 1;
    const int width_wrd = ((width - 1) >> VCTR_SIZE_LOG2) + 1;

    unsigned addr_col = 0; // in elem
    bus_t wrd_col;
    #pragma HLS ARRAY_PARTITION variable=wrd_col complete dim=1

//    n_loop:
//    for (int n = 0; n < number; n++) {
//        out_tc_loop:
//        for (int out_tc = 0; out_tc < out_tiles; out_tc++) {
//            channel_loop:
//            for (int channel = 0; channel < channels; channel++) {
//#pragma HLS LOOP_FLATTEN off
//                krow_loop:
//                for (char kernel_row = 0; kernel_row < kernel_h; kernel_row++) {
//                    kcol_loop:
//                    for (char kernel_col = 0; kernel_col < kernel_w; kernel_col++) {
//                        #pragma HLS PIPELINE II=1
//                        strm_col >> wrd_col;
//                        //data_col[addr_col++] = wrd_col;
//                        addr_col = (((n * out_tiles + out_tc) * channels + channel) * kernel_h + kernel_row) * kernel_w + kernel_col;
//                        data_col[addr_col] = wrd_col;
//                    } // kernel_col
//                } // kernel_row
//            } // channel
//        } // out_tc
//    } // n
    i_loop:
    for (short i = 0; i < number * out_tiles; i++) {
        j_loop:
        for (short j = 0; j < channels * kernel_h * kernel_w; j++) {
#pragma HLS LOOP_FLATTEN off
            #pragma HLS PIPELINE II=1
            strm_col >> wrd_col;
            addr_col = i * channels * kernel_h * kernel_w + j;
            data_col[addr_col] = wrd_col;
        }
    }
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
    //#pragma HLS INTERFACE m_axi port=data_im offset=slave bundle=gmem0 latency=100 num_read_outstanding=32 max_read_burst_length=2
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

    static hls::stream<bus_t> strm_col("strm_col");
    #pragma HLS STREAM variable=strm_col depth=32 dim=1

    //im2col_read(
    //        data_im,
    //        strm_col,
    //        number, channels, height, width,
    //        kernel_h, kernel_w,
    //        pad_h, pad_w,
    //        stride_h, stride_w,
    //        dilation_h, dilation_w,
    //        output_h, output_w);
    //im2col_write(
    //        strm_col,
    //        data_col,
    //        number, channels, height, width,
    //        kernel_h, kernel_w,
    //        pad_h, pad_w,
    //        stride_h, stride_w,
    //        dilation_h, dilation_w,
    //        output_h, output_w);
    im2col_read_v2(
            data_im,
            strm_col,
            number, channels, height, width,
            kernel_h, kernel_w,
            pad_h, pad_w,
            stride_h, stride_w,
            dilation_h, dilation_w,
            output_h, output_w);
    im2col_write_v2(
            strm_col,
            data_col,
            number, channels, height, width,
            kernel_h, kernel_w,
            pad_h, pad_w,
            stride_h, stride_w,
            dilation_h, dilation_w,
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

