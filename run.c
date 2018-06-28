#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <stdio.h>

#include <leptonica/allheaders.h>
#include <tesseract/capi.h>

#define BLACK 20.0
#define YELLOW 70.0

#define min_f(a, b, c) (fminf(a, fminf(b, c)))
#define max_f(a, b, c) (fmaxf(a, fmaxf(b, c)))

//Conversion to the CIE-LAB color space to get the Luminance
void rgb_to_lab(float R, float G, float B,float *L, float *a, float *b)
{
	float X, Y, Z, fX, fY, fZ;

	X = 0.412453*R + 0.357580*G + 0.180423*B;
	Y = 0.212671*R + 0.715160*G + 0.072169*B;
	Z = 0.019334*R + 0.119193*G + 0.950227*B;

	X /= (255 * 0.950456);
	Y /=  255;
	Z /= (255 * 1.088754);

	if (Y > 0.008856)
	{
		fY = pow(Y, 1.0/3.0);
		*L = 116.0*fY - 16.0;
	}
	else
	{
		fY = 7.787*Y + 16.0/116.0;
		*L = 903.3*Y;
	}

	if (X > 0.008856)
		fX = pow(X, 1.0/3.0);
	else
		fX = 7.787*X + 16.0/116.0;

	if (Z > 0.008856)
		fZ = pow(Z, 1.0/3.0);
	else
		fZ = 7.787*Z + 16.0/116.0;

	*a = 500.0*(fX - fY);
	*b = 200.0*(fY - fZ);

	if (*L < BLACK) {
	*a *= exp((*L - BLACK) / (BLACK / 4));
	*b *= exp((*L - BLACK) / (BLACK / 4));
	*L = BLACK;
	}
	if (*b > YELLOW)
	*b = YELLOW;
}

//Display frames for debugging purposes
void display_frame(AVFrame *frame, int width, int height) {
	PIX *im;
	im = pixCreate(width, height, 32);
	
	int i, j, p, r, g, b;
	for(i=0 ; i<height; i++) {
		for(j=0 ; j<width ; j++) {
			p = j*3 + i*frame->linesize[0];
			r = frame->data[0][p];
			g = frame->data[0][p+1];
			b = frame->data[0][p+2];
			pixSetRGBPixel(im, j, i, r, g, b);
		}
	}
	pixDisplay(im, 0, 0);
	pixDestroy(&im);
}

int main(int argc, char * argv[]) {
	AVFormatContext *pFormatCtx = NULL;
	int             i, frame_count, videoStreamIdx;
	AVCodecContext  *pCodecCtx = NULL;
	AVCodec         *pCodec = NULL;
	AVFrame         *pFrame = NULL;
	AVFrame         *pFrameRGB = NULL;
	AVPacket        packet;
	int             frameFinished;
	int             numBytes;
	uint8_t         *buffer = NULL;
    
	AVDictionary *optionsDict = NULL;
	struct SwsContext *sws_ctx = NULL;
	
	if(argc < 2) {
		printf("Please provide a video file\n");
		return -1;
	}
	
	// Register all formats and codecs
	av_register_all();
	
	// Open video file
	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
		return -1; // Couldn't open file
		
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -1; // Couldn't find stream information
	
	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, argv[1], 0);
	
	// Find the first video stream
	videoStreamIdx = -1;
	for(i=0; i<pFormatCtx->nb_streams; i++) {
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			videoStreamIdx = i;
			break;
		}
	}
	
	if(videoStreamIdx==-1)
		return -1; // Didn't find a video stream
	
	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStreamIdx]->codec;
	
	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	
	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
		return -1; // Could not open codec
		
	// Allocate video frame
	pFrame = av_frame_alloc();
	
	// Allocate an AVFrame structure
	pFrameRGB = av_frame_alloc();
	if(pFrameRGB==NULL)
		return -1;
	
	// Determine required buffer size and allocate buffer
	numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
	
	sws_ctx = sws_getContext (
		pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, PIX_FMT_RGB24,
		SWS_BILINEAR, NULL, NULL, NULL
	);
	
	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	
	// Read frames and count the number of frames
	frame_count = 0;
	while(av_read_frame(pFormatCtx, &packet)>=0) {
		//Is this a packet from a video stream?
		if(packet.stream_index==videoStreamIdx) {
			// Decode the video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			
			//Did we get a video frame?
			if(frameFinished) {
				// Increment frame count
				frame_count++;
				
				// Convert the image from its native format to RGB
				sws_scale(
					sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0,
					pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize
				);
				
				//Now you have the frame and can do whatever you wish to
				
				//Debugging purposes
				/*if(frame_count > 5000) {
					display_frame(pFrameRGB, pCodecCtx->width, pCodecCtx->height);
					char c;
					scanf("%c", &c);
				}*/
				
			}
		}
		
		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);	
	}
	
	// Free the RBG image
	av_free(buffer);
	av_free(pFrameRGB);
	
	//Free the YUV frame
	av_free(pFrame);
	
	//Close the codec
	avcodec_close(pCodecCtx);
	
	//Close the video file
	avformat_close_input(&pFormatCtx);
	
	return 0;
}
