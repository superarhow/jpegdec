/**************************************************************************************************

  superarhow's JPEG decoder

  by superarhow(superarhow@hotmail.com).  All rights reserved.

 **************************************************************************************************/

#include <jpegdec2.h>
#include <jpeghuff.h>
#include <jpegbuf.h>
#include <jpegidct.h>


#include <stdio.h>

#define NextByte(p) (*(p->p_data++))
#define CurByte(p) (*(p->p_data))
#define PrevByte(p) (*(--p->p_data))
#define WORD_ALIGN(p) (p) = (((p) + 1) / 2) * 2
#define DWORD_ALIGN(p) (p) = (((p) + 3) / 4) * 4

void jpeg_reset(p_jpeg_dec_rec p_rec)
{
	p_rec->n_left_bit_count = 0;
	p_rec->n_reset_cnt = p_rec->n_reset_size;
	p_rec->i_dc = 0;
	p_rec->n_eob_count = 0;
}

BYTE jpeg_next_byte(p_jpeg_dec_rec p_rec)
{
	BYTE b;
	for (;; ) {
		b = *p_rec->p_data++;
		if (b == 0xFF) {
			b = *p_rec->p_data++;
			if (b == 0) return 0xFF;
			if (b >= 0xD0 && b <= 0xD7) {
				/* RSTn */
				jpeg_reset(p_rec);
			}
			else return b;
		}
		else return b;
	}
	return 0;
}

WORD jpeg_next_word(p_jpeg_dec_rec p_rec)
{
	WORD w = (p_rec->p_data[0] << 8) | (p_rec->p_data[1]);
	p_rec->p_data += 2;
	return w;
}

/* 初始化解?器 */
void jpeg_init_decoder(p_jpeg_dec_rec p_rec)
{
	p_rec->n_width = 0;
	p_rec->n_height = 0;
	p_rec->n_reset_size = 0;
	p_rec->left_bits = 0;
	p_rec->n_left_bit_count = 0;
	p_rec->n_horz_sample_coes_max = 0;
	p_rec->n_vert_sample_coes_max = 0;
	p_rec->n_horz_sample_coes_min = 0xff;
	p_rec->n_vert_sample_coes_min = 0xff;
	p_rec->p_bgr_buffer = NULL;
	p_rec->p_data = NULL;
	p_rec->i_dc = 0;
	p_rec->i_progressive = 0;
	p_rec->p_dus = NULL;
	p_rec->n_eob_count = 0;
}

/* clean up decoder */
void jpeg_clear_up(p_jpeg_dec_rec p_rec)
{
}

/*
 * jump to next block
 *  return non-zero for success
 */
int jpeg_skip_next_block(p_jpeg_dec_rec p_rec)
{
	BYTE b;
	WORD n;
	b = NextByte(p_rec);
	if (b == 0x01 /* TEM ?束?志*/ || (b >= 0xd0 && b <= 0xd9 /* RSTn OR SOI OR EOI*/)) return 1;
	n = jpeg_next_word(p_rec);
	p_rec->p_data += (n - 2);
	return 1;
}

/*
 * Decode the Start-of-frame block
 *  returns non-zero for success
 */
int jpeg_decode_SOF0(p_jpeg_dec_rec p_rec)
{
	BYTE b, index, n;

	/* skip tag */
	NextByte(p_rec);
	jpeg_next_word(p_rec);

	/* sample bits */
	p_rec->n_bits_per_sample = NextByte(p_rec);
	/* picture height (0 if specified by DNL tag) */
	p_rec->n_height = jpeg_next_word(p_rec);
	/* picture width */
	p_rec->n_width = jpeg_next_word(p_rec);
	/* bytes per line */
	p_rec->n_bytes_per_line = ((DWORD)p_rec->n_width) * 3;
	DWORD_ALIGN(p_rec->n_bytes_per_line);

	/* component count */
	p_rec->n_components = NextByte(p_rec);

	if (p_rec->n_components <= 0 || p_rec->n_components > 4) return 0; /* not supported component count */

	for (n = 0; n < p_rec->n_components; ++n) {
		/* Component ID */
		p_rec->i_component_ids[n] = jpeg_next_byte(p_rec);
		/*
		 * Note!!!!
		 * Our index here doesn't mean component ids, its only an index
		 * We also reference our huffman-table & quanlity-table by index not by component id.
		 */
		index = n;
		if (index >= p_rec->n_components) return 0; /* invalid component index */
		b = jpeg_next_byte(p_rec);
		/* sample coefficient of the component */
		p_rec->n_vert_sample_coes[index] = b & 0xf;
		p_rec->n_horz_sample_coes[index] = b >> 4;
		/* calculate the max value of each sampling coefficient */
		if (p_rec->n_vert_sample_coes[index] > p_rec->n_vert_sample_coes_max) p_rec->n_vert_sample_coes_max = p_rec->n_vert_sample_coes[index];
		if (p_rec->n_horz_sample_coes[index] > p_rec->n_horz_sample_coes_max) p_rec->n_horz_sample_coes_max = p_rec->n_horz_sample_coes[index];
		if (p_rec->n_vert_sample_coes[index] < p_rec->n_vert_sample_coes_min) p_rec->n_vert_sample_coes_min = p_rec->n_vert_sample_coes[index];
		if (p_rec->n_horz_sample_coes[index] < p_rec->n_horz_sample_coes_min) p_rec->n_horz_sample_coes_min = p_rec->n_horz_sample_coes[index];
		b = jpeg_next_byte(p_rec);
		if (b >= 4) return 0; /* invalid qua table id */
		p_rec->n_quality_table_indexes[index] = b;
	}

	return 1;
}

/*
 * Process the Define-Reset-Interval block
 *  returns non-zero for success
 */
int jpeg_decode_DRI(p_jpeg_dec_rec p_rec)
{
	NextByte(p_rec);
	jpeg_next_word(p_rec);
	p_rec->n_reset_size = jpeg_next_word(p_rec);
	p_rec->n_reset_cnt = jpeg_next_word(p_rec);
	return 1;
}

/*
 * Process the Define-Quantization-Table block
 *  returns non-zero for success
 */
int jpeg_decode_DQT(p_jpeg_dec_rec p_rec)
{
	WORD size;
	BYTE b, index, coe;
	WORD i;
	NextByte(p_rec);
	size = jpeg_next_word(p_rec) - 2;
	while (size > 0) {
		b = jpeg_next_byte(p_rec);
		index = b & 0xf;
		if (index >= 4) return 0;  /* invalid quantization table id */
		coe = b >> 4;
		if (coe != 0) coe = 1;
		/* precision of quantization table. 0:8bit non-zero:16bit */
		p_rec->quality_tables[index].precision = coe;
		if (coe == 0) {
			/* 8bit quantization table */
			for (i = 0; i < 64; i++) {
				p_rec->quality_tables[index].values[i] = jpeg_next_byte(p_rec);
			}
			p_rec->quality_tables[index].process_in_idct = 0;
			jpeg_idct_prepare_qualitytable(&p_rec->quality_tables[index]);
			size -= (64 + 1);
		}
		else {
			/* 16bit quantization table */
			for (i = 0; i < 64; i++) {
				p_rec->quality_tables[index].values[i] = jpeg_next_word(p_rec);
			}
			p_rec->quality_tables[index].process_in_idct = 0;
			jpeg_idct_prepare_qualitytable(&p_rec->quality_tables[index]);
			size -= (64 * 2 + 1);
		}
	}
	return 1;
}

/*
 * Generate huffman table for decoding
 */
void jpeg_make_huff_table(p_jpeg_huff_table p_table)
{
	WORD mask;
	WORD i, j, k;
	p_table->huff_min_codes[0] = 0;
	for (i = 0, mask = 0, j = 0, k = 0; i < p_table->n_code_count; ) {

		if (j >= p_table->n_counts[k]) {

			if (j == 0) {
				/* No k-bit huffman code exists. Set the min code to 0xffff and max code to 0 to indicate this */
				p_table->huff_min_codes[k] = 0xffff;
				p_table->huff_max_codes[k] = 0;
			}
			else {
				j = 0;
			}

			mask <<= 1;
			k++;
			if (k < 16) p_table->huff_min_codes[k] = mask;

		}
		else {

			p_table->huff_max_codes[k] = mask;
			mask++;
			j++;
			i++;

		}

	} /* end of for(...) */
}

/*
 * Process the Define-Huffman-Table block
 */
int jpeg_decode_DHT(p_jpeg_dec_rec p_rec)
{
	BYTE b, index, type;
	WORD size, total, i;
	NextByte(p_rec);
	size = jpeg_next_word(p_rec) - 2;
	while (size > 0) {
		b = jpeg_next_byte(p_rec);
		index = b & 0xf;
		if (index >= 4) return 0; /* invalid huffman table index */

		type = b >> 4;
		if (type > 1) return 0; /* invalid huffman table type */

		total = 0;
		for (i = 0; i < 16; i++) {
			b = jpeg_next_byte(p_rec);
			p_rec->huff_tables[type][index].n_counts[i] = b;
			p_rec->huff_tables[type][index].start_code_indexes[i] = (BYTE)total;
			total += b;
		}

		p_rec->huff_tables[type][index].n_code_count = total;
		memcpy(p_rec->huff_tables[type][index].codes, p_rec->p_data, total);
		jpeg_make_huff_table(&(p_rec->huff_tables[type][index]));

		p_rec->p_data += total;
		if (p_rec->p_data >= p_rec->p_data_limit) return 0; /* something went error */
		size -= (total + 16 + 1);
	}
	return 1;
}

/*
 * scan for next EOI. returns non-zero if found
 */
int jpeg_scan_for_eoi(p_jpeg_dec_rec p_rec)
{
	BYTE *p = p_rec->p_data;
	for (;; ) {
		if (p + 1 >= p_rec->p_data_limit) break;
		if (*p == 0xff) {
			if (p[1] == 0xD9) break;
			else {
				if (p[1] >= 0xD0 && p[1] <= 0xD7) {
					/* RSTn */
					p += 2;
				}
				else if (p[1] == 0) {
					return 0;
				}
				else {
					jpeg_decode_next_block(p_rec);
				}
			}
		}
		else {
			return 0;
		}
	}
	return 1;
}

/*
 * Decode image data(Non-progressive)
 *  return non-zero for success
 */
int jpeg_decode_scanlines(p_jpeg_dec_rec p_rec)
{
	static BYTE _un_zig_zag[64] = {
	  0, 1, 8, 16, 9, 2, 3,10,
	  17,24,32,25,18,11, 4, 5,
	  12,19,26,33,40,48,41,34,
	  27,20,13, 6, 7,14,21,28,
	  35,42,49,56,57,50,43,36,
	  29,22,15,23,30,37,44,51,
	  58,59,52,45,38,31,39,46,
	  53,60,61,54,47,55,62,63
	};
	/* Work array(Data Unit) */
	SHORT    du[64];
	/* Point to the current quantization table counts */
	DWORD    *pw_quality_values;
	/* All components' DCs */
	SHORT    dc[5] = { 0, 0, 0, 0, 0 };
	/* All components' decoding buffer, need to free */
	BYTE*    p_com_bufs[5] = { NULL, NULL, NULL, NULL, NULL };
	/* All components' address offset after scan. need to free */
	int*    p_com_buf_incs[5] = { NULL, NULL, NULL, NULL, NULL };
	/* Point to p_com_bufs's current address (need to recalculate when p_com_bufs is reallocated!!!) */
	BYTE*    p_com_ptrs[5] = { NULL, NULL, NULL, NULL, NULL };
	/* All components' widths (in pixels) */
	WORD    w_com_widths[5] = { 0, 0, 0, 0, 0 };
	/* All components' heights (in pixels). when w_scanlines is 0，the value is not fixed */
	WORD    w_com_heights[5] = { 0, 0, 0, 0, 0 };
	/* All components' count of horizontal and vertical */
	BYTE    n_com_counts[5] = { 0, 0, 0, 0, 0 };
	/* The increased bytes after each component scan finished */
	DWORD    dw_com_line_incs[5] = { 0, 0, 0, 0, 0 };
	/* The increased bytes after each scan line of each component */
	DWORD    dw_com_scanline_incs[5] = { 0, 0, 0, 0, 0 };
	/* Segments, scan lines(0 is specified during scanning), left scanlines */
	WORD    w_segments, w_scanlines, w_leftlines;
	/* When w_scanlines is 0，is value indicates the allocated scanlines */
	WORD    w_alloc_scanlines;
	/* The current AC huffman table & DC huffman table */
	p_jpeg_huff_table p_ac_huff_table, p_dc_huff_table;
	/* The current quantization table */
	p_jpeg_quality_table p_quality_table;
	/* RS(run-length-encoding) */
	BYTE    RS;
	/* BITS(left bits) */
	WORD    BITS;
	/* RRRR(high 4 bits of RS), SSSS(low 4 bits of RS) */
	BYTE    RRRR, SSSS;
	DWORD    dw_offset, dw_old_offset;
	BYTE    i, j, m, n, cnt;
	BYTE    *p;
	SHORT    *p_i;
	SHORT    i_ac;
	WORD    k;

	w_segments = (WORD)((p_rec->n_width + p_rec->n_horz_sample_coes_max * 8 - 1) / (p_rec->n_horz_sample_coes_max * 8));
	if (p_rec->n_height != 0) {
		w_scanlines = (WORD)((p_rec->n_height + p_rec->n_vert_sample_coes_max * 8 - 1) / (p_rec->n_vert_sample_coes_max * 8));
		w_alloc_scanlines = w_scanlines;
		w_leftlines = w_scanlines;
	}
	else w_scanlines = 0;

	/* Pre-allocate the temporary decoding area */
	for (i = 0; i < p_rec->n_components; i++) {
		n_com_counts[i] = p_rec->n_horz_sample_coes[i] * p_rec->n_vert_sample_coes[i];
		w_com_widths[i] = (WORD)(w_segments * p_rec->n_horz_sample_coes[i] * 8);
		dw_com_line_incs[i] = w_com_widths[i] - 8;
		dw_com_scanline_incs[i] = (DWORD)w_com_widths[i] * p_rec->n_vert_sample_coes[i] * 8 - w_com_widths[i];

		p_com_buf_incs[i] = (int*)malloc(w_com_widths[i] * sizeof(int*));
		if (w_scanlines == 0) {
			/* Not fixed? Alloc 1 line */
			w_alloc_scanlines = 1;
			w_com_heights[i] = (WORD)(w_alloc_scanlines * p_rec->n_vert_sample_coes[i] * 8);
			p_com_bufs[i] = (BYTE*)malloc((DWORD)w_com_widths[i] * (DWORD)w_com_heights[i]);
		}
		else {
			/* Fixed lines */
			w_com_heights[i] = (WORD)(w_scanlines * p_rec->n_vert_sample_coes[i] * 8);
			p_com_bufs[i] = (BYTE*)malloc((DWORD)w_com_widths[i] * (DWORD)w_com_heights[i]);
		}
		if (p_com_buf_incs[i] != NULL) {
			dw_old_offset = dw_offset = 0;
			for (j = 0, n = 0; n < p_rec->n_vert_sample_coes[i]; ++n) {
				for (m = 0; m < p_rec->n_vert_sample_coes[i]; ++m, ++j) {
					dw_offset = (DWORD)n * 8 * w_com_widths[i] + m * 8;
					if (j > 0) p_com_buf_incs[i][j - 1] = dw_offset - dw_old_offset;
					dw_old_offset = dw_offset;
				}
			}
			dw_offset = (DWORD)p_rec->n_horz_sample_coes[i] * 8;
			p_com_buf_incs[i][j - 1] = dw_offset - dw_old_offset;
		}
		p_com_ptrs[i] = p_com_bufs[i];
	}

	/* Check for invalid buffers */
	for (i = 0; i < p_rec->n_components; ++i)
	{
		/* some allocation failed */
		if (p_com_bufs[i] == NULL || p_com_buf_incs[i] == NULL)
		{
			// free the buffers successful allocated
			for (j = 0; j < p_rec->n_components; ++j)
			{
				if (p_com_bufs[j] != NULL) free(p_com_bufs[j]);
				if (p_com_buf_incs[j] != NULL) free(p_com_buf_incs[j]);
			}
			return 0;
		}
	}

	/* scan for lines */
	for (;; ) {
		/* scan for segments */
		for (k = 0; k < w_segments; ++k) {
			/* interlaced scan: component1, 2, 3... */
			for (i = 0; i < p_rec->n_components; ++i) {
				/* DC table of component i */
				p_dc_huff_table = &p_rec->huff_tables[0][p_rec->n_huff_tables_indexes[0][i]];
				/* AC table of component i */
				p_ac_huff_table = &p_rec->huff_tables[1][p_rec->n_huff_tables_indexes[1][i]];
				for (cnt = 0; cnt < n_com_counts[i]; ++cnt) {
					/* initialize working array */
					memset(du, 0, 64 * sizeof(SHORT));

					/* the current DC */
					p_rec->i_dc = dc[i];
					/* the current quantization table */
					p_quality_table = &p_rec->quality_tables[p_rec->n_quality_table_indexes[i]];
					/* the values of current quanity table */
					pw_quality_values = p_quality_table->values;

					/* decode RS(value of RLE code) */
					RS = jpeg_dec_next_huff_code(p_rec, p_dc_huff_table);
					/* for DC, the RS indicates bits directly */
					if (RS > 0) BITS = jpeg_get_next_bits(p_rec, RS);
					else BITS = 0;
					p_rec->i_dc += BITS;
					if (RS != 0) {
						if (BITS < (1 << (RS - 1))) p_rec->i_dc -= ((1 << RS) - 1);
					}
					dc[i] = p_rec->i_dc;
					if (!p_quality_table->process_in_idct) {
						/* dequantilize */
						du[0] = p_rec->i_dc * (*pw_quality_values);
						++pw_quality_values;
					}
					else {
						du[0] = p_rec->i_dc;
					}


					j = 1;
					/* decode zigzag buffer */
					p = &_un_zig_zag[j];

					while (j < 64) {
						RS = jpeg_dec_next_huff_code(p_rec, p_ac_huff_table);
						if (!RS) {
							/* EOB */
							break;
						}
						RRRR = RS >> 4;
						/* the 0 values to skip */
						j += RRRR;
						p += RRRR;
						pw_quality_values += RRRR;
						if (j >= 64) break;
						SSSS = RS & 0xf;
						if (!SSSS) {
							/* 0 coefficient */
							++j; ++p; ++pw_quality_values;
						}
						else {
							i_ac = jpeg_get_next_bits(p_rec, SSSS);
							if (i_ac < (1 << (SSSS - 1))) i_ac -= ((1 << SSSS) - 1);
							if (!p_quality_table->process_in_idct) {
								i_ac *= *pw_quality_values;
								++pw_quality_values;
							}
							else {

							}
							du[*p] = i_ac;
							++j; ++p;
						}
					} /* DU decoded (and dequantilized) */

					/* IDCT */
					jpeg_idct(p_quality_table, du);

					p = p_com_ptrs[i];
					p_i = du;
					for (n = 0; n < 8; ++n) {
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						p += dw_com_line_incs[i];
					}
					p_com_ptrs[i] += p_com_buf_incs[i][cnt];
				} /* cnt */
			} /* i */

			if (p_rec->n_reset_size != 0 && --p_rec->n_reset_cnt == 0) {
				/* time to reset check RSTn flag */
				if (p_rec->p_data + 1 >= p_rec->p_data_limit) goto _exit;
				if (CurByte(p_rec) == 0xff && p_rec->p_data[1] >= 0xD0 && p_rec->p_data[1] <= 0xD7) {
					/* RSTn flag apperas */
					p_rec->p_data += 2;
					jpeg_reset(p_rec);
					memset(dc, 0, 4 * sizeof(SHORT));
				}
				else {
					/* next??? */
					++p_rec->n_reset_cnt;
				}
			}
		} /* k */
		/* next line */
		for (i = 0; i < p_rec->n_components; ++i) p_com_ptrs[i] += dw_com_scanline_incs[i];
		/* dynamic scanlines */
		if (w_scanlines == 0) {
			/* check and reallocate all buffers of components, and adjust the pointers */
			p_rec->n_height += p_rec->n_horz_sample_coes_max * 8;
			while ((WORD)((p_rec->n_height + p_rec->n_vert_sample_coes_max * 8 - 1) / (p_rec->n_vert_sample_coes_max * 8)) > w_alloc_scanlines) {
				w_alloc_scanlines *= 2;
				for (i = 0; i < p_rec->n_components; ++i) {
					w_com_heights[i] = (WORD)(w_alloc_scanlines * p_rec->n_vert_sample_coes[i] * 8);
					p = p_com_bufs[i];
					/* reallocate */
					p_com_bufs[i] = (BYTE*)realloc(p_com_bufs[i], (DWORD)w_com_widths[i] * (DWORD)w_com_heights[i]);
					if (p_com_ptrs[i] == NULL) goto _exit;
					/* calculate offset again */
					p_com_ptrs[i] = p_com_bufs[i] + (p_com_ptrs[i] - p);
				}
			}
			/* TODO: parse the DNL */
		}
		else {
			/* known height */
			if (--w_leftlines == 0) break;
		}
	} /* scanlines */
	/* create bitmap */
	if (p_rec->n_components == 3) jpeg_make_rgb_buffer(p_rec, p_com_bufs, w_com_widths);
	else if (p_rec->n_components == 1) jpeg_make_gray_buffer(p_rec, p_com_bufs, w_com_widths);
	//else if ( p_rec->n_components == 4 )
_exit:
	for (i = 0; i < p_rec->n_components; ++i)
	{
		if (p_com_bufs[i] != NULL) free(p_com_bufs[i]);
		if (p_com_buf_incs[i] != NULL) free(p_com_buf_incs[i]);
	}
	return 1;
}

int jpeg_check_for_scan_end(p_jpeg_dec_rec p_rec)
{
	if (*p_rec->p_data == 0xFF) {
		if (p_rec->p_data + 1 >= p_rec->p_data_limit) return 1;
		if (p_rec->p_data[1] == 0) return 0;
		if (p_rec->p_data[1] >= 0xD0 && p_rec->p_data[1] <= 0xD7) return 0;
		return 1;
	}
	return 0;
}

#define CEIL_DIV(p, q)  (((p) + (q) - 1) / (q))

/*
* Decode frame (progressive)
*  Returns non-zero for success
*/
int jpeg_decode_scanlines_progressive(p_jpeg_dec_rec p_rec, BYTE *component_indexes, BYTE n_curr_scan_components)
{
	/* zigzag>DU index */
	static BYTE _un_zig_zag[64] = {
	 0, 1, 8, 16, 9, 2, 3,10,
	 17,24,32,25,18,11, 4, 5,
	 12,19,26,33,40,48,41,34,
	 27,20,13, 6, 7,14,21,28,
	 35,42,49,56,57,50,43,36,
	 29,22,15,23,30,37,44,51,
	 58,59,52,45,38,31,39,46,
	 53,60,61,54,47,55,62,63
	};
	/* Current work array(Data Unit) */
	SHORT    *p_curr_du;
	/* DC coefficient of all components */
	SHORT    dc[5] = { 0, 0, 0, 0, 0 };
	/* Each count of each component */
	BYTE    n_com_counts[5] = { 0, 0, 0, 0, 0 };
	BYTE    n_com_offsets[5] = { 0, 0, 0, 0, 0 };
	BYTE    n_com_total_count = 0;
	BYTE    n_curr_scan_component_index;
	DWORD    n_com_offset;
	WORD    w_segments, w_scanlines, w_leftlines, w_max_segments, w_max_scanlines;
	/* Current AC huffman table, DC huffman table */
	p_jpeg_huff_table p_ac_huff_table, p_dc_huff_table;
	/* RS(Run-length-encoding) */
	BYTE    RS;
	/* BITS(left bits) */
	WORD    BITS;
	/* RRRR(RS的高4位), SSSS(RS的低4位) */
	BYTE    RRRR, SSSS;
	BYTE    i, j, index, cnt;
	BYTE    i_first_scan;
	BYTE    *p;
	SHORT    i_ac, i_dc_diff;
	WORD    k, du_index, x_block, y_block, du_cnt, x_inc, y_inc, x_pos, y_pos, x, y;
	/* Progressive, du index convert table */
	WORD    *progressive_du_indexes = NULL;

	/* No dynamic height for progressive mode */
	if (p_rec->n_height == 0) return 0;

	w_max_segments = (WORD)CEIL_DIV(p_rec->n_width, p_rec->n_horz_sample_coes_max * 8);
	w_max_scanlines = (WORD)CEIL_DIV(p_rec->n_height, p_rec->n_vert_sample_coes_max * 8);

	if (n_curr_scan_components == 1) {
		for (i = 0; i < p_rec->n_components; ++i) {
			if (component_indexes[i] <= 3) {
				/* curr scan component index */
				j = component_indexes[i];
				n_curr_scan_component_index = j;
				w_segments = (WORD)CEIL_DIV(p_rec->n_width, p_rec->n_horz_sample_coes_max / p_rec->n_horz_sample_coes[j] * 8);
				w_scanlines = (WORD)CEIL_DIV(p_rec->n_height, p_rec->n_vert_sample_coes_max / p_rec->n_vert_sample_coes[j] * 8);
				x_inc = p_rec->n_horz_sample_coes[j];
				y_inc = p_rec->n_vert_sample_coes[j];
				break;
			}
		}
		progressive_du_indexes = (WORD*)malloc(w_segments * w_scanlines * sizeof(WORD));
		du_index = 0;
		du_cnt = 0;
		for (i = 0; i < p_rec->n_components; i++) {
			n_com_counts[i] = p_rec->n_horz_sample_coes[i] * p_rec->n_vert_sample_coes[i];
		}
		for (y_block = 0, y_pos = 0; y_block < w_max_scanlines; ++y_block, y_pos += y_inc) {
			for (x_block = 0, x_pos = 0; x_block < w_max_segments; ++x_block, x_pos += x_inc) {
				x = x_pos;
				y = y_pos;
				for (i = 0; i < p_rec->n_components; ++i) {
					for (j = 0; j < n_com_counts[i]; ++j, ++du_cnt) {
						du_index = y * w_segments + x;
						if (i == n_curr_scan_component_index) {
							if (y < w_scanlines && x < w_segments) {
								progressive_du_indexes[du_index] = du_cnt;
							}
							++x;
							if (x >= x_pos + x_inc) {
								x = x_pos;
								++y;
							}
						}
					}
				}
			}
		}
	}
	else {
		w_segments = w_max_segments;
		w_scanlines = w_max_scanlines;
	}
	w_leftlines = w_scanlines;

	/* pre-allocate all components' work buffer */
	p_rec->n_du_count = 0;
	n_com_total_count = 0;
	for (i = 0; i < p_rec->n_components; i++) {
		n_com_counts[i] = p_rec->n_horz_sample_coes[i] * p_rec->n_vert_sample_coes[i];
		n_com_offsets[i] = n_com_total_count;
		n_com_total_count += n_com_counts[i];
		p_rec->n_du_count += (DWORD)w_max_segments * p_rec->n_horz_sample_coes[i] * w_max_scanlines * p_rec->n_vert_sample_coes[i];
	}

	/* Allocate DUs */
	if (!p_rec->p_dus) {
		p_rec->p_dus = (SHORT*)malloc(p_rec->n_du_count * sizeof(SHORT) * 64);
		memset(p_rec->p_dus, 0, p_rec->n_du_count * sizeof(SHORT) * 64);
	}

	/* Check for invalid buffer */
	if (p_rec->p_dus == NULL) return 0;

	i_first_scan = p_rec->n_curr_scan_ah == 0;
	p_curr_du = p_rec->p_dus;
	/* TODO: reorder the scan process */
	/* Scan lines */
	du_index = 0;
	for (;; ) {
		/* scan segments */
		for (k = 0; k < w_segments; ++k) {
			/* interlaces scan：component1,,2,3... */
			for (i = 0; i < p_rec->n_components; ++i) {

				index = component_indexes[i];
				/* This component does not exist in this scan */
				if (index >= 4) {
					p_curr_du += 64 * n_com_counts[i];
					continue;
				}

				if (n_curr_scan_components == 1) {
					n_com_offset = progressive_du_indexes[du_index];
					p_curr_du = p_rec->p_dus + (DWORD)n_com_offset * 64;
				}

				/* DC table used by component i */
				p_dc_huff_table = &p_rec->huff_tables[0][p_rec->n_huff_tables_indexes[0][index]];
				/* AD table used by component i */
				p_ac_huff_table = &p_rec->huff_tables[1][p_rec->n_huff_tables_indexes[1][index]];

				cnt = n_com_counts[index];
				if (n_curr_scan_components == 1) cnt = 1;
				for (; cnt > 0; --cnt) {

					if (p_rec->n_curr_scan_ss == 0 /* DC scan line */) {

						/* the first DC scan */
						if (i_first_scan) {
							/* the current DC coefficient */
							p_rec->i_dc = dc[index];

							/* decode RS(RLE) */
							RS = jpeg_dec_next_huff_code(p_rec, p_dc_huff_table);
							/* for DC, RS is the bits */
							if (RS > 0) BITS = jpeg_get_next_bits(p_rec, RS);
							else BITS = 0;
							i_dc_diff = BITS;
							if (RS != 0) {
								if (BITS < (1 << (RS - 1))) i_dc_diff -= ((1 << RS) - 1);
							}
							p_rec->i_dc += i_dc_diff;
							p_curr_du[0] = p_rec->i_dc << p_rec->n_curr_scan_al;
							dc[index] = p_rec->i_dc;
						}
						else {
							/* the following DC scanlines */
							if (jpeg_get_next_bits(p_rec, 1) != 0) {
								p_curr_du[0] |= (1 << p_rec->n_curr_scan_al);
							}
						}

					}
					else {

						j = p_rec->n_curr_scan_ss;
						/* zigzag address */
						p = &_un_zig_zag[j];

						while (j <= p_rec->n_curr_scan_se) {

							/* process EOB */
							if (p_rec->n_eob_count > 0) {
								--p_rec->n_eob_count;
								if (!i_first_scan) {
									/* Process correct bits in the 0's with non zero history */
									while (j <= p_rec->n_curr_scan_se) {
										if (p_curr_du[*p] != 0) {
											if (jpeg_get_next_bits(p_rec, 1)) {
												/* has correction */
												if (p_curr_du[*p] >= 0) p_curr_du[*p] += (1 << p_rec->n_curr_scan_al);
												else p_curr_du[*p] -= (1 << p_rec->n_curr_scan_al);
											} /* else no correction */
										}
										++j; ++p;
									}
								}
								break;
							}

							RS = jpeg_dec_next_huff_code(p_rec, p_ac_huff_table);

							if (!RS) {
								if (!i_first_scan) {
									/* Process correct bits in the 0's with non zero history */
									while (j <= p_rec->n_curr_scan_se) {
										if (p_curr_du[*p] != 0) {
											if (jpeg_get_next_bits(p_rec, 1)) {
												/* has correction */
												if (p_curr_du[*p] >= 0) p_curr_du[*p] += (1 << p_rec->n_curr_scan_al);
												else p_curr_du[*p] -= (1 << p_rec->n_curr_scan_al);
											} /* else no correction */
										}
										++j; ++p;
									}
								}
								break;
							}

							RRRR = RS >> 4;
							SSSS = RS & 0xf;

							if ((!SSSS) && (RRRR < 15)) {

								/* EOBn */
								if (RRRR == 0) BITS = 1;
								else {
									BITS = jpeg_get_next_bits(p_rec, RRRR);
									BITS += (1 << RRRR);
								}
								p_rec->n_eob_count = BITS;
								continue;

							}

							if (SSSS) {
								i_ac = jpeg_get_next_bits(p_rec, SSSS);
							}

							/* Coefficients to skip */
							if (i_first_scan) {
								j += RRRR;
								p += RRRR;
							}
							else {
								if (SSSS != 1 && SSSS != 0) _asm int 3;

								/* Process correct bits in the 0's with non zero history */
								while (j <= p_rec->n_curr_scan_se) {
									if (p_curr_du[*p] != 0) {
										if (jpeg_get_next_bits(p_rec, 1)) {
											/* has correction */
											if (p_curr_du[*p] >= 0) p_curr_du[*p] += (1 << p_rec->n_curr_scan_al);
											else p_curr_du[*p] -= (1 << p_rec->n_curr_scan_al);
										} /* else no correction */
									}
									else {
										/* !!! In correction scan, ignore 0's when calculate the RLE */
										if (RRRR-- == 0) break;
									}
									++j; ++p;
								}
							}
							if (j > p_rec->n_curr_scan_se)
								break;

							if (!SSSS) {

								++j; ++p;

							}
							else {

								if (i_first_scan) {

									if (i_ac < (1 << (SSSS - 1))) i_ac -= ((1 << SSSS) - 1);

									p_curr_du[*p] = (i_ac << p_rec->n_curr_scan_al);

								}
								else {

									/* SSSS must equals 1 here */
									/* p_curr_du[*p] must equals 0 here */
									/* 1 bit fo sign */
									if (i_ac) {
										/* 正 */
										p_curr_du[*p] = 1 << p_rec->n_curr_scan_al;
									}
									else {
										/* ? */
										p_curr_du[*p] = -(1 << p_rec->n_curr_scan_al);
									}
								}
								++j; ++p;

							}
						} /* DU decode (and quantilize) done */
					}
					p_curr_du += 64;
					if (n_curr_scan_components == 1) {
						++du_index;
					}
				} /* cnt */
			} /* i */

			if (p_rec->n_reset_size != 0 && --p_rec->n_reset_cnt == 0) {
				/* Time to reset. check for RSTn */
				if (p_rec->p_data + 1 >= p_rec->p_data_limit) goto _exit;
				if (CurByte(p_rec) == 0xff && p_rec->p_data[1] >= 0xD0 && p_rec->p_data[1] <= 0xD7) {
					/* we have RSTn! */
					p_rec->p_data += 2;
					jpeg_reset(p_rec);
					memset(dc, 0, 4 * sizeof(SHORT));
				}
				else {
					/* next??? */
					++p_rec->n_reset_cnt;
				}
			}
		} /* k */
		if (--w_leftlines == 0)
		{
			if (p_rec->n_left_bit_count != 0) {
				p_rec->n_left_bit_count = 0;
			}
			p_rec->n_reset_cnt = p_rec->n_reset_size;
			p_rec->i_dc = 0;
			memset(dc, 0, 4 * sizeof(SHORT));
			break;
		}
	} /* scanlines */
_exit:
	if (progressive_du_indexes) free(progressive_du_indexes);
	return 1;
}

int jpeg_make_buf_progressive(p_jpeg_dec_rec p_rec)
{
	/* Current work array(Data Unit) */
	SHORT    *p_curr_du;
	/* Buffers of each components, have to free */
	BYTE*    p_com_bufs[5] = { NULL, NULL, NULL, NULL, NULL };
	/* Each address offsets of each scan lines, must free */
	int*    p_com_buf_incs[5] = { NULL, NULL, NULL, NULL, NULL };
	/* Address point to p_com_bufs's curren address(must recalculate when p_com_bufs reallocates!!!) */
	BYTE*    p_com_ptrs[5] = { NULL, NULL, NULL, NULL, NULL };
	/* Components' width(in pixels) */
	WORD    w_com_widths[5] = { 0, 0, 0, 0, 0 };
	/* Components' height (in pixels), when w_scanlines is zero, the value varies */
	WORD    w_com_heights[5] = { 0, 0, 0, 0, 0 };
	/* Each components' each direction's count */
	BYTE    n_com_counts[5] = { 0, 0, 0, 0, 0 };
	/* Byte offset after each line(pixel) of each component */
	DWORD    dw_com_line_incs[5] = { 0, 0, 0, 0, 0 };
	/* Byte offset after each scan line of each component  */
	DWORD    dw_com_scanline_incs[5] = { 0, 0, 0, 0, 0 };
	/* Segments, scanlines (0 for specified during scan), left lines */
	WORD    w_segments, w_scanlines, w_leftlines;
	/* Current quantization table */
	p_jpeg_quality_table p_quality_table;
	/* Current quantization values */
	DWORD    *pw_quality_values;
	DWORD    dw_offset, dw_old_offset;
	DWORD    n_du_count;
	BYTE    i, j, m, n, cnt;
	BYTE    *p;
	SHORT    *p_i;
	WORD    k;

	/* No dynamic height for progressive scan */
	if (p_rec->n_height == 0) return 0;

	w_segments = (WORD)((p_rec->n_width + p_rec->n_horz_sample_coes_max * 8 - 1) / (p_rec->n_horz_sample_coes_max * 8));
	w_scanlines = (WORD)((p_rec->n_height + p_rec->n_vert_sample_coes_max * 8 - 1) / (p_rec->n_vert_sample_coes_max * 8));
	w_leftlines = w_scanlines;

	/* Preallocate scan buffers for each component */
	for (i = 0; i < p_rec->n_components; i++) {
		n_com_counts[i] = p_rec->n_horz_sample_coes[i] * p_rec->n_vert_sample_coes[i];
		w_com_widths[i] = (WORD)(w_segments * p_rec->n_horz_sample_coes[i] * 8);
		dw_com_line_incs[i] = w_com_widths[i] - 8;
		dw_com_scanline_incs[i] = (DWORD)w_com_widths[i] * p_rec->n_vert_sample_coes[i] * 8 - w_com_widths[i];

		p_com_buf_incs[i] = (int*)malloc(w_com_widths[i] * sizeof(int*));
		w_com_heights[i] = (WORD)(w_scanlines * p_rec->n_vert_sample_coes[i] * 8);
		p_com_bufs[i] = (BYTE*)malloc((DWORD)w_com_widths[i] * (DWORD)w_com_heights[i]);

		if (p_com_buf_incs[i] != NULL) {
			dw_old_offset = dw_offset = 0;
			for (j = 0, n = 0; n < p_rec->n_vert_sample_coes[i]; ++n) {
				for (m = 0; m < p_rec->n_vert_sample_coes[i]; ++m, ++j) {
					dw_offset = (DWORD)n * 8 * w_com_widths[i] + m * 8;
					if (j > 0) p_com_buf_incs[i][j - 1] = dw_offset - dw_old_offset;
					dw_old_offset = dw_offset;
				}
			}
			dw_offset = (DWORD)p_rec->n_horz_sample_coes[i] * 8;
			p_com_buf_incs[i][j - 1] = dw_offset - dw_old_offset;
		}
		p_com_ptrs[i] = p_com_bufs[i];
	}

	p_curr_du = p_rec->p_dus;
	n_du_count = p_rec->n_du_count;
	for (w_leftlines = w_scanlines; w_leftlines > 0; --w_leftlines) {
		/* scan segments */
		for (k = 0; k < w_segments; ++k) {
			/* interlaced scan：component 1,2,3... */
			for (i = 0; i < p_rec->n_components; ++i) {
				for (cnt = 0; cnt < n_com_counts[i]; ++cnt) {
					/* the current quantilization table */
					p_quality_table = &p_rec->quality_tables[p_rec->n_quality_table_indexes[i]];
					if (!p_quality_table->process_in_idct) {
						/* values of current quantilization table */
						pw_quality_values = p_quality_table->values;
						/* dequantilization */
						for (j = 0; j < 64; ++j) {
							p_curr_du[j] *= (*pw_quality_values);
							++pw_quality_values;
						}
					}
					/* IDCT */
					jpeg_idct(p_quality_table, p_curr_du);
					p_curr_du += 64;
				}
			}
		}
	}
	p_curr_du = p_rec->p_dus;
	for (w_leftlines = w_scanlines; w_leftlines > 0; --w_leftlines) {
		/* segment scan */
		for (k = 0; k < w_segments; ++k) {
			/* interlaced scan：component 1,2,3... */
			for (i = 0; i < p_rec->n_components; ++i) {
				for (cnt = 0; cnt < n_com_counts[i]; ++cnt) {
					p = p_com_ptrs[i];
					p_i = p_curr_du;
					for (n = 0; n < 8; ++n) {
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						*p++ = (BYTE)*p_i++;
						p += dw_com_line_incs[i];
					}
					p_com_ptrs[i] += p_com_buf_incs[i][cnt];
					p_curr_du += 64;
				} /* cnt */
			} /* i */
		} /* k */
		/* next line */
		for (i = 0; i < p_rec->n_components; ++i) p_com_ptrs[i] += dw_com_scanline_incs[i];
	}

	free(p_rec->p_dus);
	p_rec->p_dus = NULL;

	/* create bitmap */
	if (p_rec->n_components == 3) jpeg_make_rgb_buffer(p_rec, p_com_bufs, w_com_widths);
	else if (p_rec->n_components == 1) jpeg_make_gray_buffer(p_rec, p_com_bufs, w_com_widths);
	//else if ( p_rec->n_components == 4 )
	for (i = 0; i < p_rec->n_components; ++i)
	{
		if (p_com_bufs[i] != NULL) free(p_com_bufs[i]);
		if (p_com_buf_incs[i] != NULL) free(p_com_buf_incs[i]);
	}
	return 1;
}


/*
 * Decode SOS(Start Of Scan) block，and the proceding frame
 *  returns non-zero for success
 */
int jpeg_decode_SOS(p_jpeg_dec_rec p_rec)
{
	BYTE i, j, b, index, id;
	BYTE component_indexes[4];
	BYTE n_curr_scan_components;

	NextByte(p_rec);
	jpeg_next_word(p_rec);

	n_curr_scan_components = jpeg_next_byte(p_rec);
	if (n_curr_scan_components <= 0 || n_curr_scan_components > 4) return 0;

	/* 0xFF means this component does not appear in the scan */
	component_indexes[0] = component_indexes[1] = component_indexes[2] = component_indexes[3] = 0xFF;

	for (i = 0; i < n_curr_scan_components; ++i) {
		/* component id */
		id = jpeg_next_byte(p_rec);
		for (j = 0; j < 4; ++j) {
			if (p_rec->i_component_ids[j] == id) {
				component_indexes[i] = j;
				break;
			}
		}
		if (j == 4) return 0;
		index = j;
		b = jpeg_next_byte(p_rec);
		p_rec->n_huff_tables_indexes[0][index] = b >> 4;
		p_rec->n_huff_tables_indexes[1][index] = b & 0xf;
	}

	p_rec->n_curr_scan_ss = NextByte(p_rec);
	p_rec->n_curr_scan_se = NextByte(p_rec);
	b = NextByte(p_rec);
	p_rec->n_curr_scan_ah = b >> 4;
	p_rec->n_curr_scan_al = b & 0xf;

	if (p_rec->i_progressive) jpeg_decode_scanlines_progressive(p_rec, component_indexes, n_curr_scan_components);
	else jpeg_decode_scanlines(p_rec);

	return 1;
}

/*
 *  Process the next block and return one the following codes:
 * JPEG_FAIL_UNEXPECTED_BYTE  unexpected byte (0ffh expected)
 * JPEG_FAIL_UNKNOWN_BLOCK_TYPE unknown block type 
 * JPEG_FAIL_DECODE_ERROR   known block type but failed to decode
 * JPEG_SUCCESS_NEXTBLOCK   success and wait for next block
 * JPEG_SUCCESS_LASTBLOCK   success and already the last block
 * JPEG_SUCCESS_IGNORED   unsupported block type and ignored
 * the previous 0xff is filtered before this function
 */
int jpeg_decode_next_block(p_jpeg_dec_rec p_rec)
{
	BYTE b, curb;
	b = NextByte(p_rec);
	if (b != 0xff)
	{
		return JPEG_FAIL_UNEXPECTED_BYTE;
	}
	curb = CurByte(p_rec);
	switch (curb) {
	case 0xD8:  /* SOI */
		jpeg_skip_next_block(p_rec);
		break;
	case 0xD9:  /* EOI */
		jpeg_skip_next_block(p_rec);
		if (p_rec->i_progressive) {
			jpeg_make_buf_progressive(p_rec);
		}
		return JPEG_SUCCESS_LASTBLOCK;
	case 0xC0:  /* SOF0 */
	case 0xC2:
		if (curb == 0xC2) p_rec->i_progressive = 1;
		if (!jpeg_decode_SOF0(p_rec)) return JPEG_FAIL_DECODE_ERROR;
		break;
	case 0xDD:  /* DRI */
		if (!jpeg_decode_DRI(p_rec)) return JPEG_FAIL_DECODE_ERROR;
		break;
	case 0xDB:  /* DQT */
		if (!jpeg_decode_DQT(p_rec)) return JPEG_FAIL_DECODE_ERROR;
		break;
	case 0xC4:  /* DHT */
		if (!jpeg_decode_DHT(p_rec)) return JPEG_FAIL_DECODE_ERROR;
		break;
	case 0xE0:  /* APP0-APPF */
	case 0xE1:
	case 0xE2:
	case 0xE3:
	case 0xE4:
	case 0xE5:
	case 0xE6:
	case 0xE7:
	case 0xE8:
	case 0xE9:
	case 0xEA:
	case 0xEB:
	case 0xEC:
	case 0xED:
	case 0xEE:
	case 0xEF:
	case 0xFE:  /* COM */
		jpeg_skip_next_block(p_rec);
		break;
	case 0xDA:  /* SOS */
		if (!jpeg_decode_SOS(p_rec)) return JPEG_FAIL_DECODE_ERROR;
		break;
	case 0xFF:  /* there are random 0xFFs and should be skipped */
		break;
	default:
		jpeg_skip_next_block(p_rec);
		return JPEG_SUCCESS_IGNORED;
	}
	return JPEG_SUCCESS_NEXTBLOCK;
}
