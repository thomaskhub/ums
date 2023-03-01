#ifndef __ENCODER__
#define __ENCODER__

#include "config.h"
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
  // Video
  int inWidth;
  int inHeight;
  int inPixFormat;
  int gop;

  int outWidth;
  int outHeight;
  int fps;
  int bitrate;

  // Audio
  int sampleFormat;
  int sampleRate;
  int channelLayout;

  AVMediaType type;
  AVRational timebase;
  int codecId;
  AVDictionary *opts;
};

struct EncoderOutputs {
  Output *output;
  int index;
};

class Encoder {
private:
  AVCodecContext *ctx;
  AVCodec *codec;
  EncoderConfig config;
  AVPacket *packet;

  EncoderOutputs outputs[6];
  int outputCnt;

  void initAudio();
  void initVideo();

public:
  Encoder(EncoderConfig config) {
    this->config = config;
    this->outputCnt = 0;
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

  AVCodecContext *getCodecContext() {
    return this->ctx;
  }

  int addOutput(Output *output, int index) {
    if (this->outputCnt >= 6) {
      return AVERROR(ENOMEM);
    }

    this->outputs[this->outputCnt].index = index;
    this->outputs[this->outputCnt].output = output;

    this->outputCnt++;
    return 0;
  }
};

#endif