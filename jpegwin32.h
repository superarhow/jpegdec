/**************************************************************************************************

  superarhow's JPEG decoder

  by superarhow(superarhow@hotmail.com).  All rights reserved.

 **************************************************************************************************/

#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" HBITMAP jpeg_load_from_file(LPCTSTR lpszFileName);
#else
HBITMAP jpeg_load_from_file(LPCTSTR lpszFileName);
#endif
