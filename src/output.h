#ifndef __OUTPUT__
#define __OUTPUT__

#include "config.h"
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

#define OUTPUT_MODE_RTMP 0
#define OUTPUT_MODE_MPEGTS 1
#define OUTPUT_MODE_DASH 2

struct OutputConfig {
  const AVCodecContext *codecContext;
};

class Output {
public:
  std::string filename;
  AVFormatContext *fmtCtx;
  int codecContLength;
  int videoCnt;
  int audioCnt;
  AVStream *ouputStreams[12]; // max 12 streams allowed
  std::string formatName;
  OutputConfig *config;
  int cfgLength;

  int createStreams();

public:
  Output(std::string filename, OutputConfig *config, int length, int mode) {
    this->filename = filename;
    this->config = config;
    this->cfgLength = length;

    if (mode == OUTPUT_MODE_RTMP) {
      this->formatName = "flv";
    } else if (mode == OUTPUT_MODE_MPEGTS) {
      this->formatName = "mpegts";
    } else if (mode == OUTPUT_MODE_DASH) {
      this->formatName = "dash";
    }
  };

  int open();
  void close();

  int push(AVPacket *packet, int idx);
};

#endif