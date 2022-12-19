/**************************************************************************************************

  superarhow's JPEG decoder

  by superarhow(superarhow@hotmail.com).  All rights reserved.

 **************************************************************************************************/

#pragma once

#include <jpegdec2.h>

/* Decode next byte from huffman codec stream */
BYTE jpeg_dec_next_huff_code(p_jpeg_dec_rec p_rec, p_jpeg_huff_table p_table);

/* retreive n_bits from stream and return the retrieved bits */
WORD jpeg_get_next_bits(p_jpeg_dec_rec p_rec, BYTE n_bits);

