#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

//Read
static void read_mem(
		const int  *input, 
		const int burst_num,
		const int burst_len,
		const int base_addr,
		const int stride,
		int* result)
{
	int sum = 0;
	for(int i = 0; i < burst_num; i++){
	#pragma HLS LOOP_FLATTEN off
		int addr = base_addr + i * stride;
		for(int j = 0; j < burst_len; j++){
        #pragma HLS pipeline II=1
			sum += input[addr + j];
		}

		if(i == burst_num - 1){
			*result = sum;
		}
	}
}

//Write
static void write_mem(
		int *output,
		const int burst_num,
		const int burst_len,
		const int base_addr,
		const int inc,
		const int stride
		)
{
	for(int i = 0; i < burst_num; i++){
		int addr = base_addr + i * stride;
		for(int j = 0; j < burst_len; j++){
        #pragma HLS pipeline II=1
			output[addr + j] = inc;
		}
	}
}


extern "C" {
void mem_test(
		const int *input0,
		const int *input1,
		const int *input2,
		const int *input3,
		int *output0,
		int *output1,	
		int *output2,	
		int *output3,	
		const int inc,
		const int burst_num,
		const int burst_len,
		const int stride,
		int *result0,
		int *result1,
		int *result2,
		int *result3
		)
{
#pragma HLS INTERFACE m_axi port=input0 offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=input1 offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=input2 offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=input3 offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=output0 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=output1 offset=slave bundle=gmem5
#pragma HLS INTERFACE m_axi port=output2 offset=slave bundle=gmem6
#pragma HLS INTERFACE m_axi port=output3 offset=slave bundle=gmem7
#pragma HLS INTERFACE m_axi port=result0 offset=slave bundle=gmem8
#pragma HLS INTERFACE m_axi port=result1 offset=slave bundle=gmem9
#pragma HLS INTERFACE m_axi port=result2 offset=slave bundle=gmem10
#pragma HLS INTERFACE m_axi port=result3 offset=slave bundle=gmem11
#pragma HLS INTERFACE s_axilite port=inc bundle=control
#pragma HLS INTERFACE s_axilite port=burst_num bundle=control
#pragma HLS INTERFACE s_axilite port=burst_len bundle=control
#pragma HLS INTERFACE s_axilite port=stride bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int read_base0 = 0x200000;
	int read_base1 = 0x400000;
	int read_base2 = 0x600000;
	int read_base3 = 0x800000;
	int write_base0 = 0xA00000;
	int write_base1 = 0xC00000;
	int write_base2 = 0xE00000;
	int write_base3 = 0x000000;

#pragma HLS dataflow
	read_mem(input0, burst_num, burst_len, read_base0, stride, result0);
	read_mem(input1, burst_num, burst_len, read_base1, stride, result1);
	read_mem(input2, burst_num, burst_len, read_base2, stride, result2);
	read_mem(input3, burst_num, burst_len, read_base3, stride, result3);
	write_mem(output0, burst_num, burst_len, write_base0, inc, stride);
	write_mem(output1, burst_num, burst_len, write_base1, inc, stride);
	write_mem(output2, burst_num, burst_len, write_base2, inc, stride);
	write_mem(output3, burst_num, burst_len, write_base3, inc, stride);
}
}

