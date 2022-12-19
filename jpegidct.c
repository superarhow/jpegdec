#include <jpegidct.h>
#include <memory.h>

/*
 *  AA&N reverse-dct arithmetic implemention
 * {a, b, c, d, e, f, g} =  1/2 { cos(pi/4), cos(pi/16), cos(pi/8), cos(3pi/16), cos(5pi/16), cos(3pi/8), cos(7pi/16) }
 *  if we let: (out[8][8] is the temporary place to hold our first 1D-DCT data)
 * X[0] = ( in[0, 0], in[1, 0], in[2, 0] ... in[7, 0] )
 * X[1] = ( in[0, 1], in[1, 1], in[2, 1] ... in[7, 1] )
 * ...
 * X[7] = ( in[0, 7], in[1, 7], in[2, 7] ... in[7, 7] )
 * Y[0] = ( out[0, 0], out[1, 0], out[2, 0] ... out[7, 0] )
 * Y[1] = ( out[0, 1], out[1, 1], out[2, 1] ... out[7, 1] )
 * ...
 * Y[7] = ( out[0, 7], out[1, 7], out[2, 7] ... out[7, 7] )
 * we'll have:
 *
 *  / Y[0] /     / a  c  a  f / / X[0] /     / b  d  e  g / / X[1] /
 *  | Y[1] |  =  | a  f -a -c | | X[2] |  +  | d -g -b -e | | X[3] |
 *  | Y[2] |     | a -f -a  c | | X[4] |     | e -b  g  d | | X[5] |
 *  / Y[3] /     / a -c  a -f / / X[6] /     / g -e  d -b / / X[7] /
 *
 *  / Y[7] /     / a  c  a  f / / X[0] /     / b  d  e  g / / X[1] /
 *  | Y[6] |  =  | a  f -a -c | | X[2] |  -  | d -g -b -e | | X[3] |
 *  | Y[5] |  | a -f -a  c | | X[4] |     | e -b  g  d | | X[5] |
 *  / Y[4] /  / a -c  a -f / / X[6] /     / g -e  d -b / / X[7] /
 *
/* const * 8 */
#define FIX_1414 362
#define FIX_1847 473
#define FIX_1082 277
#define FIX_2613 669

#define FIX_MULDIV(p, q) ((INT32)(p) * (q) / 256)

void jpeg_idct(p_jpeg_quality_table p_table, SHORT* in)
{
	BYTE i;
	INT32 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	INT32 tmp10, tmp11, tmp12, tmp13;
	INT32 z5, z10, z11, z12, z13;
	INT32 work_maze[64];
	SHORT *p_row, *p_out;
	INT32 *p_col, *p_work;
	DWORD *p_dw_value;

	p_row = in;
	p_work = work_maze;
	p_dw_value = p_table->values;

#define ROW(n) ((INT32)p_row[n*8] * p_dw_value[n*8])
#define COL(n) p_col[n]
	/*
	 * first 1-D IDCT col->row
	 */
	for (i = 0; i < 8; ++i) {

		if (p_row[1 * 8] == 0 && p_row[2 * 8] == 0 && p_row[3 * 8] == 0 &&
			p_row[4 * 8] == 0 && p_row[5 * 8] == 0 && p_row[6 * 8] == 0 &&
			p_row[7 * 8] == 0) {
			p_work[0 * 8] = p_work[1 * 8] = p_work[2 * 8] = p_work[3 * 8]
				= p_work[4 * 8] = p_work[5 * 8] = p_work[6 * 8] = p_work[7 * 8] = ROW(0);
			/* next col */
			++p_work;
			++p_row;
			++p_dw_value;
			continue;
		}

		/* Even part */

		tmp0 = ROW(0);
		tmp1 = ROW(2);
		tmp2 = ROW(4);
		tmp3 = ROW(6);
		tmp4 = ROW(1);
		tmp5 = ROW(3);
		tmp6 = ROW(5);
		tmp7 = ROW(7);

		tmp10 = tmp0 + tmp2; /* phase 3 */
		tmp11 = tmp0 - tmp2;

		tmp13 = tmp1 + tmp3; /* phases 5-3 */
		tmp12 = FIX_MULDIV(tmp1 - tmp3, FIX_1414) - tmp13; /* 2*c4 */

		tmp0 = tmp10 + tmp13; /* phase 2 */
		tmp3 = tmp10 - tmp13;
		tmp1 = tmp11 + tmp12;
		tmp2 = tmp11 - tmp12;

		/* Odd part */

		z13 = tmp6 + tmp5;  /* phase 6 */
		z10 = tmp6 - tmp5;
		z11 = tmp4 + tmp7;
		z12 = tmp4 - tmp7;

		tmp7 = z11 + z13;  /* phase 5 */

		tmp11 = FIX_MULDIV(z11 - z13, FIX_1414); /* 2*c4 */

		z5 = FIX_MULDIV(z10 + z12, FIX_1847); /* 2*c2 */
		tmp10 = FIX_MULDIV(z12, FIX_1082) - z5; /* 2*(c2-c6) */
		tmp12 = FIX_MULDIV(z10, -FIX_2613) + z5; /* -2*(c2+c6) */

		tmp6 = tmp12 - tmp7; /* phase 2 */
		tmp5 = tmp11 - tmp6;
		tmp4 = tmp10 + tmp5;

		p_work[0 * 8] = tmp0 + tmp7;
		p_work[7 * 8] = tmp0 - tmp7;
		p_work[1 * 8] = tmp1 + tmp6;
		p_work[6 * 8] = tmp1 - tmp6;
		p_work[2 * 8] = tmp2 + tmp5;
		p_work[5 * 8] = tmp2 - tmp5;
		p_work[4 * 8] = tmp3 + tmp4;
		p_work[3 * 8] = tmp3 - tmp4;

		/* next col */
		++p_work;
		++p_row;
		++p_dw_value;
	}

	/*
	  * second 1-D IDCT row->col
	 */
	p_col = work_maze;
	p_out = in;
	for (i = 0; i < 8; ++i) {
		tmp0 = COL(0);
		tmp1 = COL(2);
		tmp2 = COL(4);
		tmp3 = COL(6);
		tmp4 = COL(1);
		tmp5 = COL(3);
		tmp6 = COL(5);
		tmp7 = COL(7);

		tmp10 = tmp0 + tmp2; /* phase 3 */
		tmp11 = tmp0 - tmp2;

		tmp13 = tmp1 + tmp3; /* phases 5-3 */
		tmp12 = FIX_MULDIV(tmp1 - tmp3, FIX_1414) - tmp13; /* 2*c4 */

		tmp0 = tmp10 + tmp13; /* phase 2 */
		tmp3 = tmp10 - tmp13;
		tmp1 = tmp11 + tmp12;
		tmp2 = tmp11 - tmp12;

		/* Odd part */

		z13 = tmp6 + tmp5;  /* phase 6 */
		z10 = tmp6 - tmp5;
		z11 = tmp4 + tmp7;
		z12 = tmp4 - tmp7;

		tmp7 = z11 + z13;  /* phase 5 */
		tmp11 = FIX_MULDIV(z11 - z13, FIX_1414); /* 2*c4 */

		z5 = FIX_MULDIV(z10 + z12, FIX_1847); /* 2*c2 */
		tmp10 = FIX_MULDIV(z12, FIX_1082) - z5; /* 2*(c2-c6) */
		tmp12 = FIX_MULDIV(z10, -FIX_2613) + z5; /* -2*(c2+c6) */

		tmp6 = tmp12 - tmp7; /* phase 2 */
		tmp5 = tmp11 - tmp6;
		tmp4 = tmp10 + tmp5;

		p_out[0] = (tmp0 + tmp7) / 2048;
		p_out[0] += 128;
		if (p_out[0] < 0) p_out[0] = 0; else if (p_out[0] > 255) p_out[0] = 255;
		p_out[7] = (tmp0 - tmp7) / 2048;
		p_out[7] += 128;
		if (p_out[7] < 0) p_out[7] = 0; else if (p_out[7] > 255) p_out[7] = 255;
		p_out[1] = (tmp1 + tmp6) / 2048;
		p_out[1] += 128;
		if (p_out[1] < 0) p_out[1] = 0; else if (p_out[1] > 255) p_out[1] = 255;
		p_out[6] = (tmp1 - tmp6) / 2048;
		p_out[6] += 128;
		if (p_out[6] < 0) p_out[6] = 0; else if (p_out[6] > 255) p_out[6] = 255;
		p_out[2] = (tmp2 + tmp5) / 2048;
		p_out[2] += 128;
		if (p_out[2] < 0) p_out[2] = 0; else if (p_out[2] > 255) p_out[2] = 255;
		p_out[5] = (tmp2 - tmp5) / 2048;
		p_out[5] += 128;
		if (p_out[5] < 0) p_out[5] = 0; else if (p_out[5] > 255) p_out[5] = 255;
		p_out[4] = (tmp3 + tmp4) / 2048;
		p_out[4] += 128;
		if (p_out[4] < 0) p_out[4] = 0; else if (p_out[4] > 255) p_out[4] = 255;
		p_out[3] = (tmp3 - tmp4) / 2048;
		p_out[3] += 128;
		if (p_out[3] < 0) p_out[3] = 0; else if (p_out[3] > 255) p_out[3] = 255;

		/* next col */
		p_out += 8;
		p_col += 8;
	}

}

/* when we use AA&N method, we need the function to be implemented, otherwise, left it empty */
/* we shift the factor left 5 bits for our integer operations */
void jpeg_idct_prepare_qualitytable(p_jpeg_quality_table p_table)
{
	static INT32 aan_factors[8] = { 256, 355, 334, 301, 256, 201, 139, 71 };
	static BYTE _zig_zag[64] = {
	 0, 1, 5, 6,14,15,27,28,
	 2, 4, 7,13,16,26,29,42,
	 3, 8,12,17,25,30,41,43,
	 9,11,18,24,31,40,44,53,
	 10,19,23,32,39,45,52,54,
	 20,22,33,38,46,51,55,60,
	 21,34,37,47,50,56,59,61,
	 35,36,48,49,57,58,62,63
	};
	BYTE i, j;
	DWORD values[64];
	for (j = 0; j < 8; ++j) {
		for (i = 0; i < 8; ++i) {
			values[i + j * 8] = p_table->values[_zig_zag[i + j * 8]] * aan_factors[i] * aan_factors[j] / 256;
		}
	}
	p_table->process_in_idct = 1;
	memcpy(p_table->values, values, sizeof(DWORD) * 64);
}
