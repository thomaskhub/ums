#include "inputSwitch.h"

extern GlobalT global;

static PushAudio aPush;
static PushVideo vPush;

static RtmpIsActive checkRtmp;
static Filler filler;
static uint8_t runThread = 1;
static int64_t _toffset = 0;

void resetTime() {
  _toffset = av_gettime_relative();
}

int64_t getTime() {
  return av_gettime_relative() - _toffset;
}

/**
 * @brief Get the Filler Video Frame based on the current time
 *
 * @return AVFrame*
 */
AVFrame *getFillerVideoFrame() {
  time_t now = time(NULL);

  if (now < filler.sessionStart) {
    return filler.vPreFiller;
  } else if (now >= filler.sessionStart && now < filler.sessionEnd) {
    return filler.vSessionFiller;
  } else {
    return filler.vPostFiller;
  }

  return NULL;
}

/**
 * @brief Get the Next Video Frame either coming from filler videos or from
 * rtmp input:
 *
 * TODO: if we want we can add another RTMP input here to have a backup
 * ingest stream.
 *
 * @param vFrame
 * @return int
 */
int getNextVideoFrame(AVFrame **vFrame) {
  int ret;
  int rtmpStatus;
  AVFrame *tmpFrame;

  if (checkRtmp()) {

    ret = avBufferPull(&rtmpInVBuffer, &tmpFrame);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      av_log(NULL, AV_LOG_FATAL, "inputSwitch::rtmp video buffer unknown error\n");
      exit(1);
    }

    if (ret < 0) {
      return ret;
    }

    *vFrame = tmpFrame;
    return 0;
  } else {
    *vFrame = getFillerVideoFrame();
    return 0;
  }
}

/**
 * @brief Get the Next Audio Frame either filler of from RTMP ingest
 *
 * @param aFrame
 * @return int
 */
int getNextAudioFrame(AVFrame **aFrame) {
  int ret;
  int i;
  int rtmpStatus;
  AVFrame *tmpFrame;

  if (checkRtmp()) {

    ret = avBufferPull(&rtmpInABuffer, &tmpFrame);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      av_log(NULL, AV_LOG_FATAL, "inputSwitch::rtmp video buffer unknown error\n");
      exit(1);
    }

    if (ret < 0) {
      return ret;
    }

    *aFrame = tmpFrame;
    return 0;

  } else {
    // rtmp is not running so push filler instead
    *aFrame = filler.audioFiller;
    return 0;
  }
}

/**
 * @brief Video Frame processing (Timestamping)
 *
 * @param nextVideoPTS
 * @return uint64_t
 */
uint64_t handleVideoFrames(int64_t *nextVideoPTS) {
  AVFrame *frame;
  int64_t now = getTime();

  if (*nextVideoPTS <= now) {
    if (getNextVideoFrame(&frame) >= 0) {
      frame->pts = *nextVideoPTS;
      frame->pkt_dts = *nextVideoPTS;
      frame->best_effort_timestamp = *nextVideoPTS;
      frame->pkt_duration = VFRAME_DURATION;
      vPush(frame);
      *nextVideoPTS += VFRAME_DURATION;
      global.inputSwitchVFrameCnt++;
    }
  }
}

/**
 * @brief Handle Audio Processing (Timestamping)
 *
 * @param nextAudioPTS
 * @return uint64_t
 */
uint64_t handleAudioFrames(int64_t *nextAudioPTS) {
  AVFrame *frame;
  int64_t now = getTime();
  if (*nextAudioPTS <= now) {
    if (getNextAudioFrame(&frame) >= 0) {
      frame->pts = *nextAudioPTS;
      frame->pkt_dts = *nextAudioPTS;
      frame->best_effort_timestamp = *nextAudioPTS;
      frame->pkt_duration = AFRAME_DURATION;
      aPush(frame);

      // set the next sample point
      *nextAudioPTS += AFRAME_DURATION;
      global.inputSwitchAFrameCnt += AUDIO_NB_SAMPLES;
    }
  }
}

void handleMediaStats(uint64_t videoFrameCnt, uint64_t audioFrameCnt) {

  //  int64_t loopDurationAvgData[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  // double loopDurationAvg;

  // double framrateAvgData[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  // double fpsAvg;

  // double sampleRateAvgData[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  // double sampleRateAvg;

  //  videoFpsDur = av_gettime_relative() - videoFpsStart;
  // if (videoFpsDur >= 1000000) {
  //   fpsAvg = movingAverageDouble((double *)framrateAvgData, 1, (double)vFcnt * 1000000 / (double)videoFpsDur);
  //   sampleRateAvg = movingAverageDouble((double *)sampleRateAvgData, 1, (double)aFcnt * 1000000 / (double)videoFpsDur);

  //   // printf("##### --> Video Rate=%f\n",
  //   //        (double)vFcnt * 1000000 / (double)videoFpsDur);

  //   printf("##### --> Video Rate=%f\n", fpsAvg);
  //   printf("##### --> Audio Rate=%f\n", sampleRateAvg);

  //   // printf("##### --> Audio Rate=%f\n",
  //   //        (double)aFcnt * 1000000 / (double)videoFpsDur);
  //   videoFpsStart = av_gettime_relative();
  //   printf("### runtime --> %lu\n", (av_gettime_relative() - procStart) / 1000000);
  //   vFcnt = 0;
  //   aFcnt = 0;
  // }
}

void *_worker(void *args) {
  // Variables needed for the updated approach:
  int64_t nextVideoPTS = 0;
  int64_t nextAudioPTS = 0;
  uint64_t videoFrameCnt = 0;
  uint64_t audioFrameCnt = 0;

  global.inputSwitchStartTime = av_gettime_relative();
  resetTime(); // timer starts from 0

  nextVideoPTS = getTime() + VFRAME_DURATION;
  nextAudioPTS = getTime();

  while (runThread) {
    videoFrameCnt = handleVideoFrames(&nextVideoPTS);
    audioFrameCnt = handleAudioFrames(&nextAudioPTS);
  }
}

/**
 * @brief Preapares the video filler frame
 *
 * @param path
 * @param frame
 * @return int
 */
int prepareVideoFiller(char *path, AVFrame **frame) {
  AVFrame *imgFrame;
  AVFormatContext *inCtx;
  UmsAvFilter vFilter;
  AVRational timebase = {.den = 1000, .num = 1};
  AVRational aspectRatio = {.den = 1, .num = 1};

  int ret;

  ret = getFrameFromImage(&inCtx, path, &imgFrame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not read frame from image\n");
    return ret;
  }

  ret = initAvFilter(&vFilter, FILLER_VIDEO_FILTER, imgFrame->width,
                     imgFrame->height, VIDEO_PIX_FMT, timebase, aspectRatio, 0,
                     0, 0, AVMEDIA_TYPE_VIDEO);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not init video filter\n");
    goto freeFrame;
  }

  ret = getEmptyAvFrame(frame, VIDEO_PIX_FMT, VIDEO_WIDTH, VIDEO_HEIGHT, 0, 0,
                        0, AVMEDIA_TYPE_VIDEO);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not create empty video\n");
    goto freeFilter;
  }

  // Convert the image into the correct internal representation
  ret = avFilterPush(&vFilter, imgFrame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not push into filter\n");
    goto freeFilter;
  }

  ret = avFilterPull(&vFilter, frame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not push into filter\n");
    goto freeFilter;
  }

  videoFilterFree(&vFilter);
  return 0;
freeFilter:
  videoFilterFree(&vFilter);
freeFrame:
  av_frame_free(frame);
  return ret;
}

/**
 * @brief Prepares the audio filler frame
 *
 * @param frame
 * @return int
 */
int prepareAudioFiller(AVFrame **frame) {
  int ret, i;
  ret = getEmptyAvFrame(frame, 0, 0, 0, AUDIO_SAMPLE_FMT, AUDIO_NB_SAMPLES,
                        AUDIO_CH_LAYOUT, AVMEDIA_TYPE_AUDIO);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "prepareAudioFiller::could not get empty frame\n");
    exit(1);
  }

  for (i = 0; i < (*frame)->linesize[0]; i++) {
    (*frame)->data[0][i] = 0;
  }
  return 0;
}

/**
 * @brief Initializes the input switch
 *
 * @param _vPush        callback function to push video frames to the next stage
 * @param _aPush        callback function to push audio frames to the next stage
 * @param _checkRtmp    callback to check if rtmpInput is running
 * @param preFiller     pre filler image path
 * @param sessionFiller session filler image path
 * @param postFiller    post filler image path
 * @param streamStart   ISO time string
 * @param sessionStart  ISO time string
 * @param sessionEnd    ISO time string
 *
 * @return int
 */
int inputSwitchInit(PushVideo _vPush, PushAudio _aPush, RtmpIsActive _checkRtmp,
                    char *preFiller, char *sessionFiller, char *postFiller,
                    char *streamStart, char *sessionStart, char *sessionEnd) {
  int ret;
  if (_checkRtmp == NULL) {
    return AVERROR(EINVAL);
  }

  vPush = _vPush;
  aPush = _aPush;

  checkRtmp = _checkRtmp;

  ret = prepareVideoFiller(preFiller, &filler.vPreFiller);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "inputSwitch::could not prepare pre-session filler\n");
    return ret;
  }

  ret = prepareVideoFiller(sessionFiller, &filler.vSessionFiller);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "inputSwitch::could not prepare session filler\n");
    goto freePreSsession;
  }

  ret = prepareVideoFiller(postFiller, &filler.vPostFiller);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "inputSwitch::could not prepare post-session filler\n");
    goto freeSession;
  }

  ret = prepareAudioFiller(&filler.audioFiller);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "inputSwitch::could not prepare audio filler\n");
    goto freePost;
  }

  filler.sessionStart = isoTimeToEpoch(sessionStart);
  filler.sessionEnd = isoTimeToEpoch(sessionEnd);
  filler.streamStart = isoTimeToEpoch(streamStart);

  pthread_create(&inputSwitchThread, NULL, _worker, NULL);

  return 0;

freePost:
  av_frame_free(&filler.vPostFiller);
freeSession:
  av_frame_free(&filler.vSessionFiller);
freePreSsession:
  av_frame_free(&filler.vPreFiller);
  return ret;
}

void inputSwitchClose() {
  if (filler.vPreFiller) {
    av_frame_unref(filler.vPreFiller);
  }
  if (filler.vSessionFiller) {
    av_frame_unref(filler.vSessionFiller);
  }
  if (filler.vPostFiller) {
    av_frame_unref(filler.vPostFiller);
  }

  runThread = 0;
  pthread_join(inputSwitchThread, NULL);
}