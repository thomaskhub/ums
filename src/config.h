#ifndef __CONFIG__
#define __CONFIG__
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

class Config {
private:
public:
  const char *primaryInput = std::getenv("primaryInput");
  const char *rtmpSecondaryInput = std::getenv("rtmpSecondaryInput");
  const char *outputUrl = std::getenv("outputUrl");
  const char *fillerJpg = std::getenv("fillerJpg");
  bool dvrEnabled = std::getenv("dvr") ? true : false;
  bool debug = std::getenv("debug") ? true : false;
  int logLevel = std::getenv("loglevel") ? std::stoi(std::getenv("loglevel")) : AV_LOG_ERROR;

  Config() {
    av_log_set_level(logLevel);
  }

  bool validateInput();
  bool isDvrEnabled() { return dvrEnabled; }

  // Some static config variables
  static const int VIDEO_PIX_FMT = AV_PIX_FMT_YUV420P;
  static const int VIDEO_WIDTH = 1280;
  static const int VIDEO_HEIGHT = 720;
  static const int AUDIO_SAMPLE_FMT = AV_SAMPLE_FMT_FLTP;
  static const int AUDIO_NB_SAMPLES = 1024;
  static const int AUDIO_CH_LAYOUT = AV_CH_LAYOUT_MONO;
  static const int AUDIO_RATE = 44100;
  const AVRational TIMEBASE = {.num = 1, .den = 1000000};

  // #define VIDEO_FRAME_RATE 25
  // #define VIDEO_TIMEBASE_NUM 1
  // #define VIDEO_TIMEBASE_DEN 12800
  // #define AUDIO_TIMEBASE_NUM 1

  // #define VIDEO_PIX_FMT AV_PIX_FMT_YUV420P
  // #define AUDIO_CH_LAYOUT AV_CH_LAYOUT_MONO

  // #define FILLER_VIDEO_FILTER "scale=1280:720,format=yuv420p"
  // #define RTMPIN_VIDEO_FILTER "scale=1280:720,format=yuv420p,fps=fps=25"
  // #define RTMPIN_AUDIO_FILTER "aresample=44100,asetnsamples=n=1024:p=0,aformat=channel_layouts=mono,volume=1"

  // #define TIMEBASE_NUM 1
  // #define TIMEBASE_DEN 1000000
  // #define VFRAME_DURATION 1000000 / 25
  // #define AFRAME_DURATION(samples) ceil(1000000.0 * samples / 44100.0)
};

#endif