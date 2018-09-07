// 16 x 128K bit bitmap memory
#define BITMAP_MEM_DEPTH (256 * 1024) 
#define BUFFER_ADDR_WIDTH 18
#define BRAM_BANK_NUM 16
#define BUFFER_SIZE 16
#define BANK_BW 3 // 3 when pad == 8, 4 when pad == 16

#define BITMASK0 0x1
#define BITMASK1 0x2
#define BITMASK2 0x4
#define BITMASK3 0x8
#define BITMASK4 0x10
#define BITMASK5 0x20
#define BITMASK6 0x40
#define BITMASK7 0x80

#define BANK_MASK 0x7
#define SEL_MASK  0x38
#define OFFSET_MASK 0x1FFFC0

inline int bitop(int bitsel){
	switch(bitsel){
		case 0:
			return BITMASK0;
		case 1:
			return BITMASK1;
		case 2:
			return BITMASK2;
		case 3:
			return BITMASK3;
		case 4:
			return BITMASK4;
		case 5:
			return BITMASK5;
		case 6:
			return BITMASK6;
		case 7:
			return BITMASK7;
		default:
			return EXIT_FAILURE;
	}
}

channel int2 rpa_channel __attribute__ ((depth(64)));   
channel int8 cia_channel __attribute__ ((depth(64)));
channel int  cia_end_channel __attribute__ ((depth(4)));

channel int next_frontier_channel0 __attribute__ ((depth(64)));
channel int next_frontier_channel1 __attribute__ ((depth(64)));
channel int next_frontier_channel2 __attribute__ ((depth(64)));
channel int next_frontier_channel3 __attribute__ ((depth(64)));
channel int next_frontier_channel4 __attribute__ ((depth(64)));
channel int next_frontier_channel5 __attribute__ ((depth(64)));
channel int next_frontier_channel6 __attribute__ ((depth(64)));
channel int next_frontier_channel7 __attribute__ ((depth(64)));

channel int next_frontier_end_channel0 __attribute__ ((depth(4)));
channel int next_frontier_end_channel1 __attribute__ ((depth(4)));
channel int next_frontier_end_channel2 __attribute__ ((depth(4)));
channel int next_frontier_end_channel3 __attribute__ ((depth(4)));
channel int next_frontier_end_channel4 __attribute__ ((depth(4)));
channel int next_frontier_end_channel5 __attribute__ ((depth(4)));
channel int next_frontier_end_channel6 __attribute__ ((depth(4)));
channel int next_frontier_end_channel7 __attribute__ ((depth(4)));

__kernel void __attribute__((task)) read_rpa(
		__global int2* restrict rpa,
		const int frontier_size
		){
	for(int i = 0; i < frontier_size; i++){
		write_channel_altera.write(rpa_channel, rpa[i]);
	}

}
	
// Read cia of the frontier 
__kernel void __attribute__((task)) read_cia(
		__global int8* restrict cia,
		const int               frontier_size
		)
{
	
	for(int i = 0; i < frontier_size; i++){
		int2 word  = read_channel_altera.read(rpa_channel);
		int  num   = (word.s1) >> BANK_BW;
		int  start = (word.s0) >> BANK_BW;

		for(int j = 0; j < num; j++){
			write_channel_altera(cia_channel, cia[start + j]);
		}
	}

	write_channel_altera(cia_end_channel, 1);
}

// Traverse cia 
__kernel void __attribute__((task)) traverse_cia(	
		__global int* restrict next_frontier_size,
		const int              root_vidx,
		const char             level
		)
{
	static uchar bitmap0[BITMAP_MEM_DEPTH];
	static uchar bitmap1[BITMAP_MEM_DEPTH];
	static uchar bitmap2[BITMAP_MEM_DEPTH];
	static uchar bitmap3[BITMAP_MEM_DEPTH];
	static uchar bitmap4[BITMAP_MEM_DEPTH];
	static uchar bitmap5[BITMAP_MEM_DEPTH];
	static uchar bitmap6[BITMAP_MEM_DEPTH];
	static uchar bitmap7[BITMAP_MEM_DEPTH];

   	if(level == 0){
        traverse_init:
		for (int i = 0; i < BITMAP_MEM_DEPTH; i++){
        #pragma HLS pipeline II=1
			bitmap0[i] = 0;
			bitmap1[i] = 0;
			bitmap2[i] = 0;
			bitmap3[i] = 0;
			bitmap4[i] = 0;
			bitmap5[i] = 0;
			bitmap6[i] = 0;
			bitmap7[i] = 0;
		}

		int bank_idx = root_vidx & BANK_MASK;
		int bitsel   = root_vidx & SEL_MASK
		int offset   = root_vidx & OFFSET_MASK;

		if(bank_idx == 0) bitmap0[offset] = bitop(bitsel);
		if(bank_idx == 1) bitmap1[offset] = bitop(bitsel);
		if(bank_idx == 2) bitmap2[offset] = bitop(bitsel);
		if(bank_idx == 3) bitmap3[offset] = bitop(bitsel);
		if(bank_idx == 4) bitmap4[offset] = bitop(bitsel);
		if(bank_idx == 5) bitmap5[offset] = bitop(bitsel);
		if(bank_idx == 6) bitmap6[offset] = bitop(bitsel);
		if(bank_idx == 7) bitmap7[offset] = bitop(bitsel);
	}

	int mem_addr0 = 0;
	int mem_addr1 = 0;
	int mem_addr2 = 0;
	int mem_addr3 = 0;
	int mem_addr4 = 0;
	int mem_addr5 = 0;
	int mem_addr6 = 0;
	int mem_addr7 = 0;

	bool data_valid = false;
	bool flag_valid = false;
	int  flag = 0;
	while(true){

		int8 word = read_channel_nb_altera(cia_channel, &data_valid);
		if(data_valid){
			int bitsel0 = word.s0 & SEL_MASK;
			int bitsel1 = word.s1 & SEL_MASK;
			int bitsel2 = word.s2 & SEL_MASK;
			int bitsel3 = word.s3 & SEL_MASK;
			int bitsel4 = word.s4 & SEL_MASK;
			int bitsel5 = word.s5 & SEL_MASK;
			int bitsel6 = word.s6 & SEL_MASK;
			int bitsel7 = word.s7 & SEL_MASK;

			int offset0 = word.s0 & OFFSET_MASK;
			int offset1 = word.s1 & OFFSET_MASK;
			int offset2 = word.s2 & OFFSET_MASK;
			int offset3 = word.s3 & OFFSET_MASK;
			int offset4 = word.s4 & OFFSET_MASK;
			int offset5 = word.s5 & OFFSET_MASK;
			int offset6 = word.s6 & OFFSET_MASK;
			int offset7 = word.s7 & OFFSET_MASK;

			if(word.s0 != -1){
				uchar tmp = bitmap0[offset0]; 
				uchar mask = bitop(bitsel0); 
				if((tmp & mask) == 0){
					write_channel_altera(next_frontier_channel0, word.s0);
					bitmap0[offset0] = tmp | mask;
					mem_addr0++;
				}
			}

			if(word.s1 != -1){
				uchar tmp = bitmap1[offset1]; 
				uchar mask = bitop(bitsel1); 
				if(tmp & mask == 0){
					write_channel_altera(next_frontier_channel1, word.s1);
					bitmap1[offset1] = tmp | mask;
					mem_addr1++;
				}
			}

			if(word.s2 != -1){
				uchar tmp = bitmap2[offset2]; 
				uchar mask = bitop(bitsel2); 
				if(tmp & mask == 0){
					write_channel_altera(next_frontier_channel2, word.s2);
					bitmap2[offset2] = tmp | mask;
					mem_addr2++;
				}
			}

			if(word.s3 != -1){
				uchar tmp = bitmap3[offset3]; 
				uchar mask = bitop(bitsel3); 
				if(tmp & mask == 0){
					write_channel_altera(next_frontier_channel3, word.s3);
					bitmap3[d3] = tmp | mask;
					mem_addr3++;
				}
			}

			if(word.s4 != -1){
				uchar tmp = bitmap4[offset4]; 
				uchar mask = bitop(bitsel4); 
				if(tmp & mask == 0){
					write_channel_altera(next_frontier_channel4, word.s4);
					bitmap4[d4] = tmp | mask;
					mem_addr4++;
				}
			}

			if(word.s5 != -1){
				uchar tmp = bitmap5[offset5]; 
				uchar mask = bitop(bitsel5);
				if(tmp & mask == 0){
					write_channel_altera(next_frontier_channel5, word.s5);
					bitmap5[d5] = tmp | mask;
					mem_addr5++;
				}
			}

			if(word.s6 != -1){
				uchar tmp = bitmap6[offset6]; 
				uchar mask = bitop(bitsel6); 
				if(tmp & mask == 0){
					write_channel_altera(next_frontier_channel6, word.s6);
					bitmap6[d6] = tmp | mask;
					mem_addr6++; 
				}
			}

			if(word.s7 != -1){
				uchar tmp = bitmap7[offset7]; 
				uchar mask = bitop(bitsel7); 
				if(task & mask == 0){
					write_channel_altera(next_frontier_stream7, word.s7);
					bitmap7[d7] = tmp | mask;
					mem_addr7++;
				}
			}
		}

		// Read flag channel
		int flag_tmp = read_nb_channel_altera(cia_end_channel, &flag_valid);
		if(flag_valid) flag = flag_tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}

	}

	write_channel_altera(next_frontier_end_channel0, 1);
	write_channel_altera(next_frontier_end_channel1, 1);
	write_channel_altera(next_frontier_end_channel2, 1);
	write_channel_altera(next_frontier_end_channel3, 1);
	write_channel_altera(next_frontier_end_channel4, 1);
	write_channel_altera(next_frontier_end_channel5, 1);
	write_channel_altera(next_frontier_end_channel6, 1);
	write_channel_altera(next_frontier_end_channel7, 1);

	*(next_frontier_size  + 0)  = mem_addr0;
	*(next_frontier_size  + 1)  = mem_addr1;
	*(next_frontier_size  + 2)  = mem_addr2;
	*(next_frontier_size  + 3)  = mem_addr3;
	*(next_frontier_size  + 4)  = mem_addr4;
	*(next_frontier_size  + 5)  = mem_addr5;
	*(next_frontier_size  + 6)  = mem_addr6;
	*(next_frontier_size  + 7)  = mem_addr7;
}
 
// write channel0
__kernel void __attribute__ ((task)) write_frontier0(
		int* next_frontier0
		)
{

	int buffer[BUFFER_SIZE];
	bool data_valid = false;
	bool flag_valid = false;

	int global_idx = 0;
	int local_idx  = 0;
	int flag = 0;
	while(true){
		int vidx = read_nb_channel_altera(next_frontier_channel0, &data_valid);
		if(data_valid){
			buffer[local_idx++] = vidx;
			if(local_idx == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					next_frontier0[global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		int tmp_flag = read_nb_channel_altera(next_frontier_end_channel0, &flag_valid);
		if(flag_valid) flag = tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}
	}

	//data left in the buffer
	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier0[global_idx++] = buffer[i];
		}
		local_idx = 0;
	}
}

// write channel 1
__kernel void __attribute__ ((task)) write_frontier1(
		int* next_frontier1
		)
{

	int buffer[BUFFER_SIZE];
	bool data_valid = false;
	bool flag_valid = false;

	int global_idx = 0;
	int local_idx  = 0;
	int flag = 0;
	while(true){
		int vidx = read_nb_channel_altera(next_frontier_channel1, &data_valid);
		if(data_valid){
			buffer[local_idx++] = vidx;
			if(local_idx == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					next_frontier1[global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		int tmp_flag = read_nb_channel_altera(next_frontier_end_channel1, &flag_valid);
		if(flag_valid) flag = tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}
	}

	//data left in the buffer
	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier1[global_idx++] = buffer[i];
		}
		local_idx = 0;
	}
}

// write channel 2
__kernel void __attribute__ ((task)) write_frontier2(
		int* next_frontier2
		)
{

	int buffer[BUFFER_SIZE];
	bool data_valid = false;
	bool flag_valid = false;

	int global_idx = 0;
	int local_idx  = 0;
	int flag = 0;
	while(true){
		int vidx = read_nb_channel_altera(next_frontier_channel2, &data_valid);
		if(data_valid){
			buffer[local_idx++] = vidx;
			if(local_idx == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					next_frontier2[global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		int tmp_flag = read_nb_channel_altera(next_frontier_end_channel2, &flag_valid);
		if(flag_valid) flag = tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}
	}

	//data left in the buffer
	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier2[global_idx++] = buffer[i];
		}
		local_idx = 0;
	}
}

// write channel 3
__kernel void __attribute__ ((task)) write_frontier3(
		int* next_frontier3
		)
{

	int buffer[BUFFER_SIZE];
	bool data_valid = false;
	bool flag_valid = false;

	int global_idx = 0;
	int local_idx  = 0;
	int flag = 0;
	while(true){
		int vidx = read_nb_channel_altera(next_frontier_channel3, &data_valid);
		if(data_valid){
			buffer[local_idx++] = vidx;
			if(local_idx == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					next_frontier3[global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		int tmp_flag = read_nb_channel_altera(next_frontier_end_channel3, &flag_valid);
		if(flag_valid) flag = tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}
	}

	//data left in the buffer
	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier3[global_idx++] = buffer[i];
		}
		local_idx = 0;
	}
}
// write channel 4
__kernel void __attribute__ ((task)) write_frontier4(
		int* next_frontier4
		)
{

	int buffer[BUFFER_SIZE];
	bool data_valid = false;
	bool flag_valid = false;

	int global_idx = 0;
	int local_idx  = 0;
	int flag = 0;
	while(true){
		int vidx = read_nb_channel_altera(next_frontier_channel4, &data_valid);
		if(data_valid){
			buffer[local_idx++] = vidx;
			if(local_idx == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					next_frontier4[global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		int tmp_flag = read_nb_channel_altera(next_frontier_end_channel4, &flag_valid);
		if(flag_valid) flag = tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}
	}

	//data left in the buffer
	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier4[global_idx++] = buffer[i];
		}
		local_idx = 0;
	}
}

// write channel 5
__kernel void __attribute__ ((task)) write_frontier5(
		int* next_frontier5
		)
{

	int buffer[BUFFER_SIZE];
	bool data_valid = false;
	bool flag_valid = false;

	int global_idx = 0;
	int local_idx  = 0;
	int flag = 0;
	while(true){
		int vidx = read_nb_channel_altera(next_frontier_channel5, &data_valid);
		if(data_valid){
			buffer[local_idx++] = vidx;
			if(local_idx == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					next_frontier5[global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		int tmp_flag = read_nb_channel_altera(next_frontier_end_channel5, &flag_valid);
		if(flag_valid) flag = tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}
	}

	//data left in the buffer
	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier5[global_idx++] = buffer[i];
		}
		local_idx = 0;
	}
}

// write channel 6
__kernel void __attribute__ ((task)) write_frontier6(
		int* next_frontier6
		)
{

	int buffer[BUFFER_SIZE];
	bool data_valid = false;
	bool flag_valid = false;

	int global_idx = 0;
	int local_idx  = 0;
	int flag = 0;
	while(true){
		int vidx = read_nb_channel_altera(next_frontier_channel6, &data_valid);
		if(data_valid){
			buffer[local_idx++] = vidx;
			if(local_idx == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					next_frontier6[global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		int tmp_flag = read_nb_channel_altera(next_frontier_end_channel6, &flag_valid);
		if(flag_valid) flag = tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}
	}

	//data left in the buffer
	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier6[global_idx++] = buffer[i];
		}
		local_idx = 0;
	}
}

//write channel 7
__kernel void __attribute__ ((task)) write_frontier7(
		int* next_frontier7
		)
{

	int buffer[BUFFER_SIZE];
	bool data_valid = false;
	bool flag_valid = false;

	int global_idx = 0;
	int local_idx  = 0;
	int flag = 0;
	while(true){
		int vidx = read_nb_channel_altera(next_frontier_channel7, &data_valid);
		if(data_valid){
			buffer[local_idx++] = vidx;
			if(local_idx == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					next_frontier7[global_idx++] = buffer[i];
				}
				local_idx = 0;
			}
		}

		int tmp_flag = read_nb_channel_altera(next_frontier_end_channel7, &flag_valid);
		if(flag_valid) flag = tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}
	}

	//data left in the buffer
	if(local_idx > 0){
		for(int i = 0; i < local_idx; i++){
			next_frontier7[global_idx++] = buffer[i];
		}
		local_idx = 0;
	}
}

