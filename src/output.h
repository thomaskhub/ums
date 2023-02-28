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
class Output {
public:
  std::string filename;
  AVDictionary *opts;
  AVFormatContext *fmtCtx;
  AVStream *audioStream;
  AVStream *videoStream;
  std::string formatName;

public:
  Output(std::string filename, std::string formatName, AVDictionary *opts) {
    this->filename = filename;
    this->formatName = formatName;
    this->opts = opts;
  };

  int open();
  void close();

  int pushAudio(AVPacket *packet, int idx);
  int pushVideo(AVPacket *packet, int idx);
  void writeHeader();
};

#endif