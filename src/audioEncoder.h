#ifndef __AUDIO__ENCODER__
#define __AUDIO__ENCODER__

#include <libavcodec/avcodec.h>

#include "config.h"
#include "mux.h"

typedef struct {
  int64_t bitrate;
  AVCodecContext* encCtx;
  AVCodec* encoder;
  AVPacket* packet;
  AVFrame* frame;
  AVRational timebase;
} AudioEncCtx;

int audioEncoderInit(AudioEncCtx* ctx);

int audioEncoderRun(AudioEncCtx* ctx);

#endif