#ifndef __DECODER__
#define __DECODER__

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

#include "config.h"
#include "filter.h"
#include <fstream>

class Decoder {
  bool isFirstFrame = true;
  Filter filter;
  AVStream *stream;
  AVFrame *frame;
  std::string filterString;
  AVCodecContext *decCtx;
  AVCodec *decoder;

public:
  Decoder(){};
  ~Decoder() {
    this->close();
  }
  int open(AVStream *stream, std::string filterString);
  void close();
  int sendPacket(AVPacket *packet, AVFrame **outFrame);
};

#endif