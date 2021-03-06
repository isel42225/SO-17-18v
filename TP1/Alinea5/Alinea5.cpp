// Alinea5.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "MapFile.h"

int main(int argc , TCHAR * argv[])
{
	FILEMAP original;
	LPCTSTR filename = argv[1];
	BOOL b = MapFile(filename, GENERIC_READ | GENERIC_WRITE, OPEN_EXISTING, NULL, 0, &original);
	
	BYTE * baseAddr = original.baseAddress;
	BITMAPFILEHEADER * fileheader = (BITMAPFILEHEADER *)baseAddr;
	BITMAPINFOHEADER * headerinfo = (BITMAPINFOHEADER*)(baseAddr + sizeof(BITMAPFILEHEADER));

	RGBTRIPLE * colors = (RGBTRIPLE*)(baseAddr + fileheader->bfOffBits);

	int width = headerinfo->biWidth;
	int height = headerinfo->biHeight;
	int padding = (4 - (width * 3 % 4)) % 4;
	for (int i = 0; i < height ; ++i) {
		for (int j = 0; j < width; ++j) {
			RGBTRIPLE *	ncolors = colors + (height * width);
			
			 colors->rgbtBlue = ncolors->rgbtBlue;
			 colors->rgbtRed = ncolors->rgbtRed;
			 colors->rgbtGreen = ncolors->rgbtGreen;
			
			printf("R = %d , G = %d , B = %d\n",
				colors->rgbtRed, colors->rgbtGreen, colors->rgbtBlue);
			++colors;
		}
		
		printf("\n---------------------------------------\n");
		printf("end = %p\n", colors);

	}

	UnmapViewOfFile(original.baseAddress);
	CloseHandle(original.mapHandle);

    return 0;
}

