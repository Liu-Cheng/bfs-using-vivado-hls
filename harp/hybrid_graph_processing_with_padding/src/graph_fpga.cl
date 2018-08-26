//#define SW 1
#define PR
#define EOF_FLAG 0xffff
#define PROP_TYPE float

#define VERTEX_MAX  (128*1024)//262144//40960//40960//(128*1024)
#define EDGE_MAX    (2*1024*1024)//5610680////163840 // (1024*1024)
#define BRAM_BANK 16
#define LOG2_BRAM_BANK 4
#define PAD_TYPE int16
#define PAD_WITH 16

typedef struct EdgeInfo{
	int vertexIdx;
	PAD_TYPE ngbVidx;
	int outDeg;
} EDGE_INFO;

channel int activeVertexCh    __attribute__((depth(1024)));
channel EDGE_INFO edgeInfoCh  __attribute__((depth(1024)));
channel int edgeInfoChEof     __attribute__((depth(4)));

__attribute__((always_inline)) void compute(PROP_TYPE srcProp, int dstVidx, PROP_TYPE eProp, int outDeg,
	PROP_TYPE* tmpVPropBuffer, PROP_TYPE* vPropBuffer, int  count){

	#ifdef PR 
			if(outDeg) {
				int dstBuffIdx = dstVidx >> LOG2_BRAM_BANK;
			    tmpVPropBuffer[dstBuffIdx] += ( srcProp / outDeg);
			}
	#endif
	#ifdef BFS
			tmpVPropBuffer[dstVidx] = (dstVprop > srcVprop + 1)? (srcVprop + 1) : dstVprop;
	#endif
	#ifdef SSSP
			tmpVPropBuffer[dstVidx] = (dstVprop > srcVprop + eProp)? (srcVprop + eProp) : dstVprop;
	#endif
}

__kernel void __attribute__((task)) readActiveVertices(
		__global const int* restrict blkActiveVertices, 
		__global const int* restrict blkActiveVertexNum
		)
{
	for(int i = 0; i < blkActiveVertexNum[0]; i++){
		int vertexIdx = blkActiveVertices[i];
		write_channel_altera(activeVertexCh, vertexIdx);
	}
}

__kernel void __attribute__((task)) readNgbInfo(
		__global int*  restrict blkRpa,
		__global PAD_TYPE*  restrict blkCia,
		__global PAD_TYPE* restrict blkEdgeProp,
		__global int*  restrict outDeg,
		__global int*  restrict blkActiveVertexNum,
		__global int*  restrict blkVertexNum,
		__global int*  restrict blkEdgeNum,
		__global int*  restrict srcRange,
		__global int*  restrict itNum
		)
{	
	#ifdef SW
	int* rpaStartBuffer = (int*)malloc(sizeof(int) * VERTEX_MAX);
	int* rpaNumBuffer   = (int*)malloc(sizeof(int) * VERTEX_MAX);
	int* outDegBuffer   = (int*)malloc(sizeof(int) * VERTEX_MAX);
	int* ciaBuffer      = (int*)malloc(sizeof(int) * EDGE_MAX);
	PROP_TYPE* edgePropBuffer = (int*)malloc(sizeof(int) * EDGE_MAX);
	#else
	int rpaStartBuffer[VERTEX_MAX];
	int rpaNumBuffer[VERTEX_MAX];
	int outDegBuffer[VERTEX_MAX];
	//int ciaBuffer[EDGE_MAX];
	//PROP_TYPE edgePropBuffer[EDGE_MAX];
	#endif
	int srcStart = srcRange[0];
    int rpao_old = blkRpa[0];

	for(int i = 0; i < blkVertexNum[0]; i++){
		outDegBuffer[i]   = outDeg[srcStart + i];
	}
	for(int i = 0; i < blkVertexNum[0]; i++){
		rpaStartBuffer[i] = blkRpa[i];
		rpaNumBuffer[i] = blkRpa[i + 1] - blkRpa[i];
		rpao_old = blkRpa[i + 1];
	}

	//printf("fpga ck 1\n");
	for(int i = 0; i < blkActiveVertexNum[0]; i++){
		int vertexIdx = read_channel_altera(activeVertexCh);
		int bufIdx = vertexIdx - srcStart;
		int start = rpaStartBuffer[bufIdx];
		int num = rpaNumBuffer[bufIdx];
		int deg = outDegBuffer[bufIdx];

		for(int j = start >> LOG2_BRAM_BANK; j < (start + num) >> LOG2_BRAM_BANK; j++){
			PAD_TYPE ngbVidx = blkCia[j];
			EDGE_INFO edgeInfo;
			edgeInfo.vertexIdx = vertexIdx;
			edgeInfo.ngbVidx = ngbVidx;
			//printf("bufIdx %d, num %d, vertexIdx %d, ngbVidx %d\n",bufIdx, num, vertexIdx,ngbVidx);
			edgeInfo.outDeg = deg;
			write_channel_altera(edgeInfoCh, edgeInfo);
		}
	}
	write_channel_altera(edgeInfoChEof, EOF_FLAG);
}

__kernel void __attribute__((task)) processEdge(
		__global float16* restrict vertexProp,
		__global float16* restrict tmpVertexProp,
		__global const int* restrict eop,
		__global const int* restrict itNum,
		__global const int* restrict srcRange,
		__global const int* restrict sinkRange
		)
{	
	#ifdef SW
		//PROP_TYPE * vPropBuffer 	= malloc (sizeof(PROP_TYPE) * VERTEX_MAX);
		PROP_TYPE * tmpVPropBuffer0 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer1 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer2 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer3 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer4 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer5 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer6 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer7 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer8 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer9 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer10 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer11 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer12 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer13 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer14 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * tmpVPropBuffer15 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer0 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer1 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer2 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer3 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer4 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer5 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer6 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer7 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer8 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer9 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer10 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer11 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer12 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer13 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer14 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
		PROP_TYPE * vPropBuffer15 = malloc (sizeof(PROP_TYPE) * VERTEX_MAX/BRAM_BANK);
	#else 
		//PROP_TYPE vPropBuffer[VERTEX_MAX];
		PROP_TYPE vPropBuffer0[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer1[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer2[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer3[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer4[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer5[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer6[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer7[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer8[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer9[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer10[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer11[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer12[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer13[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer14[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE vPropBuffer15[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer0[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer1[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer2[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer3[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer4[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer5[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer6[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer7[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer8[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer9[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer10[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer11[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer12[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer13[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer14[VERTEX_MAX >> LOG2_BRAM_BANK];
		PROP_TYPE tmpVPropBuffer15[VERTEX_MAX >> LOG2_BRAM_BANK];
	#endif

	int endFlag = 0;
	bool validData = 0;
	bool validFlag = 0;

	int srcStart = srcRange[0];
	int srcEnd = srcRange[1];
	int srcNum = srcEnd - srcStart;
	int dstStart = sinkRange[0];
	int dstEnd = sinkRange[1];
	int dstNum = dstEnd - dstStart;

		for(int i = 0; i < (srcNum >> 4); i++){
			float16 prop_uint16 = vertexProp[(srcStart >> 4) + i];
			vPropBuffer0[i]=  prop_uint16.s0;
			vPropBuffer1[i]=  prop_uint16.s1;
			vPropBuffer2[i]=  prop_uint16.s2;
			vPropBuffer3[i]=  prop_uint16.s3;
			vPropBuffer4[i]=  prop_uint16.s4;
			vPropBuffer5[i]=  prop_uint16.s5;
			vPropBuffer6[i]=  prop_uint16.s6;
			vPropBuffer7[i]=  prop_uint16.s7;
			vPropBuffer8[i]=  prop_uint16.s8;
			vPropBuffer9[i]=  prop_uint16.s9;
			vPropBuffer10[i] = prop_uint16.sa;
			vPropBuffer11[i] = prop_uint16.sb;
			vPropBuffer12[i] = prop_uint16.sc;
			vPropBuffer13[i] = prop_uint16.sd;
			vPropBuffer14[i] = prop_uint16.se;
			vPropBuffer15[i] = prop_uint16.sf;
		}
		for(int i = 0; i < (dstNum >> 4); i++){
			float16 prop_uint16 = tmpVertexProp[(dstStart >> 4) + i];
			tmpVPropBuffer0[i]= 	prop_uint16.s0;
			tmpVPropBuffer1[i]= 	prop_uint16.s1;
			tmpVPropBuffer2[i]= 	prop_uint16.s2;
			tmpVPropBuffer3[i]= 	prop_uint16.s3;
			tmpVPropBuffer4[i]= 	prop_uint16.s4;
			tmpVPropBuffer5[i]= 	prop_uint16.s5;
			tmpVPropBuffer6[i]= 	prop_uint16.s6;
			tmpVPropBuffer7[i]= 	prop_uint16.s7;
			tmpVPropBuffer8[i]= 	prop_uint16.s8;
			tmpVPropBuffer9[i]= 	prop_uint16.s9;
			tmpVPropBuffer10[i] = 	prop_uint16.sa;
			tmpVPropBuffer11[i] = 	prop_uint16.sb;
			tmpVPropBuffer12[i] = 	prop_uint16.sc;
			tmpVPropBuffer13[i] = 	prop_uint16.sd;
			tmpVPropBuffer14[i] = 	prop_uint16.se;
			tmpVPropBuffer15[i] = 	prop_uint16.sf;
		}

	while(true){
		EDGE_INFO edgeInfo = read_channel_nb_altera(edgeInfoCh, &validData);
		if(validData){
			int srcVidx      = edgeInfo.vertexIdx;
			PAD_TYPE dstVidx      = edgeInfo.ngbVidx;
			PROP_TYPE eProp  = 0x1;
			int outDeg       = edgeInfo.outDeg;
			int count = 0;
			int srcBufIdx = srcVidx - srcStart;
			PROP_TYPE srcProp;

			if((srcBufIdx & 0xf) == 0)
				srcProp = vPropBuffer0[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 1)
				srcProp = vPropBuffer1[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 2)
				srcProp = vPropBuffer2[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 3)
				srcProp = vPropBuffer3[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 4)
				srcProp = vPropBuffer4[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 5)
				srcProp = vPropBuffer5[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 6)
				srcProp = vPropBuffer6[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 7)
				srcProp = vPropBuffer7[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 8)
				srcProp = vPropBuffer8[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 9)
				srcProp = vPropBuffer9[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 10)
				srcProp = vPropBuffer10[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 11)
				srcProp = vPropBuffer11[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 12)
				srcProp = vPropBuffer12[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 13)
				srcProp = vPropBuffer13[srcBufIdx >> LOG2_BRAM_BANK];
			else if ((srcBufIdx & 0xf) == 14)
				srcProp = vPropBuffer14[srcBufIdx >> LOG2_BRAM_BANK];
			else 
				srcProp = vPropBuffer15[srcBufIdx >> LOG2_BRAM_BANK];


			//int dstBufIdx = dstVidx - dstStart;

			//PROP_TYPE srcProp = vertexPropBuffer[srcVidx - srcStart];
			//PROP_TYPE dstProp = tmpVertexPropBuffer[dstVidx - dstStart];

            #ifdef PR 
				if(outDeg != 0){
					if(dstVidx.s0 != -1)
						compute(srcProp, dstVidx.s0 - dstStart, eProp, outDeg, tmpVPropBuffer0, vPropBuffer0, count);
					if(dstVidx.s1 != -1)
						compute(srcProp, dstVidx.s1 - dstStart, eProp, outDeg, tmpVPropBuffer1, vPropBuffer1, count);
					if(dstVidx.s2 != -1)
						compute(srcProp, dstVidx.s2 - dstStart, eProp, outDeg, tmpVPropBuffer2, vPropBuffer2, count);
					if(dstVidx.s3 != -1)
						compute(srcProp, dstVidx.s3 - dstStart, eProp, outDeg, tmpVPropBuffer3, vPropBuffer3, count);
		
					if(dstVidx.s4 != -1)
						compute(srcProp, dstVidx.s4 - dstStart, eProp, outDeg, tmpVPropBuffer4, vPropBuffer4, count);
					if(dstVidx.s5 != -1)
						compute(srcProp, dstVidx.s5 - dstStart, eProp, outDeg, tmpVPropBuffer5, vPropBuffer5, count);
					if(dstVidx.s6 != -1)
						compute(srcProp, dstVidx.s6 - dstStart, eProp, outDeg, tmpVPropBuffer6, vPropBuffer6, count);
					if(dstVidx.s7 != -1)
						compute(srcProp, dstVidx.s7 - dstStart, eProp, outDeg, tmpVPropBuffer7, vPropBuffer7, count);
		
					if(dstVidx.s8 != -1)
						compute(srcProp, dstVidx.s8 - dstStart, eProp, outDeg, tmpVPropBuffer8, vPropBuffer8, count);
					if(dstVidx.s9 != -1)
						compute(srcProp, dstVidx.s9 - dstStart, eProp, outDeg, tmpVPropBuffer9, vPropBuffer9, count);
					if(dstVidx.sa != -1)
						compute(srcProp, dstVidx.sa - dstStart, eProp, outDeg, tmpVPropBuffer10, vPropBuffer10, count);
					if(dstVidx.sb != -1)
						compute(srcProp, dstVidx.sb - dstStart, eProp, outDeg, tmpVPropBuffer11, vPropBuffer11, count);
		
					if(dstVidx.sc != -1)
						compute(srcProp, dstVidx.sc - dstStart, eProp, outDeg, tmpVPropBuffer12, vPropBuffer12, count);
					if(dstVidx.sd != -1)
						compute(srcProp, dstVidx.sd - dstStart, eProp, outDeg, tmpVPropBuffer13, vPropBuffer13, count);
					if(dstVidx.se != -1)
						compute(srcProp, dstVidx.se - dstStart, eProp, outDeg, tmpVPropBuffer14, vPropBuffer14, count);
					if(dstVidx.sf != -1)
						compute(srcProp, dstVidx.sf - dstStart, eProp, outDeg, tmpVPropBuffer15, vPropBuffer15, count);
				}
            #endif
		}
		int tmpEndFlag = read_channel_nb_altera(edgeInfoChEof, &validFlag);
		if(validFlag) endFlag = tmpEndFlag;	
		if(endFlag == EOF_FLAG && !validData && !validFlag) break;
	}
		//for(int i = 0; i < dstNum; i++){
			//tmpVertexProp[i + dstStart] = tmpVertexPropBuffer[i];
		//}
		for(int i = 0; i < (dstNum >> 4); i++){
			float16 prop_uint16;
			prop_uint16.s0 = tmpVPropBuffer0[i]; 
			prop_uint16.s1 = tmpVPropBuffer1[i]; 
			prop_uint16.s2 = tmpVPropBuffer2[i]; 
			prop_uint16.s3 = tmpVPropBuffer3[i]; 
			prop_uint16.s4 = tmpVPropBuffer4[i]; 
			prop_uint16.s5 = tmpVPropBuffer5[i]; 
			prop_uint16.s6 = tmpVPropBuffer6[i]; 
			prop_uint16.s7 = tmpVPropBuffer7[i]; 
			prop_uint16.s8 = tmpVPropBuffer8[i]; 
			prop_uint16.s9 = tmpVPropBuffer9[i]; 
			prop_uint16.sa = tmpVPropBuffer10[i]; 
			prop_uint16.sb = tmpVPropBuffer11[i]; 
			prop_uint16.sc = tmpVPropBuffer12[i]; 
			prop_uint16.sd = tmpVPropBuffer13[i]; 
			prop_uint16.se = tmpVPropBuffer14[i]; 
			prop_uint16.sf = tmpVPropBuffer15[i]; 
			tmpVertexProp[(dstStart >> 4) + i] = prop_uint16;
		}
}


