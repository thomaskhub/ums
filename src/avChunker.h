#ifndef __AV_CHUNKER__
#define __AV_CHUNKER__

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

class AVChunker {

public:
  AVChunker(){};

  int push(AVFrame *frame);
  int pull(AVFrame *frame);
};

#endif