#ifndef __FILTER__
#define __FILTER__

#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

class Filter {

public:
  AVFilterGraph *fGraph;
  AVFilterContext *sinkCtx;
  AVFilterContext *srcCtx;
  AVFilterInOut *in;
  AVFilterInOut *out;
  bool initDone = false;
  std::string filterString;
  int width;
  int height;
  int pixFmt;
  AVRational timebase;
  AVRational aspectRatio;
  uint64_t smpFmt;
  int sampleRate;
  uint64_t chanLayout;
  enum AVMediaType type;

  /**
   * @brief Create a Video filter
   *
   * @param filterString
   * @param width
   * @param height
   * @param pixFmt
   * @param timebase
   * @param aspectRatio
   */
  Filter(std::string filterString, int width, int height, int pixFmt, AVRational aspectRatio, AVRational timebase) {
    this->filterString = filterString;
    this->width = width;
    this->height = height;
    this->pixFmt = pixFmt;
    this->timebase = timebase;
    this->aspectRatio = aspectRatio;
    this->type = AVMEDIA_TYPE_VIDEO;
  }

  /**
   * @brief Create an audio filter
   *
   * @param filterString
   * @param smpFmt
   * @param sampleRate
   * @param chanLayout
   */
  Filter(std::string filterString, int smpFmt, int sampleRate, uint64_t chanLayout, AVRational timebase) {
    this->filterString = filterString;
    this->smpFmt = smpFmt;
    this->sampleRate = sampleRate;
    this->chanLayout = chanLayout;
    this->timebase = timebase;
    this->type = AVMEDIA_TYPE_AUDIO;
  }

  Filter(){};

  int init();
};

#endif