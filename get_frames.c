#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <stdio.h>

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
