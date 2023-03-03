#ifndef __PACKAGER__
#define __PACKAGER__

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

class Packager {
private:
  bool firstAudioFrame;
  uint64_t firstAudioPts;
  bool firstVideoFrame;
  uint64_t firstVideoPts;
  uint64_t ptsStart;

  void setStartPts();

public:
  Packager() {
    this->firstAudioFrame = true;
    this->firstVideoFrame = true;
  }
  int pushAudio(AVFrame *frame);
  int pushVideo(AVFrame *frame);

  int pullAudio(AVFrame **frame);
  int pullVideo(AVFrame **frame);
};

#endif