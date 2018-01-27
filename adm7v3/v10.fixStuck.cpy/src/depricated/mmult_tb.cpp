//#include "matrix_multiply_float.h"
#include "hls/linear_algebra/utils/x_hls_matrix_utils.h"
#include "hls/linear_algebra/utils/x_hls_matrix_tb_utils.h"

#define HLS_TESTBENCH
#include "hku_defs.h"
#include "hku_types.h"

const unsigned A_ROWS = 255;
const unsigned A_COLS = 255;
const unsigned B_ROWS = A_COLS;
const unsigned B_COLS = 255;
const unsigned C_ROWS = A_ROWS;
const unsigned C_COLS = B_COLS;
const unsigned A_COLS_V = ((A_COLS - 1) / A_VCTR_SIZE + 1) * A_VCTR_SIZE;
const unsigned B_COLS_V = ((B_COLS - 1) / B_VCTR_SIZE + 1) * B_VCTR_SIZE;
const unsigned C_COLS_V = B_COLS_V;

typedef half MATRIX_T;

// Dummy top-level for testbench. 
// o A different synthesis top-level is selected for each solution by using set_directive_top
// o DESIGN_TOP is the function name specified as the project top (set_top) which each solution
//   points to a different implementation top-level function.
void DESIGN_TOP(const MATRIX_T A[A_ROWS][A_COLS], const MATRIX_T B[B_ROWS][B_COLS], MATRIX_T C[C_ROWS][C_COLS]) {
	// matrix_multiply_small(A, B, C);
	// matrix_multiply_default(A, B, C);
	// matrix_multiply_fast(A, B, C);
	// matrix_multiply_full(A, B, C);
}

void copy_C(bout_t c_gmem[C_ROWS * C_COLS_V / C_VCTR_SIZE], MATRIX_T C[C_ROWS][C_COLS]) {
	for (int i = 0; i < C_ROWS; i++)
		for (int j = 0; j < C_COLS; j++) {
#ifdef HALF_CAST
			half tmp;
			tmp = fp_struct<half>(c_gmem[(i *C_ROWS +j) /C_VCTR_SIZE].d[(i *C_ROWS +j) %C_VCTR_SIZE]).to_half();
			C[i][j] = tmp;
#else
			C[i][j] = c_gmem[(i * C_COLS_V + j) / C_VCTR_SIZE].d[(i * C_COLS_V + j) % C_VCTR_SIZE];
#endif
		}
}

void print_matrices(MATRIX_T A[A_ROWS][A_COLS], MATRIX_T B[B_ROWS][B_COLS], MATRIX_T C[C_ROWS][C_COLS]) {
	printf("A = \n");
	hls::print_matrix<A_ROWS, A_COLS, MATRIX_T, hls::NoTranspose>(A, "   ");

	printf("B = \n");
	hls::print_matrix<B_ROWS, B_COLS, MATRIX_T, hls::NoTranspose>(B, "   ");

	printf("C = \n");
	hls::print_matrix<C_ROWS, C_COLS, MATRIX_T, hls::NoTranspose>(C, "   ");
}

int main(void) {
	bool test1 = true;
	bool test2 = true;
	bool test3 = true;
	bool test4 = true;
	bool test5 = true;

	unsigned a_row = A_ROWS;
	unsigned a_col = A_COLS;
	unsigned b_col = B_COLS;

	MATRIX_T A[A_ROWS][A_COLS];
	MATRIX_T B[B_ROWS][B_COLS];
	MATRIX_T C[C_ROWS][C_COLS];
	MATRIX_T ATrans[A_ROWS][A_COLS];
	bina_t a_gmem[A_ROWS * A_COLS_V / A_VCTR_SIZE];
	binb_t b_gmem[B_ROWS * B_COLS_V / B_VCTR_SIZE];
	bout_t c_gmem[C_ROWS * C_COLS_V / C_VCTR_SIZE];
	bina_t aTrans_gmem[A_ROWS * A_COLS_V / A_VCTR_SIZE];

	unsigned ret = 0;

	// A content, simple count
	int cnt = 1;
	for (int i = 0; i < A_ROWS; i++)
		for (int j = 0; j < A_COLS; j++) {
			A[i][j] = cnt;
			ATrans[j][i] = cnt;
#ifdef HALF_CAST
			half tmp = cnt;
			a_gmem[(i *A_COLS_V +j) /A_VCTR_SIZE].d[(i *A_COLS_V +j) %A_VCTR_SIZE] = fp_struct<half>(tmp).to_int();
			aTrans_gmem[(j *A_COLS_V +i) /A_VCTR_SIZE].d[(j *A_COLS_V +i) %A_VCTR_SIZE] = fp_struct<half>(tmp).to_int();
#else
			a_gmem[(i * A_COLS_V + j) / A_VCTR_SIZE].d[(i * A_COLS_V + j) % A_VCTR_SIZE] = cnt;
			aTrans_gmem[(j * A_COLS_V + i) / A_VCTR_SIZE].d[(j * A_COLS_V + i) % A_VCTR_SIZE] = cnt;
#endif
			cnt++;
		}

	// Create identity for B
	for (int i = 0; i < B_ROWS; i++)
		for (int j = 0; j < B_COLS; j++)
			if (i == j) {
				B[i][j] = 1;
#ifdef HALF_CAST
				half tmp = 1;
				b_gmem[(i *B_COLS_V +j) /B_VCTR_SIZE].d[(i *B_COLS_V +j) %B_VCTR_SIZE] = fp_struct<half>(tmp).to_int();
#else
				b_gmem[(i * B_COLS_V + j) / B_VCTR_SIZE].d[(i * B_COLS_V + j) % B_VCTR_SIZE] = 1;
#endif
			} else {
				B[i][j] = 0;
#ifdef HALF_CAST
				half tmp = 0;
				b_gmem[(i *B_COLS_V +j) /B_VCTR_SIZE].d[(i *B_COLS_V +j) %B_VCTR_SIZE] = fp_struct<half>(tmp).to_int();
#else
				b_gmem[(i * B_COLS_V + j) / B_VCTR_SIZE].d[(i * B_COLS_V + j) % B_VCTR_SIZE] = 0;
#endif
			}

	if (test1) {
		mmult(a_gmem, b_gmem, c_gmem, a_row, a_col, b_col, false, false);
		copy_C(c_gmem, C);
		if (difference_ratio<A_ROWS, A_COLS>(A, C) > 30.0) {
			printf("---- Test mmult A x I == A failed; a_cache=false, b_trans=false ----\n");
			print_matrices(A, B, C);
			ret++;
		} else
			printf("---- Test mmult A x I == A passed; a_cache=false, b_trans=false ----\n");
	}

	if (test2) {
		mmult(b_gmem, a_gmem, c_gmem, a_row, a_col, b_col, false, false);
		copy_C(c_gmem, C);
		if (difference_ratio<A_ROWS, A_COLS>(A, C) > 30.0) {
			printf("---- Test mmult I x A == A failed; a_cache=false, b_trans=false ----\n");
			print_matrices(A, B, C);
			ret++;
		} else
			printf("---- Test mmult I x A == A passed; a_cache=false, b_trans=false ----\n");
	}

	if (test3) {
		mmult(a_gmem, b_gmem, c_gmem, a_row, a_col, b_col, true, false);
		copy_C(c_gmem, C);
		if (difference_ratio<A_ROWS, A_COLS>(A, C) > 30.0) {
			printf("---- Test mmult A x I == A failed; a_cache=true, b_trans=false ----\n");
			print_matrices(A, B, C);
			ret++;
		} else
			printf("---- Test mmult A x I == A passed; a_cache=true, b_trans=false ----\n");
	}

	if (test4) {
		mmult(b_gmem, a_gmem, c_gmem, a_row, a_col, b_col, true, false);
		copy_C(c_gmem, C);
		if (difference_ratio<A_ROWS, A_COLS>(A, C) > 30.0) {
			printf("---- Test mmult I x A == A failed; a_cache=true, b_trans=false ----\n");
			print_matrices(A, B, C);
			ret++;
		} else
			printf("---- Test mmult I x A == A passed; a_cache=true, b_trans=false ----\n");
	}

	if (test5) {
		mmult(b_gmem, aTrans_gmem, c_gmem, a_row, a_col, b_col, false, true);
		copy_C(c_gmem, C);
		if (difference_ratio<A_ROWS, A_COLS>(A, C) > 30.0) {
			printf("---- Test mmult I x ATrans == A failed; a_cache=false, b_trans=true ----\n");
			print_matrices(A, B, C);
			ret++;
		} else
			printf("---- Test mmult I x ATrans == A passed; a_cache=false, b_trans=true ----\n");
	}

	return (ret);
}

