#include "output.h"

static pthread_t threadOpenRtmp;
static uint8_t rtmpOutRunning = 0;
static uint8_t runThread = 1;

void outputStopRtmp() { runThread = 0; }
void outputRtmpJoin() { pthread_join(threadOpenRtmp, NULL); }

static void* openRtmp(void* user) {
  OutputCtxT* data = (OutputCtxT*)user;
  while (runThread) {
    int ret;
    data->rtmpOutCtx = NULL;

    // if output is not available retry until it becomes available
    do {
      ret = openOutput(&data->rtmpOutCtx, data->url, &data->outAudioRtmp,
                       &data->outVideoRtmp, "flv");
      if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "openRtmp::could not open rtmp\n");
      }
      sleep(3);
    } while (ret < 0);

    ret = avcodec_parameters_from_context(data->outVideoRtmp->codecpar,
                                          data->videoEncCtx);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR,
             "openRtmp::could not setup video codec params\n");
      goto closeOutput;
    }

    ret = avformat_write_header(data->rtmpOutCtx, NULL);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "openRtmp::could not write rtmp out header\n");
      goto closeOutput;
    }

    rtmpOutRunning = 1;
    return NULL;

  closeOutput:
    closeOutput(&data->rtmpOutCtx);
  end:
    data->rtmpOutCtx = NULL;
    rtmpOutRunning = 0;
  }

  if (data->rtmpOutCtx) {
    closeOutput(&data->rtmpOutCtx);
  }
}

static int openMpegtsRecording(OutputCtxT* data) {
  int ret;

  ret = openOutput(&data->recCtx, data->path, &data->outAudioRec,
                   &data->outVideoRec, "mpegts");
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openRec::could not open recoding output\n");
    goto end;
  }

  ret = avcodec_parameters_from_context(data->outVideoRec->codecpar,
                                        data->videoEncCtx);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openRec::could not open recoding output\n");
    goto closeOutput;
  }

  ret = avformat_write_header(data->recCtx, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openRec::could not write rtmp out header\n");
    goto closeOutput;
  }
  return 0;

closeOutput:
  closeOutput(&data->rtmpOutCtx);
end:
  data->recCtx = NULL;
  return ret;
}

int startOutput(OutputCtxT* data) {
  int ret;

  if (data->type == AVMEDIA_TYPE_AUDIO) {
    // TODO: for now skip audio  but needs to be implemented next
    return -1;
  }

  data->gop = 100;
  data->inWidth = VIDEO_WIDTH;
  data->inHeight = VIDEO_HEIGHT;
  data->format = VIDEO_PIX_FMT;
  data->timebase.num = VIDEO_TIMEBASE_NUM;
  data->timebase.den = VIDEO_TIMEBASE_DEN;
  data->sampleAspectRatio.den = 1;
  data->sampleAspectRatio.num = 1;

  data->videoEncCtx = NULL;
  data->recCtx = NULL;

  ret = initEncoder(&data->videoEncCtx, &data->videoEncoder, AV_CODEC_ID_H264);
  if (ret < 0) {
    printf("Could not init video encoder ...\n");
    return ret;
  }

  data->videoEncCtx->height = data->outHeight;
  data->videoEncCtx->width = data->outWidth;
  data->videoEncCtx->sample_aspect_ratio = data->sampleAspectRatio;
  data->videoEncCtx->pix_fmt = data->format;
  data->videoEncCtx->time_base = data->timebase;
  data->videoEncCtx->bit_rate = data->bitrate;
  data->videoEncCtx->keyint_min = data->gop;
  data->videoEncCtx->gop_size = data->gop;
  data->videoEncCtx->rc_buffer_size = data->videoEncCtx->bit_rate;
  data->videoEncCtx->rc_max_rate = data->videoEncCtx->bit_rate;
  data->videoEncCtx->colorspace = AVCOL_SPC_BT709;
  data->videoEncCtx->color_trc = AVCOL_TRC_BT709;
  data->videoEncCtx->color_primaries = AVCOL_PRI_BT709;

  av_opt_set(data->videoEncCtx->priv_data, "preset", "veryfast", 0);
  av_opt_set(data->videoEncCtx->priv_data, "tune", "stillimage", 0);

  ret = avcodec_open2(data->videoEncCtx, data->videoEncoder, NULL);
  if (ret < 0) {
    closeCodec;
  }

  data->packet = av_packet_alloc();
  if (!data->packet) {
    av_log(NULL, AV_LOG_ERROR, "output:: not able to allocate packet\n");
    goto closeCodec;
  }

  if (data->url) {
    ret = pthread_create(&threadOpenRtmp, NULL, openRtmp, (void*)data);
    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING,
             "output::not able to start rtmpOut thread\n");
    }
  }

  if (data->path) {
    data->recCtx = NULL;
    ret = openMpegtsRecording(data);
    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING,
             "output::not able to write recording file\n");
    }
  }

  if (data->inWidth != data->outWidth || data->inHeight != data->outHeight) {
    data->filterEna = 1;
    snprintf(data->filterDesc, 128, "scale=%d:%d", data->outWidth,
             data->outHeight);

    ret = initAvFilter(&data->vFilter, data->filterDesc, data->inWidth,
                       data->inHeight, data->format, data->timebase,
                       data->sampleAspectRatio, 0, 0, 0, AVMEDIA_TYPE_VIDEO);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "output::could not init video filter\n");
      goto closeRecordingOut;
    }

    ret = getEmptyAvFrame(&data->encoderFrame, data->format, data->outWidth,
                          data->outHeight, 0, 0, 0, AVMEDIA_TYPE_VIDEO);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not create empty video\n");
      goto closeRecordingOut;
    }

  } else {
    data->filterEna = 0;
  }

  return 0;

closeRecordingOut:
  if (data->path && data->recCtx) {
    closeOutput(&data->recCtx);
  };
closeRtmpOut:
  if (data->url && data->rtmpOutCtx) {
    closeOutput(&data->rtmpOutCtx);
  };
closeCodec:
  closeCodec(&data->videoEncCtx);

  if (data->packet) {
    av_packet_free(&data->packet);
  }
  return ret;
}

void outputWriteVideoFrame(OutputCtxT* data, AVFrame* frame) {
  int ret;
  AVPacket *recPacket, *rtmpPacket, *dashPacket;

  if (data->filterEna) {
    ret = avFilterPush(&data->vFilter, frame);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "output::could not push into filter\n");
      return;
    }

    ret = avFilterPull(&data->vFilter, &data->encoderFrame);
    if (ret < 0) {
      if (ret == AVERROR(EAGAIN)) {
        return;
      }

      av_log(NULL, AV_LOG_ERROR, "output::could not pull from filter\n");
      return;
    }

    ret = avcodec_send_frame(data->videoEncCtx, data->encoderFrame);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "output::Could not copy frame to encoder\n");
      exit(1);
    }

    av_frame_unref(data->encoderFrame);  // TODO: not sure if that is good
    // av_frame_free(&data->encoderFrame);  // TODO: not sure if that is good

  } else {
    ret = avcodec_send_frame(data->videoEncCtx, frame);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "output::Could not copy frame to encoder\n");
      exit(1);
    }
  }

  while (ret >= 0) {
    ret = avcodec_receive_packet(data->videoEncCtx, data->packet);
    if (ret == 0) {
      if (data->url && data->rtmpOutCtx && rtmpOutRunning) {
        // handle rtmp output if enabled
        rtmpPacket = av_packet_clone(data->packet);

        av_packet_rescale_ts(rtmpPacket, data->timebase,
                             data->outVideoRtmp->time_base);
        ret = av_interleaved_write_frame(data->rtmpOutCtx, rtmpPacket);
        if (ret < 0) {
          // if we have some error on rtmp output restart it
          rtmpOutRunning = 0;
          ret = pthread_create(&threadOpenRtmp, NULL, openRtmp, (void*)data);
          if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING,
                   "output::not able to start rtmpOut thread\n");
          }
        }
        av_packet_unref(rtmpPacket);
      }

      if (data->path && data->recCtx) {
        // handle MPEGTS recording if enabled
        recPacket = av_packet_clone(data->packet);

        av_packet_rescale_ts(recPacket, data->timebase,
                             data->outVideoRec->time_base);

        ret = av_interleaved_write_frame(data->recCtx, recPacket);
        av_packet_unref(recPacket);
      }

      // always push dash/hls cannot be disabled
      dashPacket = av_packet_clone(data->packet);
      dashPacket->stream_index = data->streamIdx;

      av_packet_rescale_ts(
          dashPacket, data->timebase,
          data->dashCtx->dashStreams[data->streamIdx]->time_base);

      dashWritePacket(data->dashCtx, dashPacket);
      av_packet_unref(dashPacket);
      av_packet_free(&dashPacket);
    }

    av_packet_unref(data->packet);
    if (ret < 0) {
      break;
    }
  }
}

void outputClose(OutputCtxT* data) {
  if (data->path && data->recCtx) {
    closeOutput(&data->recCtx);
  };

  outputStopRtmp();
  outputRtmpJoin();

  if (data->filterEna) {
    videoFilterFree(&data->vFilter);
  }

  if (data->packet->buf) {
    av_packet_unref(data->packet);
  }

  if (data->videoEncCtx) {
    closeCodec(&data->videoEncCtx);
  }
}

// TODO:
void outputWriteAudioFrame(OutputCtxT* data, AVFrame* frame) {
  //    av_packet_rescale_ts(data->outRtmpPacket, data->timebase,
  //                         data->outVideoRtmp->time_base);  //

  //   ret = av_interleaved_write_frame(data->rtmpOutCtx, data->outRtmpPacket);
  // }
  // if (ret < 0) {
  //   break;
  // }
  // }
  // av_packet_unref(data->outRtmpPacket);
}
