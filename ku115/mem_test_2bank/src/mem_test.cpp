#include <ap_int.h>

typedef ap_uint<512> uint512_dt;

extern "C" {
void mem_test(
		uint512_dt *input, 
		uint512_dt *output, 
		int inc, 
		int size) {
// Using Separate interface bundle gmem0 and gmem1 for both argument
// input and output. It will allow user to create two separate interfaces
// and as a result allow kernel to access both interfaces simultaneous. 
#pragma HLS INTERFACE m_axi port=input  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem1
#pragma HLS INTERFACE s_axilite port=input  bundle=control
#pragma HLS INTERFACE s_axilite port=output bundle=control
#pragma HLS INTERFACE s_axilite port=inc  bundle=control
#pragma HLS INTERFACE s_axilite port=size  bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
	for(int i = 0; i < size/16; i++){
#pragma HLS pipeline
		for(int j = 0; j < 16; j++){
			output[i].range((j+1)*32 - 1, j * 32) = input[i].range((j+1)*32 - 1, j * 32) + inc;
		}
	}
}
}


