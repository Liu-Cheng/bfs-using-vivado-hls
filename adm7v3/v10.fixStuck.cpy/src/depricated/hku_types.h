
#include <hls_half.h>
#include <ap_fixed.h>

#define FADD_LAT		7
#define TREE_SIZE		(FADD_LAT+1)
const unsigned FADD_LAT_SUB1 = FADD_LAT-1;

/*
 * ---- Utility class verbatim copied from x_hls_utils.h since it cannot be included in kernel for SDAccel ----
 */
template  < unsigned int _Num, unsigned int _I=_Num/2>
class BitWidthLcl
{
	public:
		static const unsigned int Value = 1 + BitWidthLcl<_Num,_I/2>::Value;
};

template <unsigned int _Num>
class BitWidthLcl<_Num, 0>
{
	public:
		static const unsigned int Value = 1;
};

const unsigned tree_size_log2 = BitWidthLcl<TREE_SIZE>::Value;
const unsigned tree_size_sub1_log2 = BitWidthLcl<TREE_SIZE-1>::Value;
const unsigned num_ranks = (tree_size_log2 != tree_size_sub1_log2 ? tree_size_log2 : tree_size_log2 + 1);


/*
 * ---- these are to be deprecated ----
 */
//const unsigned A_DIM1_PART = A_TILE_H/2;
//const unsigned A_DIM2_PART = A_VCTR_SIZE;
//const unsigned B_DIM1_PART = B_VCTR_SIZE;
//const unsigned B_DIM2_PART = B_VCTR_SIZE;


/*
 * ---- Custom datatypes useful globally ----
 */
typedef struct uint16_struct {
	unsigned d[16];
} uintv16_t;

typedef struct float16_struct {
	float d[16];
} float16_t;

typedef struct half32_struct {
	half d[32];
} half32_t;

typedef struct ushort32_struct {
	unsigned short d[32];
} ushort32_t;

typedef ap_fixed<41, 17> fixed_t;


/*
 * ---- Datatype preferences for current design ---- 
 */
//typedef float16_t bina_t;
//typedef float16_t binb_t;
//typedef float16_t bout_t;
//typedef float dina_t;
//typedef float dinb_t;
//typedef float dout_t;
//typedef dout_t mult_t;
//typedef dout_t accm_t;

//#define HALF_CAST
//typedef ushort32_t bina_t;
//typedef ushort32_t binb_t;
//typedef ushort32_t bout_t;
//typedef unsigned short dina_t;
//typedef unsigned short dinb_t;
//typedef unsigned short dout_t;
//typedef half mult_t;
//typedef half accm_t;
//typedef fixed_t accm_t;

//#define USE_HALF
typedef half32_t bina_t;
typedef half32_t binb_t;
typedef half32_t bout_t;
typedef half dina_t;
typedef half dinb_t;
typedef half dout_t;
typedef half mult_t;
typedef half accm_t;
//typedef fixed_t accm_t;


/*
 * ---- Prototype declaration for HLS testbench run ----
 */
#ifdef HLS_TESTBENCH
extern "C" {
void mmult(
		const bina_t *a,	// Read-Only Matrix A
		const binb_t *b,	// Read-Only Matrix B
		bout_t *c,		// Output Result
		unsigned a_row,	// Matrix A Row Size
		unsigned a_col, // Matrix A Col Size
		unsigned b_col, // Matrix B Col Size
		unsigned char a_cache, // caching A feasible only if localA fits whole row
		unsigned char b_trans); // specifies B is transposed in gmem, s.t. we can burst read columns
}
#endif


