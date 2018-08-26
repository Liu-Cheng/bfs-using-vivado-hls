#define SW 1
#define PR
#define EOF_FLAG 0xffff
#define PROP_TYPE float

#define VERTEX_MAX  (128*1024)//262144//40960//40960//(128*1024)
#define EDGE_MAX    (2*1024*1024)//5610680////163840 // (1024*1024)

typedef struct EdgeInfo{
	int vertexIdx;
	int ngbVidx;
	//PROP_TYPE eProp;
	int outDeg;
} EDGE_INFO;

channel int activeVertexCh    __attribute__((depth(1024)));
channel EDGE_INFO edgeInfoCh  __attribute__((depth(1024)));
channel int edgeInfoChEof     __attribute__((depth(4)));

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
		__global int*  restrict blkCia,
		__global PROP_TYPE* restrict blkEdgeProp,
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
			/*int16 rpa_uint16 = blkRpa[i];
			rpaStartBuffer[(i << 4) + 0] = rpa_uint16.s0;
			rpaStartBuffer[(i << 4) + 1] = rpa_uint16.s1;
			rpaStartBuffer[(i << 4) + 2] = rpa_uint16.s2;
			rpaStartBuffer[(i << 4) + 3] = rpa_uint16.s3;
			rpaStartBuffer[(i << 4) + 4] = rpa_uint16.s4;
			rpaStartBuffer[(i << 4) + 5] = rpa_uint16.s5;
			rpaStartBuffer[(i << 4) + 6] = rpa_uint16.s6;
			rpaStartBuffer[(i << 4) + 7] = rpa_uint16.s7;
			rpaStartBuffer[(i << 4) + 8] = rpa_uint16.s8;
			rpaStartBuffer[(i << 4) + 9] = rpa_uint16.s9;
			rpaStartBuffer[(i << 4) + 10] = rpa_uint16.sa;
			rpaStartBuffer[(i << 4) + 11] = rpa_uint16.sb;
			rpaStartBuffer[(i << 4) + 12] = rpa_uint16.sc;
			rpaStartBuffer[(i << 4) + 13] = rpa_uint16.sd;
			rpaStartBuffer[(i << 4) + 14] = rpa_uint16.se;
			rpaStartBuffer[(i << 4) + 15] = rpa_uint16.sf;
			*/
			rpaStartBuffer[i] = blkRpa[i];
			rpaNumBuffer[i]   = blkRpa[i + 1] - blkRpa[i];
			rpao_old = blkRpa[i + 1];
		}

		/*for(int i = 0; i < blkVertexNum[0] - 1; i++){
			rpaNumBuffer[i]   = rpaStartBuffer[i + 1] - rpaStartBuffer[i];
		}
			rpaNumBuffer[blkVertexNum[0]-1] = 0;
*/
		for(int i = 0; i < blkVertexNum[0]; i++){
			outDegBuffer[i]   = outDeg[srcStart + i];
		}

		/*for(int i = 0; i < blkVertexNum[0] >> 0; i++){
			int16 deg_uint16 = outDeg[(srcStart) >> 4 + i];
			outDegBuffer[(i << 4) + 0] = deg_uint16.s0;
			outDegBuffer[(i << 4) + 1] = deg_uint16.s1;
			outDegBuffer[(i << 4) + 2] = deg_uint16.s2;
			outDegBuffer[(i << 4) + 3] = deg_uint16.s3;
			outDegBuffer[(i << 4) + 4] = deg_uint16.s4;
			outDegBuffer[(i << 4) + 5] = deg_uint16.s5;
			outDegBuffer[(i << 4) + 6] = deg_uint16.s6;
			outDegBuffer[(i << 4) + 7] = deg_uint16.s7;
			outDegBuffer[(i << 4) + 8] = deg_uint16.s8;
			outDegBuffer[(i << 4) + 9] = deg_uint16.s9;
			outDegBuffer[(i << 4) + 10] = deg_uint16.sa;
			outDegBuffer[(i << 4) + 11] = deg_uint16.sb;
			outDegBuffer[(i << 4) + 12] = deg_uint16.sc;
			outDegBuffer[(i << 4) + 13] = deg_uint16.sd;
			outDegBuffer[(i << 4) + 14] = deg_uint16.se;
			outDegBuffer[(i << 4) + 15] = deg_uint16.sf;
			
			//rpaNumBuffer[i]   = blkRpa[i + 1] - blkRpa[i];
			//rpao_old = blkRpa[i + 1];
		}*/



/*		for(int i = 0; i < blkEdgeNum[0]; i++){
			ciaBuffer[i]      = blkCia[i];
			edgePropBuffer[i] = blkEdgeProp[i];
		}
*/
	for(int i = 0; i < blkActiveVertexNum[0]; i++){
		int vertexIdx = read_channel_altera(activeVertexCh);
		int bufIdx = vertexIdx - srcStart;
		int start = rpaStartBuffer[bufIdx];
		int num = rpaNumBuffer[bufIdx];
		int deg = outDegBuffer[bufIdx];

		for(int j = 0; j < num; j++){
			int ngbVidx = blkCia[start + j];
			//int eProp = edgePropBuffer[start + j];
			EDGE_INFO edgeInfo;
			edgeInfo.vertexIdx = vertexIdx;
			edgeInfo.ngbVidx = ngbVidx;
			//edgeInfo.eProp = eProp;
			edgeInfo.outDeg = deg;
			write_channel_altera(edgeInfoCh, edgeInfo);
		}
	}
	write_channel_altera(edgeInfoChEof, EOF_FLAG);
}

__kernel void __attribute__((task)) processEdge(
		__global float* restrict vertexProp,
		__global float* restrict tmpVertexProp,
		__global const int* restrict eop,
		__global const int* restrict itNum,
		__global const int* restrict srcRange,
		__global const int* restrict sinkRange
		)
{	

	#ifdef SW
	PROP_TYPE * vertexPropBuffer    = (PROP_TYPE*)malloc(sizeof(PROP_TYPE) * VERTEX_MAX);
	PROP_TYPE * tmpVertexPropBuffer = (PROP_TYPE*)malloc(sizeof(PROP_TYPE) * VERTEX_MAX);
	#else
	PROP_TYPE vertexPropBuffer[VERTEX_MAX];
	PROP_TYPE tmpVertexPropBuffer[VERTEX_MAX];
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

		for(int i = 0; i < (srcNum >> 0); i++){
			/*
			float16 prop_uint16 = vertexProp[(srcStart) >> 4 + i];
			vertexPropBuffer[(i << 4) + 0] =  prop_uint16.s0;
			vertexPropBuffer[(i << 4) + 1] =  prop_uint16.s1;
			vertexPropBuffer[(i << 4) + 2] =  prop_uint16.s2;
			vertexPropBuffer[(i << 4) + 3] =  prop_uint16.s3;
			vertexPropBuffer[(i << 4) + 4] =  prop_uint16.s4;
			vertexPropBuffer[(i << 4) + 5] =  prop_uint16.s5;
			vertexPropBuffer[(i << 4) + 6] =  prop_uint16.s6;
			vertexPropBuffer[(i << 4) + 7] =  prop_uint16.s7;
			vertexPropBuffer[(i << 4) + 8] =  prop_uint16.s8;
			vertexPropBuffer[(i << 4) + 9] =  prop_uint16.s9;
			vertexPropBuffer[(i << 4) + 10] = prop_uint16.sa;
			vertexPropBuffer[(i << 4) + 11] = prop_uint16.sb;
			vertexPropBuffer[(i << 4) + 12] = prop_uint16.sc;
			vertexPropBuffer[(i << 4) + 13] = prop_uint16.sd;
			vertexPropBuffer[(i << 4) + 14] = prop_uint16.se;
			vertexPropBuffer[(i << 4) + 15] = prop_uint16.sf;
		*/
			vertexPropBuffer[i] = vertexProp[i + srcStart];
		}
		for(int i = 0; i < (dstNum >> 0); i++){
			/*float16 prop_uint16 = tmpVertexProp[(dstStart) >> 4 + i];
			tmpVertexPropBuffer[(i << 4) + 0] =  prop_uint16.s0;
			tmpVertexPropBuffer[(i << 4) + 1] =  prop_uint16.s1;
			tmpVertexPropBuffer[(i << 4) + 2] =  prop_uint16.s2;
			tmpVertexPropBuffer[(i << 4) + 3] =  prop_uint16.s3;
			tmpVertexPropBuffer[(i << 4) + 4] =  prop_uint16.s4;
			tmpVertexPropBuffer[(i << 4) + 5] =  prop_uint16.s5;
			tmpVertexPropBuffer[(i << 4) + 6] =  prop_uint16.s6;
			tmpVertexPropBuffer[(i << 4) + 7] =  prop_uint16.s7;
			tmpVertexPropBuffer[(i << 4) + 8] =  prop_uint16.s8;
			tmpVertexPropBuffer[(i << 4) + 9] =  prop_uint16.s9;
			tmpVertexPropBuffer[(i << 4) + 10] = prop_uint16.sa;
			tmpVertexPropBuffer[(i << 4) + 11] = prop_uint16.sb;
			tmpVertexPropBuffer[(i << 4) + 12] = prop_uint16.sc;
			tmpVertexPropBuffer[(i << 4) + 13] = prop_uint16.sd;
			tmpVertexPropBuffer[(i << 4) + 14] = prop_uint16.se;
			tmpVertexPropBuffer[(i << 4) + 15] = prop_uint16.sf;
			*/
			tmpVertexPropBuffer[i] = tmpVertexProp[i + dstStart];
		}

	while(true){
		EDGE_INFO edgeInfo = read_channel_nb_altera(edgeInfoCh, &validData);
		if(validData){
			int srcVidx      = edgeInfo.vertexIdx;
			int dstVidx      = edgeInfo.ngbVidx;
			//PROP_TYPE eProp  = edgeInfo.eProp;
			int outDeg       = edgeInfo.outDeg;

			int srcBufIdx = srcVidx - srcStart;
			int dstBufIdx = dstVidx - dstStart;
			PROP_TYPE srcProp = vertexPropBuffer[srcVidx - srcStart];
			PROP_TYPE dstProp = tmpVertexPropBuffer[dstVidx - dstStart];

            #ifdef PR 
				if(outDeg != 0)
					tmpVertexPropBuffer[dstBufIdx] += (vertexPropBuffer[srcBufIdx] / outDeg);
            #endif

            #ifdef BFS
			//tmpVertexPropBuffer[dstBufIdx] = (dstProp > srcProp + 1)? (srcProp + 1) : dstProp;
			if((vertexPropBuffer[srcVidx - srcStart] + 1) < tmpVertexPropBuffer[dstVidx - dstStart]){
				tmpVertexPropBuffer[dstVidx - dstStart] = vertexPropBuffer[srcVidx - srcStart] + 1;
			}
            #endif

            #ifdef SSSP
			tmpVertexPropBuffer[dstBufIdx] = (dstProp > srcProp + eProp)? (srcProp + eProp) : dstProp;
            #endif
		}
		int tmpEndFlag = read_channel_nb_altera(edgeInfoChEof, &validFlag);
		if(validFlag) endFlag = tmpEndFlag;	
		if(endFlag == EOF_FLAG && !validData && !validFlag) break;
	}


		//for(int i = 0; i < dstNum; i++){
			//tmpVertexProp[i + dstStart] = tmpVertexPropBuffer[i];
		//}

		for(int i = 0; i < (dstNum >> 0); i++){
			/*
			float16 prop_uint16;
			prop_uint16.s0 = tmpVertexPropBuffer[(i << 4) + 0] ; 
			prop_uint16.s1 = tmpVertexPropBuffer[(i << 4) + 1] ; 
			prop_uint16.s2 = tmpVertexPropBuffer[(i << 4) + 2] ; 
			prop_uint16.s3 = tmpVertexPropBuffer[(i << 4) + 3] ; 
			prop_uint16.s4 = tmpVertexPropBuffer[(i << 4) + 4] ; 
			prop_uint16.s5 = tmpVertexPropBuffer[(i << 4) + 5] ; 
			prop_uint16.s6 = tmpVertexPropBuffer[(i << 4) + 6] ; 
			prop_uint16.s7 = tmpVertexPropBuffer[(i << 4) + 7] ; 
			prop_uint16.s8 = tmpVertexPropBuffer[(i << 4) + 8] ; 
			prop_uint16.s9 = tmpVertexPropBuffer[(i << 4) + 9] ; 
			prop_uint16.sa = tmpVertexPropBuffer[(i << 4) + 10]; 
			prop_uint16.sb = tmpVertexPropBuffer[(i << 4) + 11]; 
			prop_uint16.sc = tmpVertexPropBuffer[(i << 4) + 12]; 
			prop_uint16.sd = tmpVertexPropBuffer[(i << 4) + 13]; 
			prop_uint16.se = tmpVertexPropBuffer[(i << 4) + 14]; 
			prop_uint16.sf = tmpVertexPropBuffer[(i << 4) + 15]; 
			tmpVertexProp[(dstStart) >> 4 + i] = prop_uint16;
			*/
			tmpVertexProp[i + dstStart] = tmpVertexPropBuffer[i];
		}

}


