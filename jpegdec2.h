/**************************************************************************************************

  superarhow's JPEG decoder

  by superarhow(superarhow@hotmail.com).  All rights reserved.

 **************************************************************************************************/

#pragma once

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning(disable: 4142)
#endif

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int INT32;
typedef short SHORT;

#ifdef _MSC_VER
#pragma warning( pop )
#endif

typedef struct _tag_jpeg_huff_table
{
	/* code counts of (1-16bits length) */
	BYTE n_counts[16];
	/* codes */
	BYTE codes[256];
	/* indexes to reference first code of each length */
	BYTE start_code_indexes[16];
	/* 1-16bits huffman min length(generated) */
	WORD huff_min_codes[16];
	/* 1-16bits huffman max length(generated) */
	WORD huff_max_codes[16];
	/* total codes count */
	WORD n_code_count;
	/* when a code is specified by N-bits huffman codec, the actual code is retrieved by:
	code = codes[Bits - huff_min_codes[N] + start_code_indexes[N]] */
} jpeg_huff_table, *p_jpeg_huff_table;

typedef struct _tag_jpeg_quality_table
{
	/* quality table size. 0: 8bit, Not 0: 12bits (or 12? in some documents?) */
	BYTE precision;
	/* the flag indicating the idct function processes dequantity  */
	BYTE process_in_idct;
	/* quantity table, 64 elements */
	DWORD values[64];
} jpeg_quality_table, *p_jpeg_quality_table;

typedef struct _tag_jpeg_dec_rec
{
	/* pointer to streamed data */
	BYTE    *p_data;
	/* the last+1 byte position of data */
	BYTE    *p_data_limit;
	/* left bits not retrieved. lsb is valid */
	BYTE    left_bits;
	/* left how many bits */
	BYTE    n_left_bit_count;
	/* scan-start of the current scan. for progressively mode only */
	BYTE    n_curr_scan_ss;
	/* scan-end of the current scan. for progressively mode only */
	BYTE    n_curr_scan_se;
	/* the AH value of the current scan (last progressive bits) */
	BYTE    n_curr_scan_ah;
	/* the AL value of the current scan (the current progressive bits) */
	BYTE    n_curr_scan_al;
	/* left EOB count, for progressive mode only */
	WORD    n_eob_count;
	/* bytes of each line in target buffer */
	DWORD    n_bytes_per_line;
	/* target buffer created by jpeg_make_xxx_buffer */
	BYTE    *p_bgr_buffer;
	/* huffman table. [0][0-3]: DC table, [1][0-3]: AC table */
	jpeg_huff_table  huff_tables[2][4];
	/* quantity table */
	jpeg_quality_table quality_tables[4];
	/* total DU count */
	DWORD    n_du_count;
	/* all working buffers, must release (progressive mode) */
	SHORT    *p_dus;
	/* sample bit count (8,12 or 16, but we only support 8) */
	BYTE    n_bits_per_sample;
	/* (current frame's)components count(1 byte), 1 for grayscale, YCbCr/YIQ color mode is 3, CMYK color mode is 4
	component id (1 = Y, 2 = Cb, 3 = Cr, 4 = I, 5 = Q) must -1 for indexes */
	BYTE    n_components;
	/* the ID of the n-th component */
	BYTE    i_component_ids[4];
	/* vertical sampling coefficient */
	BYTE    n_vert_sample_coes[4];
	/* horizontal sampling coefficient */
	BYTE    n_horz_sample_coes[4];
	/* max value of vertical sampling coefficient */
	BYTE    n_vert_sample_coes_max;
	/* max value of horizontal coefficient */
	BYTE    n_horz_sample_coes_max;
	/* max value of vertical coefficient */
	BYTE    n_vert_sample_coes_min;
	/* min value of horizontal coefficient */
	BYTE    n_horz_sample_coes_min;
	/* current quantity table of each components (0-3) */
	BYTE    n_quality_table_indexes[4];
	/* current huffman table of each components [0-1][0-3] 0 DC 1 AC */
	BYTE    n_huff_tables_indexes[2][4];
	/* width of picture */
	WORD    n_width;
	/* height of pictureÅiwhen is 0, it is specified by DNL tagÅj */
	WORD    n_height;
	/* reset size specified by DNL tag. 0 for no information. it is used for error correction */
	WORD    n_reset_size;
	/* the current reset MCU's count. it is initialized using the value of n_reset_size */
	WORD    n_reset_cnt;
	/* the current DC value */
	SHORT    i_dc;
	/* is progressive? */
	BYTE    i_progressive;
} jpeg_dec_rec, *p_jpeg_dec_rec;

/*
 * return values of jpeg_decode_next_block
 */
#define JPEG_FAIL_UNEXPECTED_BYTE  -1  /* unexpected byte (0ffh expected) */
#define JPEG_FAIL_UNKNOWN_BLOCK_TYPE -2  /* unknown block type */
#define JPEG_FAIL_DECODE_ERROR   -3  /* known block type but failed to decode */
#define JPEG_SUCCESS_NEXTBLOCK   0  /* sucess, wait for next block */
#define JPEG_SUCCESS_LASTBLOCK   1  /* success, no next block */
#define JPEG_SUCCESS_IGNORED   2  /* unsupported block type and ignored */

 /* decoder initialize */
void jpeg_init_decoder(p_jpeg_dec_rec p_rec);
/*
 * process next byte and return the value of the
 * JPEG_FAIL_UNEXPECTED_BYTE  unexpected byte (0ffh expected)
 * JPEG_FAIL_UNKNOWN_BLOCK_TYPE unknown block type
 * JPEG_FAIL_DECODE_ERROR   known block type but failed to decode
 * JPEG_SUCCESS_NEXTBLOCK   sucess, wait for next block
 * JPEG_SUCCESS_LASTBLOCK   success, no next block
 * JPEG_SUCCESS_IGNORED   unsupported block type and ignored
 * Note: the prefixed 0xff will be trimmed before
 */
int jpeg_decode_next_block(p_jpeg_dec_rec p_rec);
/* clean up decoder */
void jpeg_clear_up(p_jpeg_dec_rec p_rec);
/* RSTÅCused internallly */
void jpeg_reset(p_jpeg_dec_rec p_rec);
