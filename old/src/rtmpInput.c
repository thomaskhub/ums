
#include "rtmpInput.h"

static pthread_t inputThread;
static uint32_t runThread = 1;
static AVCodecContext *audioDecCtx, *videoDecCtx;
static RtmpWorkerData wData;

static UmsAvFilter vFilter;
static UmsAvFilter aFilter;
static int firstVideoFrame = 1;
static int firstAudioFrame = 1;
volatile static int rtmpRunning = 0;

extern volatile GlobalT global;

void rtmpInputStop() { runThread = 0; }
void rtmpInputJoin() { pthread_join(inputThread, NULL); }

RtmpInputInfo rtmpInputInfo() {
  RtmpInputInfo tmp;
  tmp.audioSampleRate = audioDecCtx->sample_rate;
  tmp.audioChannelLayout = audioDecCtx->channel_layout;
  tmp.audioChannels = audioDecCtx->channels;
  tmp.audioSampleFormat = audioDecCtx->sample_fmt;
  tmp.audioTimeBase = audioDecCtx->time_base;
  return tmp;
}

AVRational rtmpGetVideoTimebase() {
  if (videoDecCtx)
    return videoDecCtx->time_base;

  AVRational tmp = {
      .den = 1,
      .num = 1,
  };
  return tmp;
}

AVRational rtmpGetAudioTimebase() {
  if (videoDecCtx)
    return audioDecCtx->time_base;

  AVRational tmp = {
      .den = 1,
      .num = 1,
  };
  return tmp;
}

void *worker(void *data) {
  int ret;
  AVFormatContext *inFmtCtx;
  AVCodecContext *tmp;
  AVStream *inputAudio = NULL, *inputVideo = NULL;
  AVPacket rtmpPacket;
  AVCodec *audioDecoder, *videoDecoder;
  AVFrame *videoFrame, *audioFrame;
  AVRational tbase;
  AVFrame *videoOutFrame, *audioOutFrame;
  AVDictionary *rtmpServerOpts = NULL;
  AVInputFormat *rtmpFmt;

  ret = getEmptyAvFrame(&videoOutFrame, VIDEO_PIX_FMT, VIDEO_WIDTH,
                        VIDEO_HEIGHT, 0, 0, 0, AVMEDIA_TYPE_VIDEO);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "rtmpInputStart::could not get empty frame\n");
    exit(1);
  }

  ret = getEmptyAvFrame(&audioOutFrame, 0, 0, 0, AUDIO_SAMPLE_FMT,
                        AUDIO_NB_SAMPLES, AUDIO_CH_LAYOUT, AVMEDIA_TYPE_AUDIO);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "rtmpInputStart::could not get empty frame\n");
    exit(1);
  }

  // this while loop is there to automatically reconnect to failing rtmp
  // input stream.
  while (runThread && ret >= 0) {
    global.rtmpInputStatus = "stopped";
    rtmpRunning = 0;
    avBufferClear(&rtmpInABuffer);
    avBufferClear(&rtmpInVBuffer);

    ret = openInput(&inFmtCtx, (char *)wData.url, &inputAudio, &inputVideo, 0, 0, 1, 1);
    if (ret < 0) {
      av_log(NULL, AV_LOG_DEBUG,
             "rtmpInput::Could not open rtmp input %i  url --> %s\n", ret,
             (char *)wData.url);

      sleep(3);
      ret = 0; // make sure we restart the loop
      continue;
    }

    rtmpRunning = 1;
    global.rtmpInputStatus = "running";

    ret = openDecoder(&audioDecCtx, &audioDecoder, inputAudio);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "Could not open audio decoder....\n");
      goto freeAll;
    }

    ret = openDecoder(&videoDecCtx, &videoDecoder, inputVideo);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "Could not open vidoe decoder....\n");
      goto freeAll;
    }
    tmp = videoDecCtx;

    audioFrame = av_frame_alloc();
    if (!audioFrame) {
      av_log(NULL, AV_LOG_ERROR, "could not alloc audio frame\n");
      goto freeAll;
    }

    audioFrame->nb_samples = audioDecCtx->frame_size;
    audioFrame->format = audioDecCtx->sample_fmt;
    audioFrame->channel_layout = audioDecCtx->channel_layout;

    ret = av_frame_get_buffer(audioFrame, 0);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "could not get audio frame\n");
      goto freeAll;
    }

    videoFrame = av_frame_alloc();
    if (!videoFrame) {
      av_log(NULL, AV_LOG_ERROR, "not able to allocate video frame\n");
      goto freeAll;
    }

    videoFrame->format = videoDecCtx->pix_fmt;
    videoFrame->width = videoDecCtx->width;
    videoFrame->height = videoDecCtx->height;

    ret = av_frame_get_buffer(videoFrame, 0);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "not able get buffer for video frame\n");
      goto freeAll;
    }

    // Read the input data decode the data which than can be forwarded
    // to processing/encoder
    while (runThread) {

      ret = av_read_frame(inFmtCtx, &rtmpPacket);

      if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not rad any packet from rtmp...\n");
        break;
      }

      if (rtmpPacket.stream_index == inputAudio->index) {
        while (avBufferFull(&rtmpInABuffer)) {
          av_usleep(1000);
        }

        ret = avcodec_send_packet(audioDecCtx, &rtmpPacket);
        if (ret < 0) {
          av_log(NULL, AV_LOG_ERROR, "Could not send packet to audio decoder....\n");
          continue;
          break;
        }

        while (ret >= 0) {
          ret = avcodec_receive_frame(audioDecCtx, audioFrame);
          if (ret == AVERROR(EINVAL) || ret == AVERROR_EOF) {
            goto freeAll;
          }

          if (ret == AVERROR(EAGAIN)) {
            break;
          }

          if (audioFrame->pts < inputVideo->start_time) {
            break; // read the nect packet and drop this frame until we
                   // reache the start
          }

          if (ret == AVERROR(EAGAIN)) {
            continue;
          }

          if (firstAudioFrame == 1) {
            AVRational noSampleAspect;
            firstAudioFrame = 0;
            ret = initAvFilter(&aFilter, RTMPIN_AUDIO_FILTER, 0, 0, 0,
                               inputAudio->time_base, noSampleAspect,
                               audioFrame->format, audioFrame->sample_rate,
                               audioFrame->channel_layout, AVMEDIA_TYPE_AUDIO);

            if (ret < 0) {
              av_log(NULL, AV_LOG_ERROR,
                     "rtmpInput::could not init video filter\n");
              goto freeAll;
            }

            global.audioFilter = &aFilter;
          }

          ret = avFilterPush(&aFilter, audioFrame);
          if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "rtmpInput::could not push audio into filter\n");
            goto freeAll;
          }

          av_frame_unref(audioFrame);

          ret = avFilterPull(&aFilter, &audioOutFrame);
          if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
              continue;
            }

            av_log(NULL, AV_LOG_ERROR,
                   "rtmpInput::could not pull from filter\n");
            goto freeAll;
          }

          ret = avBufferPush(&rtmpInABuffer, audioOutFrame);
          if (ret < 0) {
            av_log(NULL, AV_LOG_DEBUG, "rtmpInput::buffer full retry...\n");
            continue;
          }
          av_frame_unref(audioOutFrame);
        }

      } else if (rtmpPacket.stream_index == inputVideo->index) {
        // Wait until buffer has some more space before processing data
        while (avBufferFull(&rtmpInVBuffer)) {
          av_usleep(1000);
        }

        ret = avcodec_send_packet(videoDecCtx, &rtmpPacket);
        if (ret < 0) {
          av_log(NULL, AV_LOG_ERROR, "Could not send packet to video decoder....\n");
          break;
        }

        while (ret >= 0) {
          ret = avcodec_receive_frame(videoDecCtx, videoFrame);
          if (ret < 0 && ret != AVERROR(EAGAIN)) {
            goto freeAll;
          }

          if (ret == AVERROR(EAGAIN)) {
            av_frame_unref(videoFrame);
            break;
          }

          // Take the frame, filter it, then push it into double buffer for
          // creating one second chunks for further processing
          if (ret >= 0) {
            if (videoFrame->pts < inputVideo->start_time) {
              av_frame_unref(videoFrame);
              break; // read the nect packet and drop this frame until we
                     // reache the start
            }

            if (firstVideoFrame == 1) {
              firstVideoFrame = 0;
              ret = initAvFilter(
                  &vFilter, RTMPIN_VIDEO_FILTER, videoFrame->width,
                  videoFrame->height, videoFrame->format, inputVideo->time_base,
                  videoFrame->sample_aspect_ratio, 0, 0, 0, AVMEDIA_TYPE_VIDEO);
              if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "rtmpInput::could not init video filter\n");
                goto freeAll;
              }

              global.videoFilter = &vFilter;
            }

            ret = avFilterPush(&vFilter, videoFrame);
            if (ret < 0) {
              av_log(NULL, AV_LOG_ERROR,
                     "rtmpInput::could not push into filter\n");
              goto freeAll;
            }

            av_frame_unref(videoFrame);

            ret = avFilterPull(&vFilter, &videoOutFrame);
            if (ret < 0) {
              if (ret == AVERROR(EAGAIN)) {
                continue;
                // more frames are needed by filter to create an out
                // frame
              }

              av_log(NULL, AV_LOG_ERROR,
                     "rtmpInput::could not pull from filter\n");
              goto freeAll;
            }

            // we can push here because we checked before that we have at least
            // one free space so this should never fail.
            ret = avBufferPush(&rtmpInVBuffer, videoOutFrame);
            if (ret < 0) {
              av_log(NULL, AV_LOG_DEBUG,
                     "rtmpInput::video buffer full retry...\n");
              continue;
            }
            av_frame_unref(videoOutFrame);
          }
        }
      }

      av_packet_unref(&rtmpPacket);
    }

  freeAll:
    if (videoFrame) {
      av_frame_free(&videoFrame);
    }

    if (audioFrame) {
      av_frame_free(&audioFrame);
    }

    if (videoDecCtx) {
      closeCodec(&videoDecCtx);
    }

    if (audioDecCtx) {
      closeCodec(&audioDecCtx);
    }

    if (inFmtCtx) {
      closeInput(&inFmtCtx);
    }

    if (firstVideoFrame == 0) {
      videoFilterFree(&vFilter);
      firstVideoFrame = 1;
    }

    if (firstAudioFrame == 0) {
      videoFilterFree(&aFilter);
      firstAudioFrame = 1;
    }

    ret = 0; // make sure we restart the loop
  }

  // Free all resource if rtmp input is stopepd which should never happen but
  // just in case
  av_frame_free(&videoFrame);
  av_frame_free(&audioFrame);
  av_frame_free(&videoOutFrame);
  closeCodec(&audioDecCtx);
  closeCodec(&videoDecCtx);
  closeInput(&inFmtCtx);
}

void rtmpInputStart(char *url) {
  int ret;

  wData.url = url;

  rtmpInABuffer.buffer = 0;
  rtmpInABuffer.type = AVMEDIA_TYPE_AUDIO;

  rtmpInVBuffer.buffer = 0;
  rtmpInVBuffer.type = AVMEDIA_TYPE_VIDEO;
  pthread_create(&inputThread, NULL, worker, NULL);
}

int rtmpIsRunning() { return rtmpRunning; }
