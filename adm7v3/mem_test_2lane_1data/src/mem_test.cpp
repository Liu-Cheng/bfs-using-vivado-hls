#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

//Read
static void read_mem(
		const int  *input, 
		const int burst_num,
		const int base_addr,
		const int stride,
		int* result)
{
	int sum = 0;
	for(int i = 0; i < burst_num; i++){
    #pragma HLS pipeline II=1
		int addr = base_addr + i * stride;
		sum += input[addr];

		if(i == burst_num - 1){
			*result = sum;
		}
	}
}

//Write
static void write_mem(
		int *output,
		const int burst_num,
		const int base_addr,
		const int inc,
		const int stride
		)
{
	for(int i = 0; i < burst_num; i++){
        #pragma HLS pipeline II=1
		int addr = base_addr + i * stride;
		output[addr] = inc;
	}
}




extern "C" {
void mem_test(
		const int *input0,
		const int *input1,
		int *output0,
		int *output1,	
		const int inc,
		const int burst_num,
		const int stride,
		int *result0,
		int *result1
		)
{
#pragma HLS INTERFACE m_axi port=input0 offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=input1 offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=output0 offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=output1 offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=result0 offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=result1 offset=slave bundle=gmem5
#pragma HLS INTERFACE s_axilite port=inc bundle=control
#pragma HLS INTERFACE s_axilite port=burst_num bundle=control
#pragma HLS INTERFACE s_axilite port=stride bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int read_base0 = 0x20000;
	int read_base1 = 0x40000;
	int write_base0 = 0x80000;
	int write_base1 = 0xA0000;

#pragma HLS dataflow
	read_mem(input0, burst_num, read_base0, stride, result0);
	read_mem(input1, burst_num, read_base1, stride, result1);
	write_mem(output0, burst_num, write_base0, inc, stride);
	write_mem(output1, burst_num, write_base1, inc, stride);
}
}

