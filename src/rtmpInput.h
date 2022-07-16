#ifndef __RTMP_INPUT__
#define __RTMP_INPUT__

#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <pthread.h>

#include "mux.h"

typedef struct {
  int audioSampleRate;
  uint64_t audioChannelLayout;
  int audioChannels;
  int audioSampleFormat;
  AVRational audioTimeBase;
} RtmpInputInfo;

typedef struct {
  char *url;
  void (*audioCallback)(AVFrame *frame, AVRational framerate);
  void (*videoCallback)(AVFrame *frame, AVRational framerate);
} RtmpWorkerData;

int rtmpInputStart(char *url,
                   void (*audioCallback)(AVFrame *frame, AVRational framerate),
                   void (*videoCallback)(AVFrame *frame, AVRational framerate));

void rtmpInputStop();
void rtmpInputJoin();

#endif