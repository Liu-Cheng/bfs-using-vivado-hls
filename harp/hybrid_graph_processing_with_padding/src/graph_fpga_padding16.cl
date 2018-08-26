//#include "config.h"
//type define
#define SW 0
#if SW == 1
	#define VERTEX_MAX  256*1024 //(128*1024)
	#define EDGE_MAX  16384 // (1024*1024)
#else
	#define VERTEX_MAX  (128*1024)
	#define EDGE_MAX  (64*1024)
#endif
#define PROP_TYPE float
#define PR
#define kDamp 0.85f
#define BRAM_BANK 16
#define LOG2_BRAM_BANK 4
#define PAD_TYPE int16
#define PAD_WITH 16
typedef struct EdgeInfo{
	int vertexIdx;
	PAD_TYPE ngbVidx;
	int outDeg;
} edge_info_t;

channel int activeVertexCh __attribute__((depth(2048)));
channel edge_info_t edgeInfoCh __attribute__((depth(2048)));
channel int edgeInfoChEof __attribute__((depth(4)));
channel int nextFrontierCh __attribute__((depth(2048)));
channel int nextFrontierChEof __attribute__((depth(4)));

 // BFS // SSSP // PR // CC
__attribute__((always_inline)) void compute(int srcVidx, int dstVidx, PROP_TYPE eProp, int outDeg,
	PROP_TYPE* tmpVPropBuffer, PROP_TYPE* vPropBuffer, int * count){

	#ifdef PR 
			
			if(outDeg) {
				int idx = dstVidx >> LOG2_BRAM_BANK;
				PROP_TYPE operand0 = vPropBuffer[srcVidx];
				PROP_TYPE operand1 = tmpVPropBuffer[idx];
				//printf(" %d \t",dstVidx/4);
			    tmpVPropBuffer[idx] = ( operand0 / outDeg) + operand1;
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
		__global const int* restrict activeVertices, 
		__global const int* restrict activeVertexNum
		)
{
	int vertexIdx;
	for(int i = 0; i < activeVertexNum[0]; i++){
		int vertexIdx = activeVertices[i];
		write_channel_altera(activeVertexCh, vertexIdx);
	}
}

__kernel void __attribute__((task)) readNgbInfo(
		__global const int* restrict rpaStart,
		__global const int* restrict rpaNum,
		__global const int* restrict outDeg,
		__global const PAD_TYPE * restrict cia,
		__global const PROP_TYPE* restrict edgeProp,
		__global const int* restrict activeVertexNum,
		const int vertexNum,
		const int edgeNum,
		__global const int* restrict itNum
		)
{	
	#if SW == 1
	    int * rpaStartBuffer = malloc(sizeof(int) * VERTEX_MAX);
	    int * rpaNumBuffer = malloc(sizeof(int) * VERTEX_MAX);
	    int * outDegBuffer = malloc(sizeof(int) * VERTEX_MAX);
	#else
		int rpaStartBuffer[VERTEX_MAX];
		int rpaNumBuffer[VERTEX_MAX];
		PROP_TYPE outDegBuffer[VERTEX_MAX];
	#endif
		for(int i = 0; i < vertexNum; i++){
			rpaStartBuffer[i] = rpaStart[i];
			rpaNumBuffer[i] = rpaNum[i];
			outDegBuffer[i] = outDeg[i];
		}
 // int cnt = 0;
	for(int i = 0; i < activeVertexNum[0]; i++){
		int vertexIdx = read_channel_altera(activeVertexCh);
		int start = rpaStartBuffer[vertexIdx];
		int num = rpaNumBuffer[vertexIdx];
		int vertexOutDeg = outDegBuffer[vertexIdx];
		for(int j = start >> LOG2_BRAM_BANK; j < (start + num) >> LOG2_BRAM_BANK; j++){
			PAD_TYPE ngbVidx = cia[j];
			edge_info_t edge_info;
			edge_info.vertexIdx = vertexIdx;
			edge_info.ngbVidx = ngbVidx;
			edge_info.outDeg = vertexOutDeg;
			write_channel_altera(edgeInfoCh, edge_info);
    //  cnt ++;
		}
	}
  //printf("total num is %d \n", cnt);
	write_channel_altera(edgeInfoChEof, 0xffff);
}

__kernel void __attribute__((task)) processEdge(
		__global PROP_TYPE* restrict vertexProp,
		__global float16 * restrict tmpVertexProp,
		__global PROP_TYPE* restrict semaphore,
		const int vertexNum,
		__global const int* restrict itNum
		)
{
	#if SW == 1
		PROP_TYPE * vPropBuffer 	= malloc (sizeof(PROP_TYPE) * VERTEX_MAX);
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
	#else 
		PROP_TYPE vPropBuffer[VERTEX_MAX];
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
	edge_info_t edge_info; 
	PROP_TYPE eProp = 0;
	int end_flag = 0;
 	int end_flag_tmp = 0;
	bool valid_data = 0;
	bool valid_flag = 0;
	int count[1] = {vertexNum};

	for(int i = 0; i < vertexNum; i++){
		vPropBuffer[i] = vertexProp[i];
	}
	for(int i = 0; i < vertexNum >> LOG2_BRAM_BANK; i++){
		tmpVPropBuffer0[i] = 0;
		tmpVPropBuffer1[i] = 0;
		tmpVPropBuffer2[i] = 0;
		tmpVPropBuffer3[i] = 0;
		tmpVPropBuffer4[i] = 0;
		tmpVPropBuffer5[i] = 0;
		tmpVPropBuffer6[i] = 0;
		tmpVPropBuffer7[i] = 0;
		tmpVPropBuffer8[i] = 0;
		tmpVPropBuffer9[i] = 0;
		tmpVPropBuffer10[i] = 0;
		tmpVPropBuffer11[i] = 0;
		tmpVPropBuffer12[i] = 0;
		tmpVPropBuffer13[i] = 0;
		tmpVPropBuffer14[i] = 0;
		tmpVPropBuffer15[i] = 0;
	}
  bool loop_condition = true;
while(loop_condition){	
		edge_info = read_channel_nb_altera(edgeInfoCh, &valid_data);
		if(valid_data){
      int srcVidx = edge_info.vertexIdx;
			PAD_TYPE dstVidx = edge_info.ngbVidx;
			int outDeg = edge_info.outDeg;

			if(dstVidx.s0 != -1)
				compute(srcVidx, dstVidx.s0, eProp, outDeg, tmpVPropBuffer0, vPropBuffer, count);
			if(dstVidx.s1 != -1)
				compute(srcVidx, dstVidx.s1, eProp, outDeg, tmpVPropBuffer1, vPropBuffer, count);
			if(dstVidx.s2 != -1)
				compute(srcVidx, dstVidx.s2, eProp, outDeg, tmpVPropBuffer2, vPropBuffer, count);
			if(dstVidx.s3 != -1)
				compute(srcVidx, dstVidx.s3, eProp, outDeg, tmpVPropBuffer3, vPropBuffer, count);

			if(dstVidx.s4 != -1)
				compute(srcVidx, dstVidx.s4, eProp, outDeg, tmpVPropBuffer4, vPropBuffer, count);
			if(dstVidx.s5 != -1)
				compute(srcVidx, dstVidx.s5, eProp, outDeg, tmpVPropBuffer5, vPropBuffer, count);
			if(dstVidx.s6 != -1)
				compute(srcVidx, dstVidx.s6, eProp, outDeg, tmpVPropBuffer6, vPropBuffer, count);
			if(dstVidx.s7 != -1)
				compute(srcVidx, dstVidx.s7, eProp, outDeg, tmpVPropBuffer7, vPropBuffer, count);	

			if(dstVidx.s8 != -1)
				compute(srcVidx, dstVidx.s8, eProp, outDeg, tmpVPropBuffer8, vPropBuffer, count);
			if(dstVidx.s9 != -1)
				compute(srcVidx, dstVidx.s9, eProp, outDeg, tmpVPropBuffer9, vPropBuffer, count);
			if(dstVidx.sa != -1)
				compute(srcVidx, dstVidx.sa, eProp, outDeg, tmpVPropBuffer10, vPropBuffer, count);
			if(dstVidx.sb != -1)
				compute(srcVidx, dstVidx.sb, eProp, outDeg, tmpVPropBuffer11, vPropBuffer, count);	

			if(dstVidx.sc != -1)
				compute(srcVidx, dstVidx.sc, eProp, outDeg, tmpVPropBuffer12, vPropBuffer, count);
			if(dstVidx.sd != -1)
				compute(srcVidx, dstVidx.sd, eProp, outDeg, tmpVPropBuffer13, vPropBuffer, count);
			if(dstVidx.se != -1)
				compute(srcVidx, dstVidx.se, eProp, outDeg, tmpVPropBuffer14, vPropBuffer, count);
			if(dstVidx.sf != -1)
				compute(srcVidx, dstVidx.sf, eProp, outDeg, tmpVPropBuffer15, vPropBuffer, count);	
		}
		end_flag_tmp = read_channel_nb_altera(edgeInfoChEof, &valid_flag);
		if(valid_flag) end_flag = end_flag_tmp;
		if(end_flag == 0xffff && !valid_data) loop_condition = false;
	}
	for(int i = 0; i < vertexNum >> LOG2_BRAM_BANK; i++){
		float16 ddr_data;
		ddr_data.s0 = tmpVPropBuffer0[i];
		ddr_data.s1 = tmpVPropBuffer1[i];
		ddr_data.s2 = tmpVPropBuffer2[i];
		ddr_data.s3 = tmpVPropBuffer3[i];
		ddr_data.s4 = tmpVPropBuffer4[i];
		ddr_data.s5 = tmpVPropBuffer5[i];
		ddr_data.s6 = tmpVPropBuffer6[i];
		ddr_data.s7 = tmpVPropBuffer7[i];
		ddr_data.s8 = tmpVPropBuffer8[i];
		ddr_data.s9 = tmpVPropBuffer9[i];
		ddr_data.sa = tmpVPropBuffer10[i];
		ddr_data.sb = tmpVPropBuffer11[i];
		ddr_data.sc = tmpVPropBuffer12[i];
		ddr_data.sd = tmpVPropBuffer13[i];
		ddr_data.se = tmpVPropBuffer14[i];
		ddr_data.sf = tmpVPropBuffer15[i];
		tmpVertexProp[i] = ddr_data;
	}
}
