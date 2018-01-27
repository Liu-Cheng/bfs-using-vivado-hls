/**********
Copyright (c) 2017, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

/*******************************************************************************

SDx Key Concept :
    
    This is a CNN (Convolutional Neural Network) convolution layer based example
    to showcase the effectiveness of using multiple compute units when the base
    algorithm consists of multiple nested loops with large loop count. The main
    aim of this example is to help developer to overcome clock frequency issues
    and achieve better performance.

*******************************************************************************/

/*

Kernel Description (Good Example) :
   
    This example uses multiple compute units & wide range of work items which
    process Output filters when convolution operation is triggered. It also
    presents the uniform local memory alignment when multiple work_group/compute
    units are used. 
    
    Note : Naive version (Bad Example) version is in the file cnn_convolution_bad.cl
    
    Arguments :
    
        int *image   (input )  --> Input Image    
        int *weight  (input )  --> Input Weights  
        int *out     (output)  --> Output filters 
        int  size    (input )  --> Output size

    Kernel Configuration :
        
        1. Output Channels    = 256
        2. Work Groups        = Number of Compute Units
        3. Work Items         = One per Work_Group (Compute Unit) 

        -----------------------------------------------------
        | Parameter     | Value |   Description             |
        -----------------------------------------------------
        | Channels      | 96    | #Input Channels           |                            
        -----------------------------------------------------
        | IHeight       | 27    | Input Image Height        |
        -----------------------------------------------------
        | IWidth        | 27    | Input Image Width         |
        -----------------------------------------------------
        | Window        | 5     | Convolution Window Size   |
        -----------------------------------------------------
        | Stride        | 1     | Convolution Stride        |
        -----------------------------------------------------
        | Padding       | 2     | Convolution Image Padding |
        -----------------------------------------------------
        | OutputFilters | 256   | Output Filters/Images     |
        -----------------------------------------------------
        | OHeight       | 27    | Output Image Height       |
        -----------------------------------------------------
        | OWidth        | 27    | Output Image Width        |
        -----------------------------------------------------


    Memory Usage (Local Buffer (Per Work_Group / Compute Unit)):
        
        1. Image    ~ (IHeight x IWidth x Channels):[2.84 x 96 KB]
        2. Weights  ~ (Channels x Window x Window):[96 x 0.09 KB]
        3. Output   ~ (OHeight x OWidth):[2.84 KB]

    Reference : 
             
        To understand Convolution Layer of a CNN better please refer to
        website below (Convolution Demo Animation in the link).
                                                     
        Link: http://cs231n.github.io/convolutional-networks/
*/

inline int gen_random_data(int lfsr){
	int bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
	int out = (lfsr >> 1) | (bit << 15);
	return out;
}

__kernel __attribute__ ((reqd_work_group_size(1, 1, 1)))
void tran0(
		__global int *input0,
		__global int *output0,
        const int size,
        const int inc
    )
{
	int lfsr_rd0 = 0xACE;
	int lfsr_rd1 = 0xBC8;
	int lfsr_rd2 = 0xA59;
	int lfsr_rd3 = 0x3C7;
	int lfsr_wr = 0xEAC;
	int read_sum0 = 0;
	int read_sum1 = 0;
	int read_sum2 = 0;
	int read_sum3 = 0;

	__attribute__((xcl_pipeline_loop))
	for(int i = 0; i < size; i++){
		lfsr_rd0 = gen_random_data(lfsr_rd0);
		int rd_address = lfsr_rd0 & 0xFFFFFFF;
		read_sum0 += input0[rd_address];
	}

	__attribute__((xcl_pipeline_loop))
	for(int i = 0; i < size; i++){
		lfsr_rd1 = gen_random_data(lfsr_rd1);
		int rd_address = lfsr_rd1 & 0xFFFFFFF;
		read_sum1 += input0[rd_address];
	}

	__attribute__((xcl_pipeline_loop))
	for(int i = 0; i < size; i++){
		lfsr_rd2 = gen_random_data(lfsr_rd2);
		int rd_address = lfsr_rd2 & 0xFFFFFFF;
		read_sum2 += input0[rd_address];
	}

	__attribute__((xcl_pipeline_loop))
	for(int i = 0; i < size; i++){
		lfsr_rd3 = gen_random_data(lfsr_rd3);
		int rd_address = lfsr_rd3 & 0xFFFFFFF;
		read_sum3 += input0[rd_address];
	}

	__attribute__((xcl_pipeline_loop))
	for(int i = 0; i < size; i++){
		lfsr_wr = gen_random_data(lfsr_wr);
		int wr_address = lfsr_wr & 0xFFFFFFF;
		output0[wr_address] = inc + inc;
	}
}


