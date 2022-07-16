#ifndef __OUTPUT__
#define __OUTPUT__

#include "mux.h"
typedef struct {
  char* url;   // output rtmp url. If NULL nor rtmp output enabled
  char* path;  // path to recodring file. if null recodring is disabled
  AVStream* outVideoStream;  // output video stream that can be used by others
  AVCodecContext* videoEncCtx;
  AVPacket* outRtmpPacket;
  AVCodec* videoEncoder;
  // AVCodecParameters *audioCodecpar, *videoCodecpar;
  int64_t bitrate;
  AVRational timebase;
  int gop;
  AVFormatContext* rtmpOutCtx;
  AVStream *outVideoRtmp, *outAudioRtmp;

  int inWidth;
  int inHeight;
  int format;
  AVRational sampleAspectRatio;
} OutputCtxT;

/**
 * @brief take the inptu audio frame and push it through
 * the encoder and then either into rtmp out, recodring out
 * or into an AVStream that can be used by other muxers
 *
 * @param frame
 */
void outputWriteAudioFrame(AVFrame* frame);
void outputWriteVideoFrame(OutputCtxT* data, AVFrame* frame);

int startOutput(OutputCtxT* data);

#endif