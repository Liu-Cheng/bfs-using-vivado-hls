#include <hls_stream.h>
#include <ap_int.h>
#include <stdio.h>

#define VLEN 16
#define LOG2_VLEN 4

typedef unsigned int uint;
typedef struct uint32_struct{
	unsigned int data[16];
} bus32_dt;

static void write_mem(
		bus32_dt *target, 
		const int burst_num,
		const int burst_len,
		const int base_addr,
		const int stride,
		const int c)
{
	for(int i = 0; i < burst_num; i++){
		uint addr_start = base_addr + i * stride;
		uint addr_end = addr_start + burst_len;
		uint burst_start = addr_start >> LOG2_VLEN;
		uint burst_end = (addr_end >> LOG2_VLEN) + 1;
        Read_Loop:
		for(uint j = 0; j < (burst_end - burst_start); j++){
        #pragma HLS LOOP_FLATTEN off
        #pragma HLS pipeline II=1
			uint base = (burst_start + j) << LOG2_VLEN;
			for(int k = 0; k < VLEN; k++){
				uint addr = base + k;
				if(addr >= addr_start && addr < addr_end){
					target[burst_start + j].data[k] = c;
				}
			}
		}
	}
}



extern "C" {
void mem_test(
		bus32_dt *target,
		const int burst_num,
		const int burst_len,
		const int stride,
		const int c
		)
{
#pragma HLS DATA_PACK variable=target
#pragma HLS INTERFACE m_axi port=target offset=slave bundle=gmem0
#pragma HLS INTERFACE s_axilite port=burst_num bundle=control
#pragma HLS INTERFACE s_axilite port=burst_len bundle=control
#pragma HLS INTERFACE s_axilite port=stride bundle=control
#pragma HLS INTERFACE s_axilite port=c bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int read_base = 0x20000;
#pragma HLS dataflow
	write_mem(target, burst_num, burst_len, read_base, stride, c);
}
}

