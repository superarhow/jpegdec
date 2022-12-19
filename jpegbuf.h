/**************************************************************************************************

  superarhow's JPEG decoder

  by superarhow(superarhow@hotmail.com).  All rights reserved.

 **************************************************************************************************/

#pragma once

#include "jpegdec2.h"

int jpeg_make_rgb_buffer(p_jpeg_dec_rec p_rec, BYTE** p_com_bufs, WORD* w_com_widths);
int jpeg_make_gray_buffer(p_jpeg_dec_rec p_rec, BYTE** p_com_bufs, WORD* w_com_widths);
