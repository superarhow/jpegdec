#include <jpegwin32.h>
/*
 * Usage
 */
int main(int argc, const char** argv)
{
	HBITMAP hbmp;
	hbmp = jpeg_load_from_file(argv[1]);
	/*
	 * Use the hbmp
	 */
	DeleteObject(hbmp);
	return 0;
}
