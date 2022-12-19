/**************************************************************************************************

  superarhow's JPEG decoder

  by superarhow(superarhow@hotmail.com).  All rights reserved.

 **************************************************************************************************/

#pragma once

#include <jpegdec2.h>

/* 2D-IDCT convertion */
void jpeg_idct( p_jpeg_quality_table p_table, SHORT* in );
void jpeg_idct_prepare_qualitytable( p_jpeg_quality_table p_table );
