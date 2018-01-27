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
		bus32_dt *input0,
		bus32_dt *input1,
		bus32_dt *input2,
		bus32_dt *input3,
		bus32_dt *result0,
		bus32_dt *result1,
		bus32_dt *result2,
		bus32_dt *result3,
		const int burst_num,
		const int burst_len,
		const int stride
		)
{
#pragma HLS DATA_PACK variable=input0
#pragma HLS DATA_PACK variable=input1
#pragma HLS DATA_PACK variable=input2
#pragma HLS DATA_PACK variable=input3
#pragma HLS INTERFACE m_axi port=input0 offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=input1 offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=input2 offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=input3 offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=result0 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=result1 offset=slave bundle=gmem5
#pragma HLS INTERFACE m_axi port=result2 offset=slave bundle=gmem6
#pragma HLS INTERFACE m_axi port=result3 offset=slave bundle=gmem7
#pragma HLS INTERFACE s_axilite port=burst_num bundle=control
#pragma HLS INTERFACE s_axilite port=burst_len bundle=control
#pragma HLS INTERFACE s_axilite port=stride bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int read_base[4] = {0, 0, 0, 0};
#pragma HLS dataflow
	read_mem(input0, burst_num, burst_len, read_base[0], stride, result0);
	read_mem(input1, burst_num, burst_len, read_base[1], stride, result1);
	read_mem(input2, burst_num, burst_len, read_base[2], stride, result2);
	read_mem(input3, burst_num, burst_len, read_base[3], stride, result3);
}
}

