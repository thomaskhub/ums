#include "inputSwitch.h"

static PushAudio aPush;
static PushVideo vPush;
static RtmpIsActive checkRtmp;
static Filler filler;
static int64_t videoPTS = 0;
static int64_t audioPTS = 0;
static uint8_t runThread = 1;

void *_worker(void *args) {
  int rtmpStatus, lastRtmpStatus, fillerInProgress = 0;

  int64_t start, sleepTime, end, correction, aDuration, aFirstPts, aLastPTS,
      highResAudioPTS = 0;
  int64_t videoFpsDur, vFcnt = 0, aFcnt = 0, nextAItt = 44;
  double aErr = 0.0;

  int i = 0, ret;
  time_t now;
  uint32_t vBufOff, vBufLen;
  uint32_t aBufOff, aBufLen;
  AVFrame *vFrame, *aFrame;
  uint8_t firstAudioSample = 1;
  uint64_t videoFpsStart = av_gettime_relative();
  uint64_t procStart = av_gettime_relative();

  while (runThread) {
    rtmpStatus = checkRtmp();

    if (rtmpStatus == 1) {
      // align the start times after stream switch
      if (lastRtmpStatus == 0) {
        videoPTS = audioPTS > videoPTS ? audioPTS : videoPTS;
        audioPTS = videoPTS;
      }

      ret = avBufferPull(&rtmpInVBuffer, &vFrame);
      if (ret < 0 && ret != AVERROR(EAGAIN)) {
        av_log(NULL, AV_LOG_FATAL, "inputSwitch::rtmp video buffer unknown error\n");
        exit(1);
      } else if (ret >= 0) {
        vFrame->pts = videoPTS;
        vFrame->pkt_dts = videoPTS;
        vPush(vFrame);
        videoPTS += VIDEO_PTS_OFF;
        vFcnt++;
      }

      // audio
      ret = avBufferPull(&rtmpInABuffer, &aFrame);
      if (ret < 0 && ret != AVERROR(EAGAIN)) {
        av_log(NULL, AV_LOG_FATAL, "inputSwitch::rtmp audio buffer unknown error\n");
        exit(1);
      }

      if (ret >= 0) {
        if (firstAudioSample == 1) {
          aLastPTS = aFrame->pkt_dts;
          firstAudioSample = 0;

          aFrame->pts = audioPTS;
          aFrame->pkt_dts = audioPTS;
          aPush(aFrame);
          audioPTS += aFrame->pkt_duration;
        } else {
          aDuration = aFrame->pkt_dts - aLastPTS;
          aLastPTS = aFrame->pkt_dts;
          aFrame->pts = audioPTS;
          aFrame->pkt_dts = audioPTS;
          aPush(aFrame);
          audioPTS += aDuration;
          aFrame->pkt_duration = aDuration;
        }
        aFcnt += 1024;
      }
    }

    else {
      firstAudioSample = 1;
      start = av_gettime_relative();

      if (lastRtmpStatus == 1) {
        videoPTS = audioPTS > videoPTS ? audioPTS : videoPTS;
        audioPTS = videoPTS;
        highResAudioPTS = audioPTS * 1000;
      }

      // Push one second of video data
      for (i = 0; i < VIDEO_FRAME_RATE; i++) {
        now = time(NULL);
        if (now < filler.sessionStart) {
          filler.vPreFiller->pts = videoPTS;
          filler.vPreFiller->pkt_dts = videoPTS;
          vPush(filler.vPreFiller);
          videoPTS += VIDEO_PTS_OFF;

        } else if (now >= filler.sessionStart && now < filler.sessionEnd) {
          vPush(filler.vSessionFiller);
          filler.vSessionFiller->pts = videoPTS;
          filler.vSessionFiller->pkt_dts = videoPTS;
          videoPTS += VIDEO_PTS_OFF;
        } else {
          filler.vPostFiller->pts = videoPTS;
          filler.vPostFiller->pkt_dts = videoPTS;
          vPush(filler.vPostFiller);
          videoPTS += VIDEO_PTS_OFF;
        }

        vFcnt++;
      }

      // push audio samples and correct if we have pushed too many
      for (i = 0; i < nextAItt; i++) {
        filler.audioFiller->pts = audioPTS;
        filler.audioFiller->pkt_dts = audioPTS;
        aPush(filler.audioFiller);
        highResAudioPTS += AUDIO_PTS_OFF;
        audioPTS = highResAudioPTS / 1000;
        aFcnt += 1024;
      }

      aErr += abs(44100 - 1024 * nextAItt);
      if (aErr >= 1024) {
        nextAItt = 43;
        aErr -= 1024;
      } else {
        nextAItt = 44;
      }

      end = av_gettime_relative();
      sleepTime = 1000000 - (end - start);
      if (sleepTime < 0) {
        av_log(NULL, AV_LOG_WARNING, "InputSwith processing is too slow\n");
      } else {
        av_usleep(sleepTime);
      }
    }

    lastRtmpStatus = rtmpStatus;

    // Processing status::
    // TODO: we need to push this either into redis or directly into the db
    // so that it will be available in the frontend to display statistics
    videoFpsDur = av_gettime_relative() - videoFpsStart;
    if (videoFpsDur >= 1000000) {
      printf("##### --> Video Rate=%f\n",
             (double)vFcnt * 1000000 / (double)videoFpsDur);
      printf("##### --> Audio Rate=%f\n",
             (double)aFcnt * 1000000 / (double)videoFpsDur);
      videoFpsStart = av_gettime_relative();
      printf("### runtime --> %lu\n", (av_gettime_relative() - procStart) / 1000000);
      vFcnt = 0;
      aFcnt = 0;
    }
  }
}

int prepareVideoFiller(char *path, AVFrame **frame) {
  AVFrame *imgFrame;
  AVFormatContext *inCtx;
  AvFilter vFilter;
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

int prepareAudioFiller(AVFrame **frame) {
  int ret, i;
  ret = getEmptyAvFrame(frame, 0, 0, 0, AUDIO_SAMPLE_FMT, AUDIO_NB_SAMPLES,
                        AUDIO_CH_LAYOUT, AVMEDIA_TYPE_AUDIO);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "prepareAudioFiller::could not get empty frame\n");
    exit(1);
  }

  (*frame)->data[0][0] = 0;
}

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
           "inputSwitch::could not prepare post-session filler\n");
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