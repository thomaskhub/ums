#ifndef __AUDIO__ENCODER__
#define __AUDIO__ENCODER__

#include <libavcodec/avcodec.h>

#include "config.h"
#include "mux.h"

typedef struct {
  /*desired bitrate of the output audio stream*/
  int64_t bitrate;

  /*encoded packet which can be forwarded to muxers*/
  AVPacket *packet;

  /*frame that contains the data to be encoded*/
  AVFrame *frame;

  // internal use
  AVCodecContext *encCtx;
  AVCodec *encoder;
  AVRational timebase;
} AudioEncCtx;

/**
 * @brief initialize audio encoder
 *
 * @param ctx
 * @return int
 */
int audioEncoderInit(AudioEncCtx *ctx);

/**
 * @brief convert one audio frame
 *
 * @param ctx
 * @return int
 */
int audioEncoderRun(AudioEncCtx *ctx);

#endif