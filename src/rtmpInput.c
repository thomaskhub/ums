
#include "rtmpInput.h"

static pthread_t inputThread;
static uint32_t runThread = 1;
static AVCodecContext *audioDecCtx, *videoDecCtx;
static RtmpWorkerData wData;

static VideoFilter vFilter;
static int firstVideoFrame = 1;
static int rtmpRunning = 0;

void rtmpInputStop() { runThread = 0; }
void rtmpInputJoin() { pthread_join(inputThread, NULL); }

RtmpInputInfo rtmpInputInfo() {
  RtmpInputInfo tmp;
  tmp.audioSampleRate = audioDecCtx->sample_rate;
  tmp.audioChannelLayout = audioDecCtx->channel_layout;
  tmp.audioChannels = audioDecCtx->channels;
  tmp.audioSampleFormat = audioDecCtx->sample_fmt;
  tmp.audioTimeBase = audioDecCtx->time_base;
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
  AVFrame *videoOutFrame;

  ret = getEmptyVideoFrame(&videoOutFrame, VIDEO_PIX_FMT, VIDEO_WIDTH,
                           VIDEO_HEIGHT);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "rtmpInputStart::could not get empty frame\n");
    exit(1);
  }

  // this while loop is there to automatically reconnect to failing rtmp
  // input stream.
  while (runThread && ret >= 0) {
    printf("rtmp in the loop\n");
    rtmpRunning = 0;
    videoBufferReset(&rtmpInVBuffer);
    ret = openInput(&inFmtCtx, (char *)wData.url, &inputAudio, &inputVideo);
    if (ret < 0) {
      av_log(NULL, AV_LOG_DEBUG,
             "rtmpInput::Could not open rtmp input %i  url --> %s\n", ret,
             (char *)wData.url);
      sleep(3);
      ret = 0;  // make sure we restart the loop
      continue;
    }

    rtmpRunning = 1;

    ret = openDecoder(&audioDecCtx, &audioDecoder, inputAudio);
    if (ret < 0) {
      printf("Could not open audio decoder....\n");
      goto input;
    }

    ret = openDecoder(&videoDecCtx, &videoDecoder, inputVideo);
    if (ret < 0) {
      printf("Could not open vidoe decoder....\n");
      goto closeAudioDec;
    }
    tmp = videoDecCtx;

    audioFrame = av_frame_alloc();
    if (!audioFrame) {
      printf("could not alloc audio frame\n");
      goto closeVideoDec;
    }

    audioFrame->nb_samples = audioDecCtx->frame_size;
    audioFrame->format = audioDecCtx->sample_fmt;
    audioFrame->channel_layout = audioDecCtx->channel_layout;

    ret = av_frame_get_buffer(audioFrame, 0);
    if (ret < 0) {
      printf("could not get audio frame\n");
      goto freeAudioBuffer;
    }

    videoFrame = av_frame_alloc();
    if (!videoFrame) {
      printf("not able to allocate video frame\n");
      goto freeAudioBuffer;
    }

    videoFrame->format = videoDecCtx->pix_fmt;
    videoFrame->width = videoDecCtx->width;
    videoFrame->height = videoDecCtx->height;

    ret = av_frame_get_buffer(videoFrame, 0);
    if (ret < 0) {
      printf("not able get buffer for video frame\n");
      goto freeVideoBuffer;
    }

    // Read the input data decode the data which than can be forwarded
    // to processing/encoder
    while (runThread) {
      ret = av_read_frame(inFmtCtx, &rtmpPacket);
      if (ret < 0) {
        printf("Could not rad any packet from rtmp...\n");
        break;
      }

      if (rtmpPacket.stream_index == inputAudio->index) {
        ret = avcodec_send_packet(audioDecCtx, &rtmpPacket);
        if (ret < 0) {
          printf("Could not send packet to audio decoder....");
          break;
        }

        while (ret >= 0) {
          ret = avcodec_receive_frame(audioDecCtx, audioFrame);
          if (ret == AVERROR(EINVAL) || ret == AVERROR_EOF) {
            goto freeVideoBuffer;
          }

          if (wData.audioCallback && ret >= 0) {
            // let someone else take care of the data
            wData.audioCallback(audioFrame, inputAudio->r_frame_rate);
          }
        }
      } else if (rtmpPacket.stream_index == inputVideo->index) {
        ret = avcodec_send_packet(videoDecCtx, &rtmpPacket);
        if (ret < 0) {
          printf("Could not send packet to audio decoder....");
          break;
        }

        while (ret >= 0) {
          ret = avcodec_receive_frame(videoDecCtx, videoFrame);
          if (ret < 0 && ret != AVERROR(EAGAIN)) {
            goto freeVideoBuffer;
          }

          // Take the frame, filter it, then push it into double buffer for
          // creating one second chunks for further processing
          if (ret >= 0) {
            if (firstVideoFrame == 1) {
              firstVideoFrame = 0;
              ret = initVideoFilter(&vFilter, RTMPIN_VIDEO_FILTER,
                                    videoFrame->width, videoFrame->height,
                                    videoFrame->format, inputVideo->time_base,
                                    videoFrame->sample_aspect_ratio);
              if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "rtmpInput::could not init video filter\n");
                goto freeVideoBuffer;
              }
            }

            ret = videoFilterPush(&vFilter, videoFrame);
            if (ret < 0) {
              av_log(NULL, AV_LOG_ERROR,
                     "rtmpInput::could not push into filter\n");
              goto freeVideoBuffer;
            }

            ret = videoFilterPull(&vFilter, &videoOutFrame);
            if (ret < 0) {
              if (ret == AVERROR(EAGAIN)) {
                continue;
                // more frames are needed by filter to create an out
                // frame
              }

              av_log(NULL, AV_LOG_ERROR,
                     "rtmpInput::could not pull from filter\n");
              goto freeVideoBuffer;
            }

            ret = videoBufferPush(&rtmpInVBuffer, videoOutFrame, NULL);
            if (ret < 0) {
              av_log(NULL, AV_LOG_ERROR,
                     "rtmpInput::we lost data!!! restart...\n");
              goto freeVideoBuffer;
            }
          }

          if (ret >= 0 && wData.videoCallback) {
            // let someone else take care of the data
            wData.videoCallback(videoFrame, inputVideo->r_frame_rate);
          }
        }
      }

      av_packet_unref(&rtmpPacket);
    }

  freeVideoBuffer:
    av_frame_free(&videoFrame);
  freeAudioBuffer:
    av_frame_free(&audioFrame);
  closeVideoDec:
    closeCodec(&videoDecCtx);
  closeAudioDec:
    closeCodec(&audioDecCtx);
  input:
    closeInput(&inFmtCtx);

    if (firstVideoFrame == 0) {
      videoFilterFree(&vFilter);
      firstVideoFrame = 1;
    }
    ret = 0;  // make sure we restart the loop
  }
  printf("rtmp out of the loop\n");
  // Free all resource if rtmp input is stopepd which should never happen but
  // just in case
  av_frame_free(&videoFrame);
  av_frame_free(&audioFrame);
  av_frame_free(&videoOutFrame);
  closeCodec(&audioDecCtx);
  closeCodec(&videoDecCtx);
  closeInput(&inFmtCtx);
  videoBufferClose(&rtmpInVBuffer);
  printf("Input loop stoppped...\n");
}

void rtmpInputStart(char *url,
                    void (*audioCallback)(AVFrame *frame, AVRational frameRate),
                    void (*videoCallback)(AVFrame *frame,
                                          AVRational frameRate)) {
  int ret;

  wData.url = url;
  wData.audioCallback = audioCallback;
  wData.videoCallback = videoCallback;

  videoBufferInit(&rtmpInVBuffer, VIDEO_FRAME_RATE, VIDEO_PIX_FMT, VIDEO_WIDTH,
                  VIDEO_HEIGHT);

  pthread_create(&inputThread, NULL, worker, NULL);
}

int rtmpIsRunning() { return rtmpRunning; }
