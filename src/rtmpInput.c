
#include "rtmpInput.h"

pthread_t inputThread;
uint32_t runThread = 1;
AVCodecContext *audioDecCtx, *videoDecCtx;
RtmpWorkerData wData;

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

  // this while loop is there to automatically reconnect to failing rtmp
  // input stream.
  while (runThread && ret >= 0) {
    ret = openInput(&inFmtCtx, (char *)wData.url, &inputAudio, &inputVideo);
    if (ret < 0) {
      printf("Could not open the input %i  url --> %s\n", ret,
             (char *)wData.url);
      sleep(3);
      ret = 0;  // make sure we restart the loop
      continue;
    }

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
    ret = 0;  // make sure we restart the loop
  }

  // Free all resource if rtmp input is stopepd which should never happen but
  // just in case
  av_frame_free(&videoFrame);
  av_frame_free(&audioFrame);
  closeCodec(&audioDecCtx);
  closeCodec(&videoDecCtx);
  closeInput(&inFmtCtx);
  printf("Input loop stoppped...\n");
}

int rtmpInputStart(char *url,
                   void (*audioCallback)(AVFrame *frame, AVRational frameRate),
                   void (*videoCallback)(AVFrame *frame,
                                         AVRational frameRate)) {
  printf("RTMP...\n");
  wData.url = url;
  wData.audioCallback = audioCallback;
  wData.videoCallback = videoCallback;

  pthread_create(&inputThread, NULL, worker, NULL);

  return 0;
}