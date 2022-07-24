#include "inputSwitch.h"

static PushAudio aPush;
static PushVideo vPush;
static RtmpIsActive checkRtmp;
static Filler filler;
static uint32_t videoPTS = 0;
static uint8_t runThread = 1;

void *_worker(void *args) {
  int rtmpStatus, fillerInProgress = 0;
  int64_t start, sleepTime, end, correction;
  int i = 0, ret;
  time_t now;
  uint32_t vBufOff, vBufLen, vBufId;
  AVFrame *vFrame;

  while (runThread) {
    rtmpStatus = checkRtmp();
    if (rtmpStatus == 1) {
      ret = videoBufferPull(&rtmpInVBuffer, &vBufOff, &vBufLen, &vBufId);
      if (ret < 0 && ret != AVERROR(EAGAIN)) {
        av_log(NULL, AV_LOG_FATAL,
               "inputSwitch::rtmp video buffer unknown error\n");
        exit(1);
      }

      if (ret >= 0) {
        for (i = 0; i < vBufLen; i++) {
          vFrame = (&rtmpInVBuffer.doubleBuffer[vBufOff])[i];
          vFrame->pts = videoPTS;
          vFrame->pkt_dts = videoPTS;
          vPush(vFrame);
          videoPTS += VIDEO_PTS_OFF;
        }
        videoBufferDone(&rtmpInVBuffer, vBufId);
      }

    } else {
      printf("Debug:: %li\n", av_gettime_relative() - start);
      start = av_gettime_relative();

      // Push one second of video data
      for (i = 0; i < VIDEO_FRAME_RATE; i++) {
        now = time(NULL);
        if (now < filler.sessionStart) {
          filler.vPreFiller->pts = videoPTS;
          filler.vPreFiller->pkt_dts = videoPTS;
          vPush(filler.vPreFiller);
          videoPTS += VIDEO_PTS_OFF;

          // aPush(filler.aPreFiller);
        } else if (now >= filler.sessionStart && now < filler.sessionEnd) {
          vPush(filler.vSessionFiller);
          filler.vSessionFiller->pts = videoPTS;
          filler.vSessionFiller->pkt_dts = videoPTS;
          videoPTS += VIDEO_PTS_OFF;
          // aPush(filler.aSessionFiller);
        } else {
          filler.vPostFiller->pts = videoPTS;
          filler.vPostFiller->pkt_dts = videoPTS;
          vPush(filler.vPostFiller);
          videoPTS += VIDEO_PTS_OFF;
          // aPush(filler.aPostFiller);
        }
      }

      end = av_gettime_relative();
      sleepTime = 1000000 - (end - start);
      if (sleepTime < 0) {
        printf("Warning::InputSwitch::Processing is to slow!!!!\n");
      } else {
        av_usleep(sleepTime);
      }
    }
  }
}

int prepareVideoFiller(char *path, AVFrame **frame) {
  AVFrame *imgFrame;
  AVFormatContext *inCtx;
  VideoFilter vFilter;
  AVRational timebase = {.den = 1000, .num = 1};
  AVRational aspectRatio = {.den = 1, .num = 1};

  int ret;

  ret = getFrameFromImage(&inCtx, path, &imgFrame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "inputSwitch::could not read frame from image\n");
    return ret;
  }

  ret = initVideoFilter(&vFilter, FILLER_VIDEO_FILTER, imgFrame->width,
                        imgFrame->height, VIDEO_PIX_FMT, timebase, aspectRatio);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not init video filter\n");
    goto freeFrame;
  }

  ret = getEmptyVideoFrame(frame, VIDEO_PIX_FMT, VIDEO_WIDTH, VIDEO_HEIGHT);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not create empty video\n");
    goto freeFilter;
  }

  // Convert the image into the correct internal representation
  ret = videoFilterPush(&vFilter, imgFrame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not push into filter\n");
    goto freeFilter;
  }

  ret = videoFilterPull(&vFilter, frame);
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

  // TODO: implment error handling is time strings are not propper format.
  // for now we assume the one calling this function will ensure this.
  filler.sessionStart = isoTimeToEpoch(sessionStart);
  filler.sessionEnd = isoTimeToEpoch(sessionEnd);
  filler.streamStart = isoTimeToEpoch(streamStart);

  pthread_create(&inputSwitchThread, NULL, _worker, NULL);

  return 0;

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