#include "audioEncoder.h"

int audioEncoderInit(AudioEncCtx *ctx) {
  int ret;
  ret = initEncoder(&ctx->encCtx, &ctx->encoder, AV_CODEC_ID_AAC);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "audioEncoderInit::init AAC encoder failed.\n");
    return ret;
  }

  ctx->timebase = ctx->encCtx->time_base;
  ctx->encCtx->bit_rate = ctx->bitrate;
  ctx->encCtx->sample_fmt = AUDIO_SAMPLE_FMT;
  ctx->encCtx->sample_rate = AUDIO_RATE;
  ctx->encCtx->channel_layout = AUDIO_CH_LAYOUT;
  ctx->encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  ret = avcodec_open2(ctx->encCtx, ctx->encoder, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "audioEncoderInit:: open encoder failed\n");
    goto closeCodec;
  }

  ctx->packet = av_packet_alloc();
  if (!ctx->packet) {
    ret = AVERROR(ENOMEM);
    av_log(NULL, AV_LOG_ERROR, "audioEncoderInit:: allocate packet failed\n");
    goto closeCodec;
  }

  return 0;

closeCodec:
  closeCodec(&ctx->encCtx);
  return ret;
}

int audioEncoderRun(AudioEncCtx *ctx) {
  int ret;

  ret = avcodec_send_frame(ctx->encCtx, ctx->frame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "audioEncoder::frame copy to encoder failed\n");
    if (ret == AVERROR(EAGAIN)) {
      return ret;
    }
    exit(1);
  }

  ret = avcodec_receive_packet(ctx->encCtx, ctx->packet);
  if (ret == AVERROR(EAGAIN)) {
    return ret;
  }

  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "audioEncoder::encoder packet rec failed\n");
    exit(1);
  }
}