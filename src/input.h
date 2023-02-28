#ifndef __MEDIA_INPUT__
#define __MEDIA_INPUT__

#include "config.h"
#include <pthread.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/samplefmt.h>
}

class Input {

protected:
  AVStream *audioStream;
  AVStream *videoStream;
  AVFormatContext *fmtCtx;
  AVDictionary *opts;
  std::string filename;

public:
    Input(std::string filename, AVDictionary *opts) {
    this->filename = filename;
    this->opts = opts;
    std::cout << "inout Url -->" << filename << std::endl;
  }

  ~Input() {
    this->close();
  }

  int openFile(bool haAudio, bool hasVideo);
  void close();

  int readPacket(AVPacket *packet);
};

#endif