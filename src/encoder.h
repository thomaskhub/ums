#ifndef __ENCODER__
#define __ENCODER__

#include "output.h"
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

class EncoderConfig {
public:
  int inWidth;
  int inHeight;
  int inPixFormat;
  int gop;

  int outWidth;
  int outHeight;
  int fps;
  int bitrate;

  AVMediaType type;
  AVRational timebase;
  int codecId;
  AVDictionary *opts;
};

class Encoder {
private:
  AVCodecContext *ctx;
  AVCodec *codec;
  EncoderConfig config;

  void initAudio();
  void initVideo();

public:
  Output *output;

  Encoder(EncoderConfig config) {
    this->config = config;
  }

  /**
   * @brief Initialize the encoder, must be called before pushing
   * data into it
   * @return int
   */
  int init();

  /**
   * @brief push a frame into the encoder
   *
   * @param frame
   * @return int
   */
  int push(AVFrame *frame);
  //   int push(AVFrame *frame, Output *output, int idx); THIS is how it should be

  /**
   * @brief pull a frame packet encoder
   * -EAGAIN if no packet was available yet
   *
   * @param frame
   * @return int
   */
  int pull(AVPacket **packet);

  int setCodecPar(AVCodecParameters **codecpar) {
    return avcodec_parameters_from_context((*codecpar), this->ctx);
  }
};

#endif