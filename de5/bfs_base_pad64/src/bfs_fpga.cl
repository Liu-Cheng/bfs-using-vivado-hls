// 8 * 32 x 16K bit bitmap memory, around 2M
#define BITMAP_DEPTH (16 * 1024) 
#define BUFFER_SIZE 16
#define PAD 64
#define CIA_LEN (PAD/16)
#define BANK_BW 6 // 3 when pad == 8, 4 when pad == 16, 5 when pad == 32, 6 when pad == 64
#define BITSEL_BW 3 // 3 when data with of the buffer is 8.

#define BITMASK0 0x1
#define BITMASK1 0x2
#define BITMASK2 0x4
#define BITMASK3 0x8
#define BITMASK4 0x10
#define BITMASK5 0x20
#define BITMASK6 0x40
#define BITMASK7 0x80

#define BANK_MASK   0x3f     //0000_0000_0000_0000_0011_1111
#define SEL_MASK    0x1C0    //0000_0000_0000_0001_1100_0000
#define OFFSET_MASK 0x3FFE00 //0011_1111_1111_1110_0000_0000

#define SW_EMU

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
			return -1;
	}
}

channel int2   rpa_channel      __attribute__ ((depth(1024)));   
channel int16  cia_channel0     __attribute__ ((depth(1024)));
channel int16  cia_channel1     __attribute__ ((depth(1024)));
channel int16  cia_channel2     __attribute__ ((depth(1024)));
channel int16  cia_channel3     __attribute__ ((depth(1024)));
channel int    cia_end_channel0 __attribute__ ((depth(4)));
channel int    cia_end_channel1 __attribute__ ((depth(4)));
channel int    cia_end_channel2 __attribute__ ((depth(4)));
channel int    cia_end_channel3 __attribute__ ((depth(4)));
channel int    nxtFrnt_channel[PAD]     __attribute__ ((depth(1024)));
channel int    nxtFrnt_end_channel[PAD] __attribute__ ((depth(4)));

__kernel void __attribute__((task)) read_rpa(
		__global int2* restrict rpa,
		const int frontier_size
		){
	for(int i = 0; i < frontier_size; i++){
		write_channel_altera(rpa_channel, rpa[i]);
	}
}
	
// Read cia of the frontier 
__kernel void __attribute__((task)) read_cia(
		__global int16* restrict cia,
		const int                frontier_size
		)
{
	for(int i = 0; i < frontier_size; i++){
		int2 word  = read_channel_altera(rpa_channel);
		int  num   = (word.s1) >> BANK_BW;
		int  start = (word.s0) >> BANK_BW;

		for(int j = 0; j < num; j++){
			int16 word0 = cia[(start + j) * CIA_LEN];
			int16 word1 = cia[(start + j) * CIA_LEN + 1];
			int16 word2 = cia[(start + j) * CIA_LEN + 2];
			int16 word3 = cia[(start + j) * CIA_LEN + 3];
			write_channel_altera(cia_channel0, word0);
			write_channel_altera(cia_channel1, word1);
			write_channel_altera(cia_channel2, word2);
			write_channel_altera(cia_channel3, word3);
		}
	}

	write_channel_altera(cia_end_channel0, 1);
	write_channel_altera(cia_end_channel1, 1);
	write_channel_altera(cia_end_channel2, 1);
	write_channel_altera(cia_end_channel3, 1);
}
// Traverse cia 
__kernel void __attribute__((task)) traverse_cia0(	
		__global int* restrict nxtFrnt_size,
		const int              root_vidx,
		const char             level
#ifdef SW_EMU
	   ,__global uchar* restrict bmap0,
		__global uchar* restrict bmap1,
		__global uchar* restrict bmap2,
		__global uchar* restrict bmap3,
		__global uchar* restrict bmap4,
		__global uchar* restrict bmap5,
		__global uchar* restrict bmap6,
		__global uchar* restrict bmap7,
		__global uchar* restrict bmap8,
		__global uchar* restrict bmap9,
		__global uchar* restrict bmap10,
		__global uchar* restrict bmap11,
		__global uchar* restrict bmap12,
		__global uchar* restrict bmap13,
		__global uchar* restrict bmap14,
		__global uchar* restrict bmap15
#endif
		)
{
	uchar bitmap0[BITMAP_DEPTH];
	uchar bitmap1[BITMAP_DEPTH];
	uchar bitmap2[BITMAP_DEPTH];
	uchar bitmap3[BITMAP_DEPTH];
	uchar bitmap4[BITMAP_DEPTH];
	uchar bitmap5[BITMAP_DEPTH];
	uchar bitmap6[BITMAP_DEPTH];
	uchar bitmap7[BITMAP_DEPTH];

	uchar bitmap8[BITMAP_DEPTH];
	uchar bitmap9[BITMAP_DEPTH];
	uchar bitmap10[BITMAP_DEPTH];
	uchar bitmap11[BITMAP_DEPTH];
	uchar bitmap12[BITMAP_DEPTH];
	uchar bitmap13[BITMAP_DEPTH];
	uchar bitmap14[BITMAP_DEPTH];
	uchar bitmap15[BITMAP_DEPTH];

	if(level == 0){
		for (int i = 0; i < BITMAP_DEPTH; i++){
			bitmap0[i] = 0;
			bitmap1[i] = 0;
			bitmap2[i] = 0;
			bitmap3[i] = 0;
			bitmap4[i] = 0;
			bitmap5[i] = 0;
			bitmap6[i] = 0;
			bitmap7[i] = 0;

			bitmap8[i] = 0;
			bitmap9[i] = 0;
			bitmap10[i] = 0;
			bitmap11[i] = 0;
			bitmap12[i] = 0;
			bitmap13[i] = 0;
			bitmap14[i] = 0;
			bitmap15[i] = 0;
		}

		int bank_idx = root_vidx & BANK_MASK;
		int bitsel   = (root_vidx & SEL_MASK) >> BANK_BW;
		int offset   = (root_vidx & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);

		if(bank_idx == 0)  bitmap0[offset]  = bitop(bitsel);
		if(bank_idx == 1)  bitmap1[offset]  = bitop(bitsel);
		if(bank_idx == 2)  bitmap2[offset]  = bitop(bitsel);
		if(bank_idx == 3)  bitmap3[offset]  = bitop(bitsel);
		if(bank_idx == 4)  bitmap4[offset]  = bitop(bitsel);
		if(bank_idx == 5)  bitmap5[offset]  = bitop(bitsel);
		if(bank_idx == 6)  bitmap6[offset]  = bitop(bitsel);
		if(bank_idx == 7)  bitmap7[offset]  = bitop(bitsel);
		if(bank_idx == 8)  bitmap8[offset]  = bitop(bitsel);
		if(bank_idx == 9)  bitmap9[offset]  = bitop(bitsel);
		if(bank_idx == 10) bitmap10[offset] = bitop(bitsel);
		if(bank_idx == 11) bitmap11[offset] = bitop(bitsel);
		if(bank_idx == 12) bitmap12[offset] = bitop(bitsel);
		if(bank_idx == 13) bitmap13[offset] = bitop(bitsel);
		if(bank_idx == 14) bitmap14[offset] = bitop(bitsel);
		if(bank_idx == 15) bitmap15[offset] = bitop(bitsel);
	}
#ifdef SW_EMU
	else{
		for(int i = 0; i < BITMAP_DEPTH; i++){
			bitmap0[i]  = bmap0[i];
			bitmap1[i]  = bmap1[i];
			bitmap2[i]  = bmap2[i];
			bitmap3[i]  = bmap3[i];
			bitmap4[i]  = bmap4[i];
			bitmap5[i]  = bmap5[i];
			bitmap6[i]  = bmap6[i];
			bitmap7[i]  = bmap7[i];
			bitmap8[i]  = bmap8[i];
			bitmap9[i]  = bmap9[i];
			bitmap10[i] = bmap10[i];
			bitmap11[i] = bmap11[i];
			bitmap12[i] = bmap12[i];
			bitmap13[i] = bmap13[i];
			bitmap14[i] = bmap14[i];
			bitmap15[i] = bmap15[i];
		}
	}
#endif

	int mem_addr0 = 0; int mem_addr8 = 0;  
	int mem_addr1 = 0; int mem_addr9 = 0;  
	int mem_addr2 = 0; int mem_addr10 = 0; 
	int mem_addr3 = 0; int mem_addr11 = 0; 
	int mem_addr4 = 0; int mem_addr12 = 0; 
	int mem_addr5 = 0; int mem_addr13 = 0; 
	int mem_addr6 = 0; int mem_addr14 = 0; 
	int mem_addr7 = 0; int mem_addr15 = 0; 

	bool data_valid = false;
	bool flag_valid = false;
	int  flag = 0;
	while(true){

		int16 word0 = read_channel_nb_altera(cia_channel0, &data_valid);
		if(data_valid){
			int bitsel0  = ((word0).s0 & SEL_MASK) >> BANK_BW;
			int bitsel1  = ((word0).s1 & SEL_MASK) >> BANK_BW;
			int bitsel2  = ((word0).s2 & SEL_MASK) >> BANK_BW;
			int bitsel3  = ((word0).s3 & SEL_MASK) >> BANK_BW;
			int bitsel4  = ((word0).s4 & SEL_MASK) >> BANK_BW;
			int bitsel5  = ((word0).s5 & SEL_MASK) >> BANK_BW;
			int bitsel6  = ((word0).s6 & SEL_MASK) >> BANK_BW;
			int bitsel7  = ((word0).s7 & SEL_MASK) >> BANK_BW;
			int bitsel8  = ((word0).s8 & SEL_MASK) >> BANK_BW;
			int bitsel9  = ((word0).s9 & SEL_MASK) >> BANK_BW;
			int bitsel10 = ((word0).sa & SEL_MASK) >> BANK_BW;
			int bitsel11 = ((word0).sb & SEL_MASK) >> BANK_BW;
			int bitsel12 = ((word0).sc & SEL_MASK) >> BANK_BW;
			int bitsel13 = ((word0).sd & SEL_MASK) >> BANK_BW;
			int bitsel14 = ((word0).se & SEL_MASK) >> BANK_BW;
			int bitsel15 = ((word0).sf & SEL_MASK) >> BANK_BW;

			int offset0  = ((word0).s0 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset1  = ((word0).s1 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset2  = ((word0).s2 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset3  = ((word0).s3 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset4  = ((word0).s4 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset5  = ((word0).s5 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset6  = ((word0).s6 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset7  = ((word0).s7 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset8  = ((word0).s8 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset9  = ((word0).s9 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset10 = ((word0).sa & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset11 = ((word0).sb & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset12 = ((word0).sc & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset13 = ((word0).sd & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset14 = ((word0).se & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset15 = ((word0).sf & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);

			if((word0).s0 != -1){
				uchar tmp = bitmap0[offset0]; 
				uchar mask = bitop(bitsel0); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[0], (word0).s0);
					bitmap0[offset0] = tmp | mask;
					mem_addr0++;
				}
			}

			if((word0).s1 != -1){
				uchar tmp = bitmap1[offset1]; 
				uchar mask = bitop(bitsel1); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[1], (word0).s1);
					bitmap1[offset1] = tmp | mask;
					mem_addr1++;
				}
			}

			if((word0).s2 != -1){
				uchar tmp = bitmap2[offset2]; 
				uchar mask = bitop(bitsel2); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[2], (word0).s2);
					bitmap2[offset2] = tmp | mask;
					mem_addr2++;
				}
			}

			if((word0).s3 != -1){
				uchar tmp = bitmap3[offset3]; 
				uchar mask = bitop(bitsel3); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[3], (word0).s3);
					bitmap3[offset3] = tmp | mask;
					mem_addr3++;
				}
			}

			if((word0).s4 != -1){
				uchar tmp = bitmap4[offset4]; 
				uchar mask = bitop(bitsel4); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[4], (word0).s4);
					bitmap4[offset4] = tmp | mask;
					mem_addr4++;
				}
			}

			if((word0).s5 != -1){
				uchar tmp = bitmap5[offset5]; 
				uchar mask = bitop(bitsel5);
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[5], (word0).s5);
					bitmap5[offset5] = tmp | mask;
					mem_addr5++;
				}
			}

			if((word0).s6 != -1){
				uchar tmp = bitmap6[offset6]; 
				uchar mask = bitop(bitsel6); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[6], (word0).s6);
					bitmap6[offset6] = tmp | mask;
					mem_addr6++; 
				}
			}

			if((word0).s7 != -1){
				uchar tmp = bitmap7[offset7]; 
				uchar mask = bitop(bitsel7); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[7], (word0).s7);
					bitmap7[offset7] = tmp | mask;
					mem_addr7++;
				}
			}

			if((word0).s8 != -1){
				uchar tmp = bitmap8[offset8]; 
				uchar mask = bitop(bitsel8); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[8], (word0).s8);
					bitmap8[offset8] = tmp | mask;
					mem_addr8++;
				}
			}

			if((word0).s9 != -1){
				uchar tmp = bitmap9[offset9]; 
				uchar mask = bitop(bitsel9); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[9], (word0).s9);
					bitmap9[offset9] = tmp | mask;
					mem_addr9++;
				}
			}

			if((word0).sa != -1){
				uchar tmp = bitmap10[offset10]; 
				uchar mask = bitop(bitsel10); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[10], (word0).sa);
					bitmap10[offset10] = tmp | mask;
					mem_addr10++;
				}
			}

			if((word0).sb != -1){
				uchar tmp = bitmap11[offset11]; 
				uchar mask = bitop(bitsel11); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[11], (word0).sb);
					bitmap11[offset11] = tmp | mask;
					mem_addr11++;
				}
			}

			if((word0).sc != -1){
				uchar tmp = bitmap12[offset12]; 
				uchar mask = bitop(bitsel12); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[12], (word0).sc);
					bitmap12[offset12] = tmp | mask;
					mem_addr12++;
				}
			}

			if((word0).sd != -1){
				uchar tmp = bitmap13[offset13]; 
				uchar mask = bitop(bitsel13);
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[13], (word0).sd);
					bitmap13[offset13] = tmp | mask;
					mem_addr13++;
				}
			}

			if((word0).se != -1){
				uchar tmp = bitmap14[offset14]; 
				uchar mask = bitop(bitsel14); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[14], (word0).se);
					bitmap14[offset14] = tmp | mask;
					mem_addr14++; 
				}
			}

			if((word0).sf != -1){
				uchar tmp = bitmap15[offset15]; 
				uchar mask = bitop(bitsel15); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[15], (word0).sf);
					bitmap15[offset15] = tmp | mask;
					mem_addr15++;
				}
			}

		}

		// Read flag channel
		int flag_tmp = read_channel_nb_altera(cia_end_channel0, &flag_valid);
		if(flag_valid) flag = flag_tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}

	}

#ifdef SW_EMU
	for(int i = 0; i < BITMAP_DEPTH; i++){
		bmap0[i]  = bitmap0[i];
		bmap1[i]  = bitmap1[i];
		bmap2[i]  = bitmap2[i];
		bmap3[i]  = bitmap3[i];
		bmap4[i]  = bitmap4[i];
		bmap5[i]  = bitmap5[i];
		bmap6[i]  = bitmap6[i];
		bmap7[i]  = bitmap7[i];
		bmap8[i]  = bitmap8[i];
		bmap9[i]  = bitmap9[i];
		bmap10[i] = bitmap10[i];
		bmap11[i] = bitmap11[i];
		bmap12[i] = bitmap12[i];
		bmap13[i] = bitmap13[i];
		bmap14[i] = bitmap14[i];
		bmap15[i] = bitmap15[i];
	}
#endif

	write_channel_altera(nxtFrnt_end_channel[0], 1);
	write_channel_altera(nxtFrnt_end_channel[1], 1);
	write_channel_altera(nxtFrnt_end_channel[2], 1);
	write_channel_altera(nxtFrnt_end_channel[3], 1);
	write_channel_altera(nxtFrnt_end_channel[4], 1);
	write_channel_altera(nxtFrnt_end_channel[5], 1);
	write_channel_altera(nxtFrnt_end_channel[6], 1);
	write_channel_altera(nxtFrnt_end_channel[7], 1);

	write_channel_altera(nxtFrnt_end_channel[8], 1);
	write_channel_altera(nxtFrnt_end_channel[9], 1);
	write_channel_altera(nxtFrnt_end_channel[10], 1);
	write_channel_altera(nxtFrnt_end_channel[11], 1);
	write_channel_altera(nxtFrnt_end_channel[12], 1);
	write_channel_altera(nxtFrnt_end_channel[13], 1);
	write_channel_altera(nxtFrnt_end_channel[14], 1);
	write_channel_altera(nxtFrnt_end_channel[15], 1);

	*(nxtFrnt_size  + 0)  = mem_addr0;
	*(nxtFrnt_size  + 1)  = mem_addr1;
	*(nxtFrnt_size  + 2)  = mem_addr2;
	*(nxtFrnt_size  + 3)  = mem_addr3;
	*(nxtFrnt_size  + 4)  = mem_addr4;
	*(nxtFrnt_size  + 5)  = mem_addr5;
	*(nxtFrnt_size  + 6)  = mem_addr6;
	*(nxtFrnt_size  + 7)  = mem_addr7;
	*(nxtFrnt_size  + 8)  = mem_addr8;
	*(nxtFrnt_size  + 9)  = mem_addr9;
	*(nxtFrnt_size  + 10) = mem_addr10;
	*(nxtFrnt_size  + 11) = mem_addr11;
	*(nxtFrnt_size  + 12) = mem_addr12;
	*(nxtFrnt_size  + 13) = mem_addr13;
	*(nxtFrnt_size  + 14) = mem_addr14;
	*(nxtFrnt_size  + 15) = mem_addr15;
}
 

// Traverse cia 
__kernel void __attribute__((task)) traverse_cia1(	
		__global int* restrict nxtFrnt_size,
		const int              root_vidx,
		const char             level
#ifdef SW_EMU
	   ,__global uchar* restrict bmap16,
		__global uchar* restrict bmap17,
		__global uchar* restrict bmap18,
		__global uchar* restrict bmap19,
		__global uchar* restrict bmap20,
		__global uchar* restrict bmap21,
		__global uchar* restrict bmap22,
		__global uchar* restrict bmap23,
		__global uchar* restrict bmap24,
		__global uchar* restrict bmap25,
		__global uchar* restrict bmap26,
		__global uchar* restrict bmap27,
		__global uchar* restrict bmap28,
		__global uchar* restrict bmap29,
		__global uchar* restrict bmap30,
		__global uchar* restrict bmap31
#endif

		)
{

	uchar bitmap16[BITMAP_DEPTH];
	uchar bitmap17[BITMAP_DEPTH];
	uchar bitmap18[BITMAP_DEPTH];
	uchar bitmap19[BITMAP_DEPTH];
	uchar bitmap20[BITMAP_DEPTH];
	uchar bitmap21[BITMAP_DEPTH];
	uchar bitmap22[BITMAP_DEPTH];
	uchar bitmap23[BITMAP_DEPTH];

	uchar bitmap24[BITMAP_DEPTH];
	uchar bitmap25[BITMAP_DEPTH];
	uchar bitmap26[BITMAP_DEPTH];
	uchar bitmap27[BITMAP_DEPTH];
	uchar bitmap28[BITMAP_DEPTH];
	uchar bitmap29[BITMAP_DEPTH];
	uchar bitmap30[BITMAP_DEPTH];
	uchar bitmap31[BITMAP_DEPTH];

	if(level == 0){
		for (int i = 0; i < BITMAP_DEPTH; i++){
			bitmap16[i] = 0;
			bitmap17[i] = 0;
			bitmap18[i] = 0;
			bitmap19[i] = 0;
			bitmap20[i] = 0;
			bitmap21[i] = 0;
			bitmap22[i] = 0;
			bitmap23[i] = 0;

			bitmap24[i] = 0;
			bitmap25[i] = 0;
			bitmap26[i] = 0;
			bitmap27[i] = 0;
			bitmap28[i] = 0;
			bitmap29[i] = 0;
			bitmap30[i] = 0;
			bitmap31[i] = 0;
		}

		int bank_idx = root_vidx & BANK_MASK;
		int bitsel   = (root_vidx & SEL_MASK) >> BANK_BW;
		int offset   = (root_vidx & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);

		if(bank_idx == 16)  bitmap16[offset] = bitop(bitsel);
		if(bank_idx == 17)  bitmap17[offset] = bitop(bitsel);
		if(bank_idx == 18)  bitmap18[offset] = bitop(bitsel);
		if(bank_idx == 19)  bitmap19[offset] = bitop(bitsel);
		if(bank_idx == 20)  bitmap20[offset] = bitop(bitsel);
		if(bank_idx == 21)  bitmap21[offset] = bitop(bitsel);
		if(bank_idx == 22)  bitmap22[offset] = bitop(bitsel);
		if(bank_idx == 23)  bitmap23[offset] = bitop(bitsel);
		if(bank_idx == 24)  bitmap24[offset] = bitop(bitsel);
		if(bank_idx == 25)  bitmap25[offset] = bitop(bitsel);
		if(bank_idx == 26)  bitmap26[offset] = bitop(bitsel);
		if(bank_idx == 27)  bitmap27[offset] = bitop(bitsel);
		if(bank_idx == 28)  bitmap28[offset] = bitop(bitsel);
		if(bank_idx == 29)  bitmap29[offset] = bitop(bitsel);
		if(bank_idx == 30)  bitmap30[offset] = bitop(bitsel);
		if(bank_idx == 31)  bitmap31[offset] = bitop(bitsel);

	}
#ifdef SW_EMU
	else{
		for(int i = 0; i < BITMAP_DEPTH; i++){
			bitmap16[i] = bmap16[i];
			bitmap17[i] = bmap17[i];
			bitmap18[i] = bmap18[i];
			bitmap19[i] = bmap19[i];
			bitmap20[i] = bmap20[i];
			bitmap21[i] = bmap21[i];
			bitmap22[i] = bmap22[i];
			bitmap23[i] = bmap23[i];
			bitmap24[i] = bmap24[i];
			bitmap25[i] = bmap25[i];
			bitmap26[i] = bmap26[i];
			bitmap27[i] = bmap27[i];
			bitmap28[i] = bmap28[i];
			bitmap29[i] = bmap29[i];
			bitmap30[i] = bmap30[i];
			bitmap31[i] = bmap31[i];
		}
	}
#endif

	int mem_addr16 = 0; int mem_addr24 = 0;
	int mem_addr17 = 0; int mem_addr25 = 0;
	int mem_addr18 = 0; int mem_addr26 = 0;
	int mem_addr19 = 0; int mem_addr27 = 0;
	int mem_addr20 = 0; int mem_addr28 = 0;
	int mem_addr21 = 0; int mem_addr29 = 0;
	int mem_addr22 = 0; int mem_addr30 = 0;
	int mem_addr23 = 0; int mem_addr31 = 0;

	bool data_valid = false;
	bool flag_valid = false;
	int  flag = 0;
	while(true){

		int16 word1 = read_channel_nb_altera(cia_channel1, &data_valid);
		if(data_valid){
			int bitsel16  = ((word1).s0 & SEL_MASK) >> BANK_BW;
			int bitsel17  = ((word1).s1 & SEL_MASK) >> BANK_BW;
			int bitsel18  = ((word1).s2 & SEL_MASK) >> BANK_BW;
			int bitsel19  = ((word1).s3 & SEL_MASK) >> BANK_BW;
			int bitsel20  = ((word1).s4 & SEL_MASK) >> BANK_BW;
			int bitsel21  = ((word1).s5 & SEL_MASK) >> BANK_BW;
			int bitsel22  = ((word1).s6 & SEL_MASK) >> BANK_BW;
			int bitsel23  = ((word1).s7 & SEL_MASK) >> BANK_BW;
			int bitsel24  = ((word1).s8 & SEL_MASK) >> BANK_BW;
			int bitsel25  = ((word1).s9 & SEL_MASK) >> BANK_BW;
			int bitsel26  = ((word1).sa & SEL_MASK) >> BANK_BW;
			int bitsel27  = ((word1).sb & SEL_MASK) >> BANK_BW;
			int bitsel28  = ((word1).sc & SEL_MASK) >> BANK_BW;
			int bitsel29  = ((word1).sd & SEL_MASK) >> BANK_BW;
			int bitsel30  = ((word1).se & SEL_MASK) >> BANK_BW;
			int bitsel31  = ((word1).sf & SEL_MASK) >> BANK_BW;

			int offset16 = ((word1).s0 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset17 = ((word1).s1 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset18 = ((word1).s2 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset19 = ((word1).s3 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset20 = ((word1).s4 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset21 = ((word1).s5 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset22 = ((word1).s6 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset23 = ((word1).s7 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset24 = ((word1).s8 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset25 = ((word1).s9 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset26 = ((word1).sa & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset27 = ((word1).sb & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset28 = ((word1).sc & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset29 = ((word1).sd & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset30 = ((word1).se & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset31 = ((word1).sf & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);

			if((word1).s0 != -1){
				uchar tmp = bitmap16[offset16]; 
				uchar mask = bitop(bitsel16); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[16], (word1).s0);
					bitmap16[offset16] = tmp | mask;
					mem_addr16++;
				}
			}

			if((word1).s1 != -1){
				uchar tmp = bitmap17[offset17]; 
				uchar mask = bitop(bitsel17); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[17], (word1).s1);
					bitmap17[offset17] = tmp | mask;
					mem_addr17++;
				}
			}

			if((word1).s2 != -1){
				uchar tmp = bitmap18[offset18]; 
				uchar mask = bitop(bitsel18); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[18], (word1).s2);
					bitmap18[offset18] = tmp | mask;
					mem_addr18++;
				}
			}

			if((word1).s3 != -1){
				uchar tmp = bitmap19[offset19]; 
				uchar mask = bitop(bitsel19); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[19], (word1).s3);
					bitmap19[offset19] = tmp | mask;
					mem_addr19++;
				}
			}

			if((word1).s4 != -1){
				uchar tmp = bitmap20[offset20]; 
				uchar mask = bitop(bitsel20); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[20], (word1).s4);
					bitmap20[offset20] = tmp | mask;
					mem_addr20++;
				}
			}

			if((word1).s5 != -1){
				uchar tmp = bitmap21[offset21]; 
				uchar mask = bitop(bitsel21);
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[21], (word1).s5);
					bitmap21[offset21] = tmp | mask;
					mem_addr21++;
				}
			}

			if((word1).s6 != -1){
				uchar tmp = bitmap22[offset22]; 
				uchar mask = bitop(bitsel22); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[22], (word1).s6);
					bitmap22[offset22] = tmp | mask;
					mem_addr22++; 
				}
			}

			if((word1).s7 != -1){
				uchar tmp = bitmap23[offset23]; 
				uchar mask = bitop(bitsel23); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[23], (word1).s7);
					bitmap23[offset23] = tmp | mask;
					mem_addr23++;
				}
			}

			if((word1).s8 != -1){
				uchar tmp = bitmap24[offset24]; 
				uchar mask = bitop(bitsel24); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[24], (word1).s8);
					bitmap24[offset24] = tmp | mask;
					mem_addr24++;
				}
			}

			if((word1).s9 != -1){
				uchar tmp = bitmap25[offset25]; 
				uchar mask = bitop(bitsel25); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[25], (word1).s9);
					bitmap25[offset25] = tmp | mask;
					mem_addr25++;
				}
			}

			if((word1).sa != -1){
				uchar tmp = bitmap26[offset26]; 
				uchar mask = bitop(bitsel26); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[26], (word1).sa);
					bitmap26[offset26] = tmp | mask;
					mem_addr26++;
				}
			}

			if((word1).sb != -1){
				uchar tmp = bitmap27[offset27]; 
				uchar mask = bitop(bitsel27); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[27], (word1).sb);
					bitmap27[offset27] = tmp | mask;
					mem_addr27++;
				}
			}

			if((word1).sc != -1){
				uchar tmp = bitmap28[offset28]; 
				uchar mask = bitop(bitsel28); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[28], (word1).sc);
					bitmap28[offset28] = tmp | mask;
					mem_addr28++;
				}
			}

			if((word1).sd != -1){
				uchar tmp = bitmap29[offset29]; 
				uchar mask = bitop(bitsel29);
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[29], (word1).sd);
					bitmap29[offset29] = tmp | mask;
					mem_addr29++;
				}
			}

			if((word1).se != -1){
				uchar tmp = bitmap30[offset30]; 
				uchar mask = bitop(bitsel30); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[30],(word1).se);
					bitmap30[offset30] = tmp | mask;
					mem_addr30++; 
				}
			}

			if((word1).sf != -1){
				uchar tmp = bitmap31[offset31]; 
				uchar mask = bitop(bitsel31); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[31], (word1).sf);
					bitmap31[offset31] = tmp | mask;
					mem_addr31++;
				}
			}

		}

		// Read flag channel
		int flag_tmp = read_channel_nb_altera(cia_end_channel1, &flag_valid);
		if(flag_valid) flag = flag_tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}

	}

#ifdef SW_EMU
	for(int i = 0; i < BITMAP_DEPTH; i++){
		bmap16[i] = bitmap16[i];
		bmap17[i] = bitmap17[i];
		bmap18[i] = bitmap18[i];
		bmap19[i] = bitmap19[i];
		bmap20[i] = bitmap20[i];
		bmap21[i] = bitmap21[i];
		bmap22[i] = bitmap22[i];
		bmap23[i] = bitmap23[i];
		bmap24[i] = bitmap24[i];
		bmap25[i] = bitmap25[i];
		bmap26[i] = bitmap26[i];
		bmap27[i] = bitmap27[i];
		bmap28[i] = bitmap28[i];
		bmap29[i] = bitmap29[i];
		bmap30[i] = bitmap30[i];
		bmap31[i] = bitmap31[i];
	}
#endif


	write_channel_altera(nxtFrnt_end_channel[16], 1);
	write_channel_altera(nxtFrnt_end_channel[17], 1);
	write_channel_altera(nxtFrnt_end_channel[18], 1);
	write_channel_altera(nxtFrnt_end_channel[19], 1);
	write_channel_altera(nxtFrnt_end_channel[20], 1);
	write_channel_altera(nxtFrnt_end_channel[21], 1);
	write_channel_altera(nxtFrnt_end_channel[22], 1);
	write_channel_altera(nxtFrnt_end_channel[23], 1);
	write_channel_altera(nxtFrnt_end_channel[24], 1);
	write_channel_altera(nxtFrnt_end_channel[25], 1);
	write_channel_altera(nxtFrnt_end_channel[26], 1);
	write_channel_altera(nxtFrnt_end_channel[27], 1);
	write_channel_altera(nxtFrnt_end_channel[28], 1);
	write_channel_altera(nxtFrnt_end_channel[29], 1);
	write_channel_altera(nxtFrnt_end_channel[30], 1);
	write_channel_altera(nxtFrnt_end_channel[31], 1);

	*(nxtFrnt_size  + 16)  = mem_addr16;
	*(nxtFrnt_size  + 17)  = mem_addr17;
	*(nxtFrnt_size  + 18)  = mem_addr18;
	*(nxtFrnt_size  + 19)  = mem_addr19;
	*(nxtFrnt_size  + 20)  = mem_addr20;
	*(nxtFrnt_size  + 21)  = mem_addr21;
	*(nxtFrnt_size  + 22)  = mem_addr22;
	*(nxtFrnt_size  + 23)  = mem_addr23;
	*(nxtFrnt_size  + 24)  = mem_addr24;
	*(nxtFrnt_size  + 25)  = mem_addr25;
	*(nxtFrnt_size  + 26)  = mem_addr26;
	*(nxtFrnt_size  + 27)  = mem_addr27;
	*(nxtFrnt_size  + 28)  = mem_addr28;
	*(nxtFrnt_size  + 29)  = mem_addr29;
	*(nxtFrnt_size  + 30)  = mem_addr30;
	*(nxtFrnt_size  + 31)  = mem_addr31;
}
 
// Traverse cia 
__kernel void __attribute__((task)) traverse_cia2(	
		__global int* restrict nxtFrnt_size,
		const int              root_vidx,
		const char             level
#ifdef SW_EMU
	   ,__global uchar* restrict bmap32,
		__global uchar* restrict bmap33,
		__global uchar* restrict bmap34,
		__global uchar* restrict bmap35,
		__global uchar* restrict bmap36,
		__global uchar* restrict bmap37,
		__global uchar* restrict bmap38,
		__global uchar* restrict bmap39,
		__global uchar* restrict bmap40,
		__global uchar* restrict bmap41,
		__global uchar* restrict bmap42,
		__global uchar* restrict bmap43,
		__global uchar* restrict bmap44,
		__global uchar* restrict bmap45,
		__global uchar* restrict bmap46,
		__global uchar* restrict bmap47
#endif

		)
{

	uchar bitmap32[BITMAP_DEPTH];
	uchar bitmap33[BITMAP_DEPTH];
	uchar bitmap34[BITMAP_DEPTH];
	uchar bitmap35[BITMAP_DEPTH];
	uchar bitmap36[BITMAP_DEPTH];
	uchar bitmap37[BITMAP_DEPTH];
	uchar bitmap38[BITMAP_DEPTH];
	uchar bitmap39[BITMAP_DEPTH];

	uchar bitmap40[BITMAP_DEPTH];
	uchar bitmap41[BITMAP_DEPTH];
	uchar bitmap42[BITMAP_DEPTH];
	uchar bitmap43[BITMAP_DEPTH];
	uchar bitmap44[BITMAP_DEPTH];
	uchar bitmap45[BITMAP_DEPTH];
	uchar bitmap46[BITMAP_DEPTH];
	uchar bitmap47[BITMAP_DEPTH];

	if(level == 0){
		for (int i = 0; i < BITMAP_DEPTH; i++){
			bitmap32[i] = 0;
			bitmap33[i] = 0;
			bitmap34[i] = 0;
			bitmap35[i] = 0;
			bitmap36[i] = 0;
			bitmap37[i] = 0;
			bitmap38[i] = 0;
			bitmap39[i] = 0;
                    
			bitmap40[i] = 0;
			bitmap41[i] = 0;
			bitmap42[i] = 0;
			bitmap43[i] = 0;
			bitmap44[i] = 0;
			bitmap45[i] = 0;
			bitmap46[i] = 0;
			bitmap47[i] = 0;
		}

		int bank_idx = root_vidx & BANK_MASK;
		int bitsel   = (root_vidx & SEL_MASK) >> BANK_BW;
		int offset   = (root_vidx & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);

		if(bank_idx == 32)  bitmap32[offset] = bitop(bitsel);
		if(bank_idx == 33)  bitmap33[offset] = bitop(bitsel);
		if(bank_idx == 34)  bitmap34[offset] = bitop(bitsel);
		if(bank_idx == 35)  bitmap35[offset] = bitop(bitsel);
		if(bank_idx == 36)  bitmap36[offset] = bitop(bitsel);
		if(bank_idx == 37)  bitmap37[offset] = bitop(bitsel);
		if(bank_idx == 38)  bitmap38[offset] = bitop(bitsel);
		if(bank_idx == 39)  bitmap39[offset] = bitop(bitsel);
		if(bank_idx == 40)  bitmap40[offset] = bitop(bitsel);
		if(bank_idx == 41)  bitmap41[offset] = bitop(bitsel);
		if(bank_idx == 42)  bitmap42[offset] = bitop(bitsel);
		if(bank_idx == 43)  bitmap43[offset] = bitop(bitsel);
		if(bank_idx == 44)  bitmap44[offset] = bitop(bitsel);
		if(bank_idx == 45)  bitmap45[offset] = bitop(bitsel);
		if(bank_idx == 46)  bitmap46[offset] = bitop(bitsel);
		if(bank_idx == 47)  bitmap47[offset] = bitop(bitsel);

	}
#ifdef SW_EMU
	else{
		for(int i = 0; i < BITMAP_DEPTH; i++){
			bitmap32[i] = bmap32[i];
			bitmap33[i] = bmap33[i];
			bitmap34[i] = bmap34[i];
			bitmap35[i] = bmap35[i];
			bitmap36[i] = bmap36[i];
			bitmap37[i] = bmap37[i];
			bitmap38[i] = bmap38[i];
			bitmap39[i] = bmap39[i];
			bitmap40[i] = bmap40[i];
			bitmap41[i] = bmap41[i];
			bitmap42[i] = bmap42[i];
			bitmap43[i] = bmap43[i];
			bitmap44[i] = bmap44[i];
			bitmap45[i] = bmap45[i];
			bitmap46[i] = bmap46[i];
			bitmap47[i] = bmap47[i];
		}
	}
#endif

	int mem_addr32= 0; 
	int mem_addr33= 0; 
	int mem_addr34= 0; 
	int mem_addr35= 0; 
	int mem_addr36= 0; 
	int mem_addr37= 0; 
	int mem_addr38= 0; 
	int mem_addr39= 0; 
                  
	int mem_addr40= 0;
	int mem_addr41= 0;
	int mem_addr42= 0;
	int mem_addr43= 0;
	int mem_addr44= 0;
	int mem_addr45= 0;
	int mem_addr46= 0;
	int mem_addr47= 0;

	bool data_valid = false;
	bool flag_valid = false;
	int  flag = 0;
	while(true){

		int16 word2 = read_channel_nb_altera(cia_channel2, &data_valid);
		if(data_valid){
			int bitsel32 = ((word2).s0 & SEL_MASK) >> BANK_BW;
			int bitsel33 = ((word2).s1 & SEL_MASK) >> BANK_BW;
			int bitsel34 = ((word2).s2 & SEL_MASK) >> BANK_BW;
			int bitsel35 = ((word2).s3 & SEL_MASK) >> BANK_BW;
			int bitsel36 = ((word2).s4 & SEL_MASK) >> BANK_BW;
			int bitsel37 = ((word2).s5 & SEL_MASK) >> BANK_BW;
			int bitsel38 = ((word2).s6 & SEL_MASK) >> BANK_BW;
			int bitsel39 = ((word2).s7 & SEL_MASK) >> BANK_BW;
			int bitsel40 = ((word2).s8 & SEL_MASK) >> BANK_BW;
			int bitsel41 = ((word2).s9 & SEL_MASK) >> BANK_BW;
			int bitsel42 = ((word2).sa & SEL_MASK) >> BANK_BW;
			int bitsel43 = ((word2).sb & SEL_MASK) >> BANK_BW;
			int bitsel44 = ((word2).sc & SEL_MASK) >> BANK_BW;
			int bitsel45 = ((word2).sd & SEL_MASK) >> BANK_BW;
			int bitsel46 = ((word2).se & SEL_MASK) >> BANK_BW;
			int bitsel47 = ((word2).sf & SEL_MASK) >> BANK_BW;
                        
			int offset32 = ((word2).s0 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset33 = ((word2).s1 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset34 = ((word2).s2 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset35 = ((word2).s3 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset36 = ((word2).s4 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset37 = ((word2).s5 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset38 = ((word2).s6 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset39 = ((word2).s7 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset40 = ((word2).s8 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset41 = ((word2).s9 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset42 = ((word2).sa & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset43 = ((word2).sb & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset44 = ((word2).sc & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset45 = ((word2).sd & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset46 = ((word2).se & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset47 = ((word2).sf & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);

			if((word2).s0 != -1){
				uchar tmp = bitmap32[offset32]; 
				uchar mask = bitop(bitsel32); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[32], (word2).s0);
					bitmap32[offset32] = tmp | mask;
					mem_addr32++;
				}
			}

			if((word2).s1 != -1){
				uchar tmp = bitmap33[offset33]; 
				uchar mask = bitop(bitsel33); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[33], (word2).s1);
					bitmap33[offset33] = tmp | mask;
					mem_addr33++;
				}
			}

			if((word2).s2 != -1){
				uchar tmp = bitmap34[offset34]; 
				uchar mask = bitop(bitsel34); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[34], (word2).s2);
					bitmap34[offset34] = tmp | mask;
					mem_addr34++;
				}
			}

			if((word2).s3 != -1){
				uchar tmp = bitmap35[offset35]; 
				uchar mask = bitop(bitsel35); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[35], (word2).s3);
					bitmap35[offset35] = tmp | mask;
					mem_addr35++;
				}
			}

			if((word2).s4 != -1){
				uchar tmp = bitmap36[offset36]; 
				uchar mask = bitop(bitsel36); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[36], (word2).s4);
					bitmap36[offset36] = tmp | mask;
					mem_addr36++;
				}
			}

			if((word2).s5 != -1){
				uchar tmp = bitmap37[offset37]; 
				uchar mask = bitop(bitsel37);
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[37], (word2).s5);
					bitmap37[offset37] = tmp | mask;
					mem_addr37++;
				}
			}

			if((word2).s6 != -1){
				uchar tmp = bitmap38[offset38]; 
				uchar mask = bitop(bitsel38); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[38], (word2).s6);
					bitmap38[offset38] = tmp | mask;
					mem_addr38++; 
				}
			}

			if((word2).s7 != -1){
				uchar tmp = bitmap39[offset39]; 
				uchar mask = bitop(bitsel39); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[39], (word2).s7);
					bitmap39[offset39] = tmp | mask;
					mem_addr39++;
				}
			}

			if((word2).s8 != -1){
				uchar tmp = bitmap40[offset40]; 
				uchar mask = bitop(bitsel40); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[40], (word2).s8);
					bitmap40[offset40] = tmp | mask;
					mem_addr40++;
				}
			}

			if((word2).s9 != -1){
				uchar tmp = bitmap41[offset41]; 
				uchar mask = bitop(bitsel41); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[41], (word2).s9);
					bitmap41[offset41] = tmp | mask;
					mem_addr41++;
				}
			}

			if((word2).sa != -1){
				uchar tmp = bitmap42[offset42]; 
				uchar mask = bitop(bitsel42); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[42], (word2).sa);
					bitmap42[offset42] = tmp | mask;
					mem_addr42++;
				}
			}

			if((word2).sb != -1){
				uchar tmp = bitmap43[offset43]; 
				uchar mask = bitop(bitsel43); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[43], (word2).sb);
					bitmap43[offset43] = tmp | mask;
					mem_addr43++;
				}
			}

			if((word2).sc != -1){
				uchar tmp = bitmap44[offset44]; 
				uchar mask = bitop(bitsel44); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[44], (word2).sc);
					bitmap44[offset44] = tmp | mask;
					mem_addr44++;
				}
			}

			if((word2).sd != -1){
				uchar tmp = bitmap45[offset45]; 
				uchar mask = bitop(bitsel45);
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[45], (word2).sd);
					bitmap45[offset45] = tmp | mask;
					mem_addr45++;
				}
			}

			if((word2).se != -1){
				uchar tmp = bitmap46[offset46]; 
				uchar mask = bitop(bitsel46); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[46],(word2).se);
					bitmap46[offset46] = tmp | mask;
					mem_addr46++; 
				}
			}

			if((word2).sf != -1){
				uchar tmp = bitmap47[offset47]; 
				uchar mask = bitop(bitsel47); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[47], (word2).sf);
					bitmap47[offset47] = tmp | mask;
					mem_addr47++;
				}
			}

		}

		// Read flag channel
		int flag_tmp = read_channel_nb_altera(cia_end_channel2, &flag_valid);
		if(flag_valid) flag = flag_tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}

	}

#ifdef SW_EMU
	for(int i = 0; i < BITMAP_DEPTH; i++){
		bmap32[i] = bitmap32[i];
		bmap33[i] = bitmap33[i];
		bmap34[i] = bitmap34[i];
		bmap35[i] = bitmap35[i];
		bmap36[i] = bitmap36[i];
		bmap37[i] = bitmap37[i];
		bmap38[i] = bitmap38[i];
		bmap39[i] = bitmap39[i];
		bmap40[i] = bitmap40[i];
		bmap41[i] = bitmap41[i];
		bmap42[i] = bitmap42[i];
		bmap43[i] = bitmap43[i];
		bmap44[i] = bitmap44[i];
		bmap45[i] = bitmap45[i];
		bmap46[i] = bitmap46[i];
		bmap47[i] = bitmap47[i];
	}
#endif

	write_channel_altera(nxtFrnt_end_channel[32], 1);
	write_channel_altera(nxtFrnt_end_channel[33], 1);
	write_channel_altera(nxtFrnt_end_channel[34], 1);
	write_channel_altera(nxtFrnt_end_channel[35], 1);
	write_channel_altera(nxtFrnt_end_channel[36], 1);
	write_channel_altera(nxtFrnt_end_channel[37], 1);
	write_channel_altera(nxtFrnt_end_channel[38], 1);
	write_channel_altera(nxtFrnt_end_channel[39], 1);
	write_channel_altera(nxtFrnt_end_channel[40], 1);
	write_channel_altera(nxtFrnt_end_channel[41], 1);
	write_channel_altera(nxtFrnt_end_channel[42], 1);
	write_channel_altera(nxtFrnt_end_channel[43], 1);
	write_channel_altera(nxtFrnt_end_channel[44], 1);
	write_channel_altera(nxtFrnt_end_channel[45], 1);
	write_channel_altera(nxtFrnt_end_channel[46], 1);
	write_channel_altera(nxtFrnt_end_channel[47], 1);

	*(nxtFrnt_size  + 32)  = mem_addr32;
	*(nxtFrnt_size  + 33)  = mem_addr33;
	*(nxtFrnt_size  + 34)  = mem_addr34;
	*(nxtFrnt_size  + 35)  = mem_addr35;
	*(nxtFrnt_size  + 36)  = mem_addr36;
	*(nxtFrnt_size  + 37)  = mem_addr37;
	*(nxtFrnt_size  + 38)  = mem_addr38;
	*(nxtFrnt_size  + 39)  = mem_addr39;
	*(nxtFrnt_size  + 40)  = mem_addr40;
	*(nxtFrnt_size  + 41)  = mem_addr41;
	*(nxtFrnt_size  + 42)  = mem_addr42;
	*(nxtFrnt_size  + 43)  = mem_addr43;
	*(nxtFrnt_size  + 44)  = mem_addr44;
	*(nxtFrnt_size  + 45)  = mem_addr45;
	*(nxtFrnt_size  + 46)  = mem_addr46;
	*(nxtFrnt_size  + 47)  = mem_addr47;
}
 
// Traverse cia 
__kernel void __attribute__((task)) traverse_cia3(	
		__global int* restrict nxtFrnt_size,
		const int              root_vidx,
		const char             level
#ifdef SW_EMU
	   ,__global uchar* restrict bmap48,
		__global uchar* restrict bmap49,
		__global uchar* restrict bmap50,
		__global uchar* restrict bmap51,
		__global uchar* restrict bmap52,
		__global uchar* restrict bmap53,
		__global uchar* restrict bmap54,
		__global uchar* restrict bmap55,
		__global uchar* restrict bmap56,
		__global uchar* restrict bmap57,
		__global uchar* restrict bmap58,
		__global uchar* restrict bmap59,
		__global uchar* restrict bmap60,
		__global uchar* restrict bmap61,
		__global uchar* restrict bmap62,
		__global uchar* restrict bmap63
#endif

		)
{

	uchar bitmap48[BITMAP_DEPTH];
	uchar bitmap49[BITMAP_DEPTH];
	uchar bitmap50[BITMAP_DEPTH];
	uchar bitmap51[BITMAP_DEPTH];
	uchar bitmap52[BITMAP_DEPTH];
	uchar bitmap53[BITMAP_DEPTH];
	uchar bitmap54[BITMAP_DEPTH];
	uchar bitmap55[BITMAP_DEPTH];
	uchar bitmap56[BITMAP_DEPTH];
	uchar bitmap57[BITMAP_DEPTH];
	uchar bitmap58[BITMAP_DEPTH];
	uchar bitmap59[BITMAP_DEPTH];
	uchar bitmap60[BITMAP_DEPTH];
	uchar bitmap61[BITMAP_DEPTH];
	uchar bitmap62[BITMAP_DEPTH];
	uchar bitmap63[BITMAP_DEPTH];

	if(level == 0){
		for (int i = 0; i < BITMAP_DEPTH; i++){
			bitmap48[i] = 0;
			bitmap49[i] = 0;
			bitmap50[i] = 0;
			bitmap51[i] = 0;
			bitmap52[i] = 0;
			bitmap53[i] = 0;
			bitmap54[i] = 0;
			bitmap55[i] = 0; 
			bitmap56[i] = 0;
			bitmap57[i] = 0;
			bitmap58[i] = 0;
			bitmap59[i] = 0;
			bitmap60[i] = 0;
			bitmap61[i] = 0;
			bitmap62[i] = 0;
			bitmap63[i] = 0;
		}

		int bank_idx = root_vidx & BANK_MASK;
		int bitsel   = (root_vidx & SEL_MASK) >> BANK_BW;
		int offset   = (root_vidx & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);

		if(bank_idx == 48)  bitmap48[offset] = bitop(bitsel);
		if(bank_idx == 49)  bitmap49[offset] = bitop(bitsel);
		if(bank_idx == 50)  bitmap50[offset] = bitop(bitsel);
		if(bank_idx == 51)  bitmap51[offset] = bitop(bitsel);
		if(bank_idx == 52)  bitmap52[offset] = bitop(bitsel);
		if(bank_idx == 53)  bitmap53[offset] = bitop(bitsel);
		if(bank_idx == 54)  bitmap54[offset] = bitop(bitsel);
		if(bank_idx == 55)  bitmap55[offset] = bitop(bitsel);
		if(bank_idx == 56)  bitmap56[offset] = bitop(bitsel);
		if(bank_idx == 57)  bitmap57[offset] = bitop(bitsel);
		if(bank_idx == 58)  bitmap58[offset] = bitop(bitsel);
		if(bank_idx == 59)  bitmap59[offset] = bitop(bitsel);
		if(bank_idx == 60)  bitmap60[offset] = bitop(bitsel);
		if(bank_idx == 61)  bitmap61[offset] = bitop(bitsel);
		if(bank_idx == 62)  bitmap62[offset] = bitop(bitsel);
		if(bank_idx == 63)  bitmap63[offset] = bitop(bitsel);

	}
#ifdef SW_EMU
	else{
		for(int i = 0; i < BITMAP_DEPTH; i++){
			bitmap48[i] = bmap48[i];
			bitmap49[i] = bmap49[i];
			bitmap50[i] = bmap50[i];
			bitmap51[i] = bmap51[i];
			bitmap52[i] = bmap52[i];
			bitmap53[i] = bmap53[i];
			bitmap54[i] = bmap54[i];
			bitmap55[i] = bmap55[i];
			bitmap56[i] = bmap56[i];
			bitmap57[i] = bmap57[i];
			bitmap58[i] = bmap58[i];
			bitmap59[i] = bmap59[i];
			bitmap60[i] = bmap60[i];
			bitmap61[i] = bmap61[i];
			bitmap62[i] = bmap62[i];
			bitmap63[i] = bmap63[i];
		}
	}
#endif

	int mem_addr48= 0; 
	int mem_addr49= 0; 
	int mem_addr50= 0; 
	int mem_addr51= 0; 
	int mem_addr52= 0; 
	int mem_addr53= 0; 
	int mem_addr54= 0; 
	int mem_addr55= 0;    
	int mem_addr56= 0;
	int mem_addr57= 0;
	int mem_addr58= 0;
	int mem_addr59= 0;
	int mem_addr60= 0;
	int mem_addr61= 0;
	int mem_addr62= 0;
	int mem_addr63= 0;

	bool data_valid = false;
	bool flag_valid = false;
	int  flag = 0;
	while(true){

		int16 word3 = read_channel_nb_altera(cia_channel3, &data_valid);
		if(data_valid){
			int bitsel48 = ((word3).s0 & SEL_MASK) >> BANK_BW;
			int bitsel49 = ((word3).s1 & SEL_MASK) >> BANK_BW;
			int bitsel50 = ((word3).s2 & SEL_MASK) >> BANK_BW;
			int bitsel51 = ((word3).s3 & SEL_MASK) >> BANK_BW;
			int bitsel52 = ((word3).s4 & SEL_MASK) >> BANK_BW;
			int bitsel53 = ((word3).s5 & SEL_MASK) >> BANK_BW;
			int bitsel54 = ((word3).s6 & SEL_MASK) >> BANK_BW;
			int bitsel55 = ((word3).s7 & SEL_MASK) >> BANK_BW;
			int bitsel56 = ((word3).s8 & SEL_MASK) >> BANK_BW;
			int bitsel57 = ((word3).s9 & SEL_MASK) >> BANK_BW;
			int bitsel58 = ((word3).sa & SEL_MASK) >> BANK_BW;
			int bitsel59 = ((word3).sb & SEL_MASK) >> BANK_BW;
			int bitsel60 = ((word3).sc & SEL_MASK) >> BANK_BW;
			int bitsel61 = ((word3).sd & SEL_MASK) >> BANK_BW;
			int bitsel62 = ((word3).se & SEL_MASK) >> BANK_BW;
			int bitsel63 = ((word3).sf & SEL_MASK) >> BANK_BW;
                        
			int offset48 = ((word3).s0 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset49 = ((word3).s1 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset50 = ((word3).s2 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset51 = ((word3).s3 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset52 = ((word3).s4 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset53 = ((word3).s5 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset54 = ((word3).s6 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset55 = ((word3).s7 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset56 = ((word3).s8 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset57 = ((word3).s9 & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset58 = ((word3).sa & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset59 = ((word3).sb & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset60 = ((word3).sc & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset61 = ((word3).sd & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset62 = ((word3).se & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);
			int offset63 = ((word3).sf & OFFSET_MASK) >> (BANK_BW + BITSEL_BW);

			if((word3).s0 != -1){
				uchar tmp = bitmap48[offset48]; 
				uchar mask = bitop(bitsel48); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[48], (word3).s0);
					bitmap48[offset48] = tmp | mask;
					mem_addr48++;
				}
			}

			if((word3).s1 != -1){
				uchar tmp = bitmap49[offset49]; 
				uchar mask = bitop(bitsel49); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[49], (word3).s1);
					bitmap49[offset49] = tmp | mask;
					mem_addr49++;
				}
			}

			if((word3).s2 != -1){
				uchar tmp = bitmap50[offset50]; 
				uchar mask = bitop(bitsel50); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[50], (word3).s2);
					bitmap50[offset50] = tmp | mask;
					mem_addr50++;
				}
			}

			if((word3).s3 != -1){
				uchar tmp = bitmap51[offset51]; 
				uchar mask = bitop(bitsel51); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[51], (word3).s3);
					bitmap51[offset51] = tmp | mask;
					mem_addr51++;
				}
			}

			if((word3).s4 != -1){
				uchar tmp = bitmap52[offset52]; 
				uchar mask = bitop(bitsel52); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[52], (word3).s4);
					bitmap52[offset52] = tmp | mask;
					mem_addr52++;
				}
			}

			if((word3).s5 != -1){
				uchar tmp = bitmap53[offset53]; 
				uchar mask = bitop(bitsel53);
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[53], (word3).s5);
					bitmap53[offset53] = tmp | mask;
					mem_addr53++;
				}
			}

			if((word3).s6 != -1){
				uchar tmp = bitmap54[offset54]; 
				uchar mask = bitop(bitsel54); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[54], (word3).s6);
					bitmap54[offset54] = tmp | mask;
					mem_addr54++; 
				}
			}

			if((word3).s7 != -1){
				uchar tmp = bitmap55[offset55]; 
				uchar mask = bitop(bitsel55); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[55], (word3).s7);
					bitmap55[offset55] = tmp | mask;
					mem_addr55++;
				}
			}

			if((word3).s8 != -1){
				uchar tmp = bitmap56[offset56]; 
				uchar mask = bitop(bitsel56); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[56], (word3).s8);
					bitmap56[offset56] = tmp | mask;
					mem_addr56++;
				}
			}

			if((word3).s9 != -1){
				uchar tmp = bitmap57[offset57]; 
				uchar mask = bitop(bitsel57); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[57], (word3).s9);
					bitmap57[offset57] = tmp | mask;
					mem_addr57++;
				}
			}

			if((word3).sa != -1){
				uchar tmp = bitmap58[offset58]; 
				uchar mask = bitop(bitsel58); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[58], (word3).sa);
					bitmap58[offset58] = tmp | mask;
					mem_addr58++;
				}
			}

			if((word3).sb != -1){
				uchar tmp = bitmap59[offset59]; 
				uchar mask = bitop(bitsel59); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[59], (word3).sb);
					bitmap59[offset59] = tmp | mask;
					mem_addr59++;
				}
			}

			if((word3).sc != -1){
				uchar tmp = bitmap60[offset60]; 
				uchar mask = bitop(bitsel60); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[60], (word3).sc);
					bitmap60[offset60] = tmp | mask;
					mem_addr60++;
				}
			}

			if((word3).sd != -1){
				uchar tmp = bitmap61[offset61]; 
				uchar mask = bitop(bitsel61);
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[61], (word3).sd);
					bitmap61[offset61] = tmp | mask;
					mem_addr61++;
				}
			}

			if((word3).se != -1){
				uchar tmp = bitmap62[offset62]; 
				uchar mask = bitop(bitsel62); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[62],(word3).se);
					bitmap62[offset62] = tmp | mask;
					mem_addr62++; 
				}
			}

			if((word3).sf != -1){
				uchar tmp = bitmap63[offset63]; 
				uchar mask = bitop(bitsel63); 
				if((tmp & mask) == 0){
					write_channel_altera(nxtFrnt_channel[63], (word3).sf);
					bitmap63[offset63] = tmp | mask;
					mem_addr63++;
				}
			}

		}

		// Read flag channel
		int flag_tmp = read_channel_nb_altera(cia_end_channel3, &flag_valid);
		if(flag_valid) flag = flag_tmp;
		if(flag == 1 && !data_valid && !flag_valid){
			break;
		}

	}

#ifdef SW_EMU
	for(int i = 0; i < BITMAP_DEPTH; i++){
		bmap48[i] = bitmap48[i];
		bmap49[i] = bitmap49[i];
		bmap50[i] = bitmap50[i];
		bmap51[i] = bitmap51[i];
		bmap52[i] = bitmap52[i];
		bmap53[i] = bitmap53[i];
		bmap54[i] = bitmap54[i];
		bmap55[i] = bitmap55[i];
		bmap56[i] = bitmap56[i];
		bmap57[i] = bitmap57[i];
		bmap58[i] = bitmap58[i];
		bmap59[i] = bitmap59[i];
		bmap60[i] = bitmap60[i];
		bmap61[i] = bitmap61[i];
		bmap62[i] = bitmap62[i];
		bmap63[i] = bitmap63[i];
	}
#endif

	write_channel_altera(nxtFrnt_end_channel[48], 1);
	write_channel_altera(nxtFrnt_end_channel[49], 1);
	write_channel_altera(nxtFrnt_end_channel[50], 1);
	write_channel_altera(nxtFrnt_end_channel[51], 1);
	write_channel_altera(nxtFrnt_end_channel[52], 1);
	write_channel_altera(nxtFrnt_end_channel[53], 1);
	write_channel_altera(nxtFrnt_end_channel[54], 1);
	write_channel_altera(nxtFrnt_end_channel[55], 1);
	write_channel_altera(nxtFrnt_end_channel[56], 1);
	write_channel_altera(nxtFrnt_end_channel[57], 1);
	write_channel_altera(nxtFrnt_end_channel[58], 1);
	write_channel_altera(nxtFrnt_end_channel[59], 1);
	write_channel_altera(nxtFrnt_end_channel[60], 1);
	write_channel_altera(nxtFrnt_end_channel[61], 1);
	write_channel_altera(nxtFrnt_end_channel[62], 1);
	write_channel_altera(nxtFrnt_end_channel[63], 1);

	*(nxtFrnt_size + 48)  = mem_addr48;
	*(nxtFrnt_size + 49)  = mem_addr49;
	*(nxtFrnt_size + 50)  = mem_addr50;
	*(nxtFrnt_size + 51)  = mem_addr51;
	*(nxtFrnt_size + 52)  = mem_addr52;
	*(nxtFrnt_size + 53)  = mem_addr53;
	*(nxtFrnt_size + 54)  = mem_addr54;
	*(nxtFrnt_size + 55)  = mem_addr55;
	*(nxtFrnt_size + 56)  = mem_addr56;
	*(nxtFrnt_size + 57)  = mem_addr57;
	*(nxtFrnt_size + 58)  = mem_addr58;
	*(nxtFrnt_size + 59)  = mem_addr59;
	*(nxtFrnt_size + 60)  = mem_addr60;
	*(nxtFrnt_size + 61)  = mem_addr61;
	*(nxtFrnt_size + 62)  = mem_addr62;
	*(nxtFrnt_size + 63)  = mem_addr63;
}

// write channel0
__kernel void __attribute__ ((task)) write_frontier0to3(
		__global int* restrict nxtFrnt0,
		__global int* restrict nxtFrnt1,
		__global int* restrict nxtFrnt2,
		__global int* restrict nxtFrnt3
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[0], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[1], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[2], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[3], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt0[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt1[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt2[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt3[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[0], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[1], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[2], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[3], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt0[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt1[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt2[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt3[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}
}

// write channel 4
__kernel void __attribute__ ((task)) write_frontier4to7(
	__global int* restrict nxtFrnt4,
	__global int* restrict nxtFrnt5,
	__global int* restrict nxtFrnt6,
	__global int* restrict nxtFrnt7
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[4], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[5], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[6], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[7], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt4[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt5[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt6[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt7[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[4], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[5], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[6], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[7], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt4[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt5[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt6[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt7[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

// write channel 8 to 11
__kernel void __attribute__ ((task)) write_frontier8to11(
	__global int* restrict nxtFrnt8,
	__global int* restrict nxtFrnt9,
	__global int* restrict nxtFrnt10,
	__global int* restrict nxtFrnt11
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[8], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[9], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[10], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[11], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt8[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt9[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt10[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt11[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[8], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[9], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[10], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[11], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt8[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt9[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt10[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt11[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}
}

// write channel 6
__kernel void __attribute__ ((task)) write_frontier12to15(
	__global int* restrict nxtFrnt12,
	__global int* restrict nxtFrnt13,
	__global int* restrict nxtFrnt14,
	__global int* restrict nxtFrnt15
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[12], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[13], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[14], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[15], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt12[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt13[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt14[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt15[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[12], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[13], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[14], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[15], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt12[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt13[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt14[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt15[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

//write channel 7
__kernel void __attribute__ ((task)) write_frontier16to19(
	__global int* restrict nxtFrnt16,
	__global int* restrict nxtFrnt17,
	__global int* restrict nxtFrnt18,
	__global int* restrict nxtFrnt19
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[16], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[17], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[18], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[19], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt16[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt17[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt18[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt19[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[16], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[17], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[18], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[19], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt16[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt17[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt18[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt19[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

// write channel 20
__kernel void __attribute__ ((task)) write_frontier20to23(
	__global int* restrict nxtFrnt20,
	__global int* restrict nxtFrnt21,
	__global int* restrict nxtFrnt22,
	__global int* restrict nxtFrnt23
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[20], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[21], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[22], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[23], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt20[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt21[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt22[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt23[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[20], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[21], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[22], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[23], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt20[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt21[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt22[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt23[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

// write channel 24 to 27
__kernel void __attribute__ ((task)) write_frontier24to27(
	__global int* restrict nxtFrnt24,
	__global int* restrict nxtFrnt25,
	__global int* restrict nxtFrnt26,
	__global int* restrict nxtFrnt27
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[24], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[25], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[26], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[27], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt24[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt25[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt26[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt27[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[24], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[25], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[26], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[27], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt24[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt25[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt26[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt27[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

// write channel 28
__kernel void __attribute__ ((task)) write_frontier28to31(
	__global int* restrict nxtFrnt28,
	__global int* restrict nxtFrnt29,
	__global int* restrict nxtFrnt30,
	__global int* restrict nxtFrnt31
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[28], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[29], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[30], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[31], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt28[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt29[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt30[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt31[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[28], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[29], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[30], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[31], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt28[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt29[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt30[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt31[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

// write channel 5
__kernel void __attribute__ ((task)) write_frontier32to35(
	__global int* restrict nxtFrnt32,
	__global int* restrict nxtFrnt33,
	__global int* restrict nxtFrnt34,
	__global int* restrict nxtFrnt35
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[32], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[33], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[34], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[35], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt32[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt33[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt34[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt35[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[32], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[33], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[34], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[35], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt32[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt33[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt34[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt35[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

// write channel 36
__kernel void __attribute__ ((task)) write_frontier36to39(
	__global int* restrict nxtFrnt36,
	__global int* restrict nxtFrnt37,
	__global int* restrict nxtFrnt38,
	__global int* restrict nxtFrnt39
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[36], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[37], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[38], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[39], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt36[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt37[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt38[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt39[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[36], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[37], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[38], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[39], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt36[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt37[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt38[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt39[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

//write channel 40
__kernel void __attribute__ ((task)) write_frontier40to43(
	__global int* restrict nxtFrnt40,
	__global int* restrict nxtFrnt41,
	__global int* restrict nxtFrnt42,
	__global int* restrict nxtFrnt43
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[40], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[41], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[42], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[43], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt40[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt41[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt42[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt43[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[40], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[41], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[42], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[43], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt40[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt41[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt42[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt43[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}


// write channel 44 to 47
__kernel void __attribute__ ((task)) write_frontier44to47(
		__global int* restrict nxtFrnt44,
		__global int* restrict nxtFrnt45,
		__global int* restrict nxtFrnt46,
		__global int* restrict nxtFrnt47
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[44], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[45], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[46], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[47], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt44[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt45[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt46[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt47[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[44], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[45], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[46], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[47], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt44[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt45[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt46[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt47[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

// write channel 48 to 51
__kernel void __attribute__ ((task)) write_frontier48to51(
		__global int* restrict nxtFrnt48,
		__global int* restrict nxtFrnt49,
		__global int* restrict nxtFrnt50,
		__global int* restrict nxtFrnt51
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[48], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[49], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[50], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[51], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt48[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt49[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt50[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt51[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[48], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[49], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[50], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[51], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt48[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt49[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt50[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt51[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}
}

// write channel 52 to 55
__kernel void __attribute__ ((task)) write_frontier52to55(
	__global int* restrict nxtFrnt52,
	__global int* restrict nxtFrnt53,
	__global int* restrict nxtFrnt54,
	__global int* restrict nxtFrnt55
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[52], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[53], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[54], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[55], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt52[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt53[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt54[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt55[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[52], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[53], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[54], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[55], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt52[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt53[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt54[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt55[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

// write channel 56
__kernel void __attribute__ ((task)) write_frontier56to59(
	__global int* restrict nxtFrnt56,
	__global int* restrict nxtFrnt57,
	__global int* restrict nxtFrnt58,
	__global int* restrict nxtFrnt59
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[56], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[57], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[58], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[59], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt56[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt57[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt58[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt59[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[56], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[57], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[58], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[59], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt56[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt57[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt58[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt59[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}

}

// write channel 60 to 63
__kernel void __attribute__ ((task)) write_frontier60to63(
	__global int* restrict nxtFrnt60,
	__global int* restrict nxtFrnt61,
	__global int* restrict nxtFrnt62,
	__global int* restrict nxtFrnt63
		)
{
	int buffer0[BUFFER_SIZE];
	int buffer1[BUFFER_SIZE];
	int buffer2[BUFFER_SIZE];
	int buffer3[BUFFER_SIZE];

	bool data_valid0 = false;
	bool flag_valid0 = false;

	bool data_valid1 = false;
	bool flag_valid1 = false;

	bool data_valid2 = false;
	bool flag_valid2 = false;

	bool data_valid3 = false;
	bool flag_valid3 = false;

	int global_idx0 = 0;
	int local_idx0  = 0;

	int global_idx1 = 0;
	int local_idx1  = 0;

	int global_idx2 = 0;
	int local_idx2  = 0;

	int global_idx3 = 0;
	int local_idx3  = 0;

	int flag0 = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	while(true){
		int vidx0 = read_channel_nb_altera(nxtFrnt_channel[60], &data_valid0);
		int vidx1 = read_channel_nb_altera(nxtFrnt_channel[61], &data_valid1);
		int vidx2 = read_channel_nb_altera(nxtFrnt_channel[62], &data_valid2);
		int vidx3 = read_channel_nb_altera(nxtFrnt_channel[63], &data_valid3);
		if(data_valid0){
			buffer0[local_idx0++] = vidx0;
			if(local_idx0 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt60[global_idx0 + i] = buffer0[i];
				}
				global_idx0 += BUFFER_SIZE;
				local_idx0 = 0;
			}
		}

		if(data_valid1){
			buffer1[local_idx1++] = vidx1;
			if(local_idx1 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt61[global_idx1 + i] = buffer1[i];
				}
				global_idx1 += BUFFER_SIZE;
				local_idx1 = 0;
			}
		}

		if(data_valid2){
			buffer2[local_idx2++] = vidx2;
			if(local_idx2 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt62[global_idx2 + i] = buffer2[i];
				}
				global_idx2 += BUFFER_SIZE;
				local_idx2 = 0;
			}
		}

		if(data_valid3){
			buffer3[local_idx3++] = vidx3;
			if(local_idx3 == BUFFER_SIZE){
				for(int i = 0; i < BUFFER_SIZE; i++){
					nxtFrnt63[global_idx3 + i] = buffer3[i];
				}
				global_idx3 += BUFFER_SIZE;
				local_idx3 = 0;
			}
		}

		int flag_tmp0 = read_channel_nb_altera(nxtFrnt_end_channel[60], &flag_valid0);
		int flag_tmp1 = read_channel_nb_altera(nxtFrnt_end_channel[61], &flag_valid1);
		int flag_tmp2 = read_channel_nb_altera(nxtFrnt_end_channel[62], &flag_valid2);
		int flag_tmp3 = read_channel_nb_altera(nxtFrnt_end_channel[63], &flag_valid3);
		if(flag_valid0) flag0 = flag_tmp0;
		if(flag_valid1) flag1 = flag_tmp1;
		if(flag_valid2) flag2 = flag_tmp2;
		if(flag_valid3) flag3 = flag_tmp3;
		if(flag0 == 1 && flag1 == 1 && flag2 == 1 && flag3 == 1 && 
		   !flag_valid0 && !flag_valid1 && !flag_valid2 && !flag_valid3 &&
		   !data_valid0 && !data_valid1 && !data_valid2 && !data_valid3)
		{
			break;
		}
	}

	//data left in the buffer
	if(local_idx0 > 0){
		for(int i = 0; i < local_idx0; i++){
			nxtFrnt60[global_idx0 + i] = buffer0[i];
		}
		global_idx0 += local_idx0;
		local_idx0 = 0;
	}

	if(local_idx1 > 0){
		for(int i = 0; i < local_idx1; i++){
			nxtFrnt61[global_idx1 + i] = buffer1[i];
		}
		global_idx1 += local_idx1;
		local_idx1 = 0;
	}

	if(local_idx2 > 0){
		for(int i = 0; i < local_idx2; i++){
			nxtFrnt62[global_idx2 + i] = buffer2[i];
		}
		global_idx2 += local_idx2;
		local_idx2 = 0;
	}

	if(local_idx3 > 0){
		for(int i = 0; i < local_idx3; i++){
			nxtFrnt63[global_idx3 + i] = buffer3[i];
		}
		global_idx3 += local_idx3;
		local_idx3 = 0;
	}
}


