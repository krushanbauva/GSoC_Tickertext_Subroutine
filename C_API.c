#include <stdio.h>
#include <leptonica/allheaders.h>
#include <tesseract/capi.h>

int main(int argc, char *argv[]) {
	TessBaseAPI *handle;
	PIX *img;
	char *text;
	
	if((img = pixRead("text_russian.png")) == NULL) {
		printf("Error reading image\n");
		return -1;
	}
	
	handle = TessBaseAPICreate();
	if(TessBaseAPIInit3(handle, NULL, "rus") != 0) {
		printf("Error initialising tesseract\n");
		return -1;
	}
	
	TessBaseAPISetImage2(handle, img);
	if(TessBaseAPIRecognize(handle, NULL) != 0) {
		printf("Error in tesseract recognition\n");
		return -1;
	}
	
	if((text = TessBaseAPIGetUTF8Text(handle)) == NULL) {
		printf("Error getting text\n");
		return -1;
	}
	
	printf("%s", text);
	
	TessDeleteText(text);
	TessBaseAPIEnd(handle);
	TessBaseAPIDelete(handle);
	pixDestroy(&img);
	
	return 0;
	
}
