/**************************************************************************************************

  superarhow's JPEG decoder

  by superarhow(superarhow@hotmail.com).  All rights reserved.

 **************************************************************************************************/

#include "jpegdec2.h"

 /*
  *  Decode next byte from huffman codec stream
  */
BYTE jpeg_dec_next_huff_code(p_jpeg_dec_rec p_rec, p_jpeg_huff_table p_table)
{
	WORD *p_min_code, *p_max_code;
	BYTE *p_codedata, *p_startindexes;
	BYTE *p_data;
	BYTE left_bits, n_left_bit_count, mask;
	WORD code;

	p_min_code = p_table->huff_min_codes;
	p_max_code = p_table->huff_max_codes;
	p_codedata = p_table->start_code_indexes;
	p_startindexes = p_table->start_code_indexes;

	p_data = p_rec->p_data;
	left_bits = p_rec->left_bits;
	n_left_bit_count = p_rec->n_left_bit_count;
	mask = 1 << (n_left_bit_count - 1);
	code = 0;
	for (;; ) {
		if (n_left_bit_count == 0) {
			/* fetch the next bits from the stream */
			n_left_bit_count = 8;
			mask = 0x80;
			left_bits = *p_data++;
			while (left_bits == 0xFF) {
				if (*p_data == 0) {
					p_data++;
					break;
				}
				else if (*p_data >= 0xD0 && *p_data <= 0xD7) {
					/* RSTn */
					n_left_bit_count = 0;
					jpeg_reset(p_rec);
					++p_data;
					break;
				}
				else {
					left_bits = *p_data++;
					break;
				}
			}
			if (n_left_bit_count == 0) continue; /* still not get the next byte :( */
		}
		code = code << 1;
		if (left_bits & mask) code++;
		n_left_bit_count--;
		left_bits &= (~mask);
		if (code <= *p_max_code && (*p_max_code >= 0x8000 || 0x8000 >= *p_min_code)) {
			/* Done! */
			break;
		}
		p_max_code++;
		p_min_code++;
		p_codedata++;
		p_startindexes++;
		mask >>= 1;
	}
	p_rec->n_left_bit_count = n_left_bit_count;
	p_rec->left_bits = left_bits;
	p_rec->p_data = p_data;
	code -= *p_min_code;
	if (code + *p_startindexes >= p_table->n_code_count) _asm int 3;
	return p_table->codes[code + *p_startindexes];
}

/*
 *  retreive n_bits from stream and return the retrieved bits
 */
WORD jpeg_get_next_bits(p_jpeg_dec_rec p_rec, BYTE n_bits)
{
	WORD result;
	BYTE *p_data;
	BYTE left_bits, n_left_bit_count;

	result = 0;
	p_data = p_rec->p_data;
	left_bits = p_rec->left_bits;
	n_left_bit_count = p_rec->n_left_bit_count;

	while (n_bits > 0) {

		if (n_left_bit_count == 0) {
			/* fetch the next bits from the stream */
			n_left_bit_count = 8;
			left_bits = *p_data++;
			while (left_bits == 0xFF) {
				if (*p_data == 0) {
					++p_data;
					break;
				}
				else if (*p_data >= 0xD0 && *p_data <= 0xD7) {
					/* RSTn */
					n_left_bit_count = 0;
					jpeg_reset(p_rec);
					++p_data;
					break;
				}
				else {
					left_bits = *p_data++;
					break;
				}
			}
			if (n_left_bit_count == 0) continue; /* still not get the next byte :( */
		}

		if (n_left_bit_count >= n_bits) {
			/* fetch n_bits bits */
			n_left_bit_count -= n_bits;
			result <<= n_bits;
			result |= (((1 << n_bits) - 1) & (left_bits >> n_left_bit_count));
			break;
		}
		else {
			/* fetch n_left_bit_count bits */
			result <<= n_left_bit_count;
			result |= (((1 << n_left_bit_count) - 1) & left_bits);
			n_bits -= n_left_bit_count;
			n_left_bit_count = 0;
		}
	}
	/* set the changed pointer and bit values */
	p_rec->left_bits = left_bits;
	p_rec->n_left_bit_count = n_left_bit_count;
	p_rec->p_data = p_data;
	return result;
}
