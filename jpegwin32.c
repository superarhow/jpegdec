#include <jpegwin32.h>
#include <jpegdec2.h>
#include <windows.h>
#include <stdio.h>

HBITMAP jpeg_load_from_file(LPCTSTR lpszFileName)
{
	jpeg_dec_rec rec;
	FILE *fp = NULL;
	DWORD len;
	BYTE *buf;
	int ret;
	BITMAPINFO info;
	HBITMAP hbmp;

	fopen_s(&fp, lpszFileName, "rb");
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	buf = (BYTE *)malloc(len + 2);
	fread(buf, 1, len, fp);
	/* add 0xFF for error-free */
	buf[len] = 0xFF;
	buf[len + 1] = 0xD9;

	jpeg_init_decoder(&rec);
	rec.p_data = buf;
	rec.p_data_limit = buf + len;
	for (;; ) {
		ret = jpeg_decode_next_block(&rec);
		if (rec.p_data >= rec.p_data_limit) break;
	}
	jpeg_clear_up(&rec);

	free(buf);

	if (rec.p_bgr_buffer) {
		memset(&info, 0, sizeof(BITMAPINFO));
		info.bmiHeader.biBitCount = 24;
		info.bmiHeader.biCompression = BI_RGB;
		info.bmiHeader.biHeight = -(int)rec.n_height;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = rec.n_width;
		hbmp = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&buf, NULL, 0);
		memcpy(buf, rec.p_bgr_buffer, rec.n_bytes_per_line * rec.n_height);
		return hbmp;
	}

	return NULL;
}

