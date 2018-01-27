#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

//Read
static void read_mem(
		int  *input, 
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
		int *input,
		int *output, 
		const int inc,
		const int burst_num,
		const int stride,
		int *result
		)
{
#pragma HLS INTERFACE m_axi port=input offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=result offset=slave bundle=gmem2
#pragma HLS INTERFACE s_axilite port=inc bundle=control
#pragma HLS INTERFACE s_axilite port=burst_num bundle=control
#pragma HLS INTERFACE s_axilite port=stride bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int read_base = 0x20000;
	int write_base = 0x30000;
#pragma HLS dataflow
	read_mem(input, burst_num, read_base, stride, result);
	write_mem(output, burst_num, write_base, inc, stride);	
}
}

