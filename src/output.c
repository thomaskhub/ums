#include "output.h"

int startOutput(OutputCtxT* data) {
  int ret;

  ret = initEncoder(&data->videoEncCtx, &data->videoEncoder, AV_CODEC_ID_H264);
  if (ret < 0) {
    printf("Could not init video encoder ...\n");
    return ret;
  }

  data->videoEncCtx->height = data->inHeight;
  data->videoEncCtx->width = data->inWidth;
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

  if (data->url) {
    // setup rtmp output
    ret = openOutput(&data->rtmpOutCtx, data->url, &data->outAudioRtmp,
                     &data->outVideoRtmp, "flv");
    if (ret < 0) {
      printf("Could not open output file abort...\n");
      goto closeCodec;
    }

    ret = avcodec_parameters_from_context(data->outVideoRtmp->codecpar,
                                          data->videoEncCtx);
    if (ret < 0) {
      printf("could not setup video output params\n");
      goto closeCodec;
    }

    // data->outVideoRtmp->time_base = data->outVideoRtmp->time_base;
    data->outRtmpPacket = av_packet_alloc();
    if (!data->outRtmpPacket) {
      printf("could no allocated packet for rtmp output...\n");
      goto closeCodec;
    }

    ret = avformat_write_header(data->rtmpOutCtx, NULL);
    if (ret < 0) {
      printf("could not write header for rtmp output...\n");
      goto closeCodec;
    }
    av_dump_format(data->rtmpOutCtx, 0, "", 1);
  }

  if (data->path) {
    // setup TS recording
  }

  return 0;
closeRtmpOut:
  closeOutput(&data->rtmpOutCtx);
closeCodec:
  closeCodec(&data->videoEncCtx);

  if (data->outRtmpPacket) {
    av_packet_free(&data->outRtmpPacket);
  }
  return ret;
}

// Todo: this function only works with the rtmp output.... bcause we are using
// outVideoRtmp
void outputWriteVideoFrame(OutputCtxT* data, AVFrame* frame) {
  int ret;
  ret = avcodec_send_frame(data->videoEncCtx, frame);
  if (ret < 0) {
    printf("Could not copy frame to encoder...\n");
    exit(1);
  }

  while (ret >= 0) {
    ret = avcodec_receive_packet(data->videoEncCtx, data->outRtmpPacket);
    if (ret == 0) {
      // videoEncPacket->stream_index = outputVideo->index;

      av_packet_rescale_ts(data->outRtmpPacket, data->timebase,
                           data->outVideoRtmp->time_base);  //

      ret = av_interleaved_write_frame(data->rtmpOutCtx, data->outRtmpPacket);
    }
    if (ret < 0) {
      break;
    }
  }
  av_packet_unref(data->outRtmpPacket);
}

// void outputWriteAudioFrame(OutputCtxT* data, AVFrame* frame) {
//   av_packet_rescale_ts(data->outRtmpPacket, data->timebase,
//                        data->outVideoRtmp->time_base);  //

//   ret = av_interleaved_write_frame(data->rtmpOutCtx, data->outRtmpPacket);
// }
// if (ret < 0) {
//   break;
// }
// }
// av_packet_unref(data->outRtmpPacket);
// }
