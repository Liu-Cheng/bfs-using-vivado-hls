#include <ap_int.h>

typedef ap_uint<512> uint512_dt;

extern "C" {
void mem_test(
		uint512_dt *input0, 
		uint512_dt *input1, 
		uint512_dt *output0, 
		uint512_dt *output1, 
		int inc, 
		int size) {
// Using Separate interface bundle gmem0 and gmem1 for both argument
// input and output. It will allow user to create two separate interfaces
// and as a result allow kernel to access both interfaces simultaneous. 
#pragma HLS INTERFACE m_axi port=input0  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=input1  offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=output0 offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=output1 offset=slave bundle=gmem3
#pragma HLS INTERFACE s_axilite port=inc  bundle=control
#pragma HLS INTERFACE s_axilite port=size  bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
	for(int i = 0; i < size/16; i++){
#pragma HLS pipeline
		for(int j = 0; j < 16; j++){
			output0[i].range((j+1)*32 - 1, j * 32) = input0[i].range((j+1)*32 - 1, j * 32) + inc;
			output1[i].range((j+1)*32 - 1, j * 32) = input1[i].range((j+1)*32 - 1, j * 32) + inc;
		}
	}
}
}


