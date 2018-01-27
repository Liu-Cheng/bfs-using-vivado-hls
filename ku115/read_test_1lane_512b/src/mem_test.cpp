#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define VLEN 16
#define LOG2_VLEN 4

typedef unsigned int uint;
typedef struct uint32_struct{
	unsigned int data[16];
} bus32_dt;

static void read_mem(
		bus32_dt *input, 
		const int burst_num,
		const int burst_len,
		const int base_addr,
		const int stride,
		bus32_dt* result)
{
	bus32_dt sum;
	#pragma HLS ARRAY_PARTITION variable=sum complete dim=1

	for(int i = 0; i < VLEN; i++){
    #pragma LOOP UNROLL
		sum.data[i] = 0;
	}
	for(int i = 0; i < burst_num; i++){
		uint addr_start = base_addr + i * stride;
		uint addr_end = addr_start + burst_len;
		uint burst_start = addr_start >> LOG2_VLEN;
		uint burst_end = (addr_end >> LOG2_VLEN) + 1;
        Read_Loop:
		for(uint j = 0; j < (burst_end - burst_start); j++){
        #pragma HLS LOOP_FLATTEN off
        #pragma HLS pipeline II=1
			bus32_dt word = input[burst_start + j];
			uint base = (burst_start + j) << LOG2_VLEN;
            #pragma HLS ARRAY_PARTITION variable=word complete dim=1
			for(int k = 0; k < VLEN; k++){
				uint addr = base + k;
				if(addr >= addr_start && addr < addr_end){
					sum.data[k] += word.data[k];
				}
			}
		}

		if(i == burst_num - 1){
			*result = sum;
		}
	}
}



extern "C" {
void mem_test(
		bus32_dt *input,
		const int inc,
		const int burst_num,
		const int burst_len,
		const int stride,
		bus32_dt *result
		)
{
#pragma HLS DATA_PACK variable=input
#pragma HLS INTERFACE m_axi port=input offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=result offset=slave bundle=gmem2
#pragma HLS INTERFACE s_axilite port=inc bundle=control
#pragma HLS INTERFACE s_axilite port=burst_num bundle=control
#pragma HLS INTERFACE s_axilite port=burst_len bundle=control
#pragma HLS INTERFACE s_axilite port=stride bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int read_base = 0x20000;
#pragma HLS dataflow
	read_mem(input, burst_num, burst_len, read_base, stride, result);
}
}

