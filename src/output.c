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

    ret = avcodec_parameters_from_context(data->outAudioRtmp->codecpar,
                                          data->audioEnc->encCtx);
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

static int openMpegtsRecording(OutputCtxT* ctx) {
  int ret;

  ret = openOutput(&ctx->recCtx, ctx->path, &ctx->outAudioRec,
                   &ctx->outVideoRec, "mpegts");
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openRec::could not open recoding output\n");
    goto end;
  }

  ret = avcodec_parameters_from_context(ctx->outVideoRec->codecpar,
                                        ctx->videoEncCtx);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openRec::could not open recoding output\n");
    goto closeOutput;
  }

  ret = avcodec_parameters_from_context(ctx->outAudioRec->codecpar,
                                        ctx->audioEnc->encCtx);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openRec::could not open recoding output\n");
    goto closeOutput;
  }

  ret = avformat_write_header(ctx->recCtx, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openRec::could not write rtmp out header\n");
    goto closeOutput;
  }
  return 0;

closeOutput:
  closeOutput(&ctx->recCtx);
end:
  ctx->recCtx = NULL;
  return ret;
}

int startOutput(OutputCtxT* ctx) {
  int ret;

  // if (video->type == AVMEDIA_TYPE_AUDIO) {
  //   // TODO: for now skip audio  but needs to be implemented next
  //   // Audio filerte on input has normaliyed sample rate, sample format and
  //   // to mono output
  //   return -1;
  // }

  ctx->gop = 100;
  ctx->inWidth = VIDEO_WIDTH;
  ctx->inHeight = VIDEO_HEIGHT;
  ctx->format = VIDEO_PIX_FMT;
  ctx->timebase.num = VIDEO_TIMEBASE_NUM;
  ctx->timebase.den = VIDEO_TIMEBASE_DEN;
  ctx->sampleAspectRatio.den = 1;
  ctx->sampleAspectRatio.num = 1;

  ctx->videoEncCtx = NULL;
  ctx->recCtx = NULL;

  ret = initEncoder(&ctx->videoEncCtx, &ctx->videoEncoder, AV_CODEC_ID_H264);
  if (ret < 0) {
    printf("Could not init video encoder ...\n");
    return ret;
  }

  ctx->videoEncCtx->height = ctx->outHeight;
  ctx->videoEncCtx->width = ctx->outWidth;
  ctx->videoEncCtx->sample_aspect_ratio = ctx->sampleAspectRatio;
  ctx->videoEncCtx->pix_fmt = ctx->format;
  ctx->videoEncCtx->time_base = ctx->timebase;
  ctx->videoEncCtx->bit_rate = ctx->bitrate;
  ctx->videoEncCtx->keyint_min = ctx->gop;
  ctx->videoEncCtx->gop_size = ctx->gop;
  ctx->videoEncCtx->rc_buffer_size = ctx->videoEncCtx->bit_rate;
  ctx->videoEncCtx->rc_max_rate = ctx->videoEncCtx->bit_rate;
  ctx->videoEncCtx->colorspace = AVCOL_SPC_BT709;
  ctx->videoEncCtx->color_trc = AVCOL_TRC_BT709;
  ctx->videoEncCtx->color_primaries = AVCOL_PRI_BT709;
  ctx->videoEncCtx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;

  av_opt_set(ctx->videoEncCtx->priv_data, "level", "3.1", 0);
  av_opt_set(ctx->videoEncCtx->priv_data, "preset", "veryfast", 0);
  av_opt_set(ctx->videoEncCtx->priv_data, "tune", "stillimage", 0);
  av_opt_set(ctx->videoEncCtx->priv_data, "profile", "high", 0);

  ret = avcodec_open2(ctx->videoEncCtx, ctx->videoEncoder, NULL);
  if (ret < 0) {
    closeCodec;
  }

  ctx->packet = av_packet_alloc();
  if (!ctx->packet) {
    av_log(NULL, AV_LOG_ERROR, "output:: not able to allocate packet\n");
    goto closeCodec;
  }

  if (ctx->url) {
    ret = pthread_create(&threadOpenRtmp, NULL, openRtmp, (void*)ctx);
    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING,
             "output::not able to start rtmpOut thread\n");
    }
  }

  if (ctx->path) {
    ctx->recCtx = NULL;
    ret = openMpegtsRecording(ctx);
    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING,
             "output::not able to write recording file\n");
    }
  }

  if (ctx->inWidth != ctx->outWidth || ctx->inHeight != ctx->outHeight) {
    ctx->filterEna = 1;
    snprintf(ctx->filterDesc, 128, "scale=%d:%d", ctx->outWidth,
             ctx->outHeight);

    ret = initAvFilter(&ctx->vFilter, ctx->filterDesc, ctx->inWidth,
                       ctx->inHeight, ctx->format, ctx->timebase,
                       ctx->sampleAspectRatio, 0, 0, 0, AVMEDIA_TYPE_VIDEO);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "output::could not init video filter\n");
      goto closeRecordingOut;
    }

    ret = getEmptyAvFrame(&ctx->encoderFrame, ctx->format, ctx->outWidth,
                          ctx->outHeight, 0, 0, 0, AVMEDIA_TYPE_VIDEO);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "inputSwitch::could not create empty video\n");
      goto closeRecordingOut;
    }

  } else {
    ctx->filterEna = 0;
  }

  return 0;

closeRecordingOut:
  if (ctx->path && ctx->recCtx) {
    closeOutput(&ctx->recCtx);
  };
closeRtmpOut:
  if (ctx->url && ctx->rtmpOutCtx) {
    closeOutput(&ctx->rtmpOutCtx);
  };
closeCodec:
  closeCodec(&ctx->videoEncCtx);

  if (ctx->packet) {
    av_packet_free(&ctx->packet);
  }
  return ret;
}

int outputWriteAudioPacket(OutputCtxT* output) {
  AVPacket *pkt, *recPacket, *rtmpPacket, *dashPacket;
  int ret;

  // pkt = av_packet_clone(ctx->packet);
  // av_packet_rescale_ts(pkt, ctx->timebase, output->audio->time_base);

  // ret = av_interleaved_write_frame(output->recCtx, pkt);
  // av_packet_unref(pkt);

  if (output->url && output->rtmpOutCtx && rtmpOutRunning) {
    // handle rtmp output if enabled
    rtmpPacket = av_packet_clone(output->audioEnc->packet);

    av_packet_rescale_ts(rtmpPacket, output->timebase,
                         output->outVideoRtmp->time_base);

    rtmpPacket->stream_index = 1;
    ret = av_interleaved_write_frame(output->rtmpOutCtx, rtmpPacket);
    av_packet_unref(rtmpPacket);
  }

  if (output->path && output->recCtx) {
    // // handle MPEGTS recording if enabled
    recPacket = av_packet_clone(output->audioEnc->packet);

    av_packet_rescale_ts(recPacket, output->timebase,
                         output->outAudioRec->time_base);

    recPacket->stream_index = 1;
    ret = av_interleaved_write_frame(output->recCtx, recPacket);
    av_packet_unref(recPacket);
  }

  // Only the main output will push audio to dash
  if (output->name && strcmp(output->name, "main") == 0) {
    dashPacket = av_packet_clone(output->audioEnc->packet);
    dashPacket->stream_index = output->dashCtx->streamLen;

    av_packet_rescale_ts(dashPacket, output->timebase,
                         output->dashCtx->dashASteam->time_base);

    dashWritePacket(output->dashCtx, dashPacket);
    av_packet_unref(dashPacket);
    av_packet_free(&dashPacket);
  }
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

  } else {
    ret = avcodec_send_frame(data->videoEncCtx, frame);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "output::Could not copy frame to encoder\n");
      exit(1);
    }
  }

  while (ret >= 0) {
    ret = avcodec_receive_packet(data->videoEncCtx, data->packet);
    if (ret == AVERROR(EAGAIN)) {
      break;
    }

    if (ret < 0) {
      av_log(NULL, AV_LOG_DEBUG, "outputWriteVideoFrame::enc packet failed\n");
      exit(1);
    }

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
