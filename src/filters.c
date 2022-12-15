#include "filters.h"

int initAvFilter(UmsAvFilter *ctx, const char *fDesc, int width, int height,
                 int pixFmt, AVRational timebase, AVRational aspectRatio,
                 uint64_t smpFmt, int sampleRate, uint64_t chanLayout,
                 enum AVMediaType type) {
  const AVFilter *buffSrc;
  const AVFilter *buffSink;
  char inArgs[1024];
  char sinkArgs[512];

  const char *inbufStr = type == AVMEDIA_TYPE_VIDEO ? "buffer" : "abuffer";
  const char *outbufStr =
      type == AVMEDIA_TYPE_VIDEO ? "buffersink" : "abuffersink";

  enum AVPixelFormat pixFmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUVJ420P,
                                  AV_PIX_FMT_NONE};

  int ret = 0;

  buffSrc = avfilter_get_by_name(inbufStr);
  if (!buffSrc) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not get buffer\n");
    return AVERROR(ENODEV);
  }

  buffSink = avfilter_get_by_name(outbufStr);
  if (!buffSink) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not get buffer\n");
    return AVERROR(ENODEV);
  }

  ctx->in = avfilter_inout_alloc();
  if (!ctx->in) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not allocate inputs\n");
    return AVERROR(ENOMEM);
  }

  ctx->out = avfilter_inout_alloc();
  if (!ctx->out) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not allocate outputs\n");
    ret = AVERROR(ENOMEM);
    goto freeInput;
  }

  ctx->fGraph = avfilter_graph_alloc();
  if (!ctx->fGraph) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not allocate filter graph\n");
    ret = AVERROR(ENOMEM);
    goto freeOutput;
  }

  if (type == AVMEDIA_TYPE_VIDEO) {
    snprintf(inArgs, sizeof(inArgs),
             "pix_fmt=%d:time_base=%d/%d:video_size=%dx%d:pixel_aspect=%d/%d",
             pixFmt, timebase.num, timebase.den, width, height, aspectRatio.num,
             aspectRatio.den);
  } else if (type == AVMEDIA_TYPE_AUDIO) {
    snprintf(inArgs, sizeof(inArgs),
             "sample_fmt=%lu:time_base=%d/%d:sample_rate=%d:channel_layout=%lu",
             smpFmt, timebase.num, timebase.den, sampleRate, chanLayout);
  }

  ret = avfilter_graph_create_filter(&ctx->srcCtx, buffSrc, "in", inArgs, NULL,
                                     ctx->fGraph);

  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not create source buffer\n");
    ret = AVERROR(ENOMEM);
    goto freeFilterGraph;
  }

  ret = avfilter_graph_create_filter(&ctx->sinkCtx, buffSink, "out", NULL, NULL,
                                     ctx->fGraph);

  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not create sink buffer\n");
    ret = AVERROR(ENOMEM);
    goto freeFilterGraph;
  }

  ctx->out->name = av_strdup("in");
  ctx->out->filter_ctx = ctx->srcCtx;
  ctx->out->pad_idx = 0;
  ctx->out->next = NULL;

  ctx->in->name = av_strdup("out");
  ctx->in->filter_ctx = ctx->sinkCtx;
  ctx->in->pad_idx = 0;
  ctx->in->next = NULL;

  ret = avfilter_graph_parse_ptr(ctx->fGraph, fDesc, &ctx->in, &ctx->out, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not parse filter graph\n");
    goto freeFilterGraph;
  }

  ret = avfilter_graph_config(ctx->fGraph, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not configure graph\n");
    goto freeFilterGraph;
  }

  return 0;

freeFilterGraph:
  avfilter_graph_free(&ctx->fGraph);
freeOutput:
  avfilter_inout_free(&ctx->out);
freeInput:
  avfilter_inout_free(&ctx->in);

  return ret;
}

void videoFilterFree(UmsAvFilter *ctx) {
  if (ctx->fGraph) {
    avfilter_graph_free(&ctx->fGraph);
    avfilter_inout_free(&ctx->out);
    avfilter_inout_free(&ctx->in);
  }
}

int avFilterPush(UmsAvFilter *ctx, AVFrame *frame) {
  int ret;

  ret = av_buffersrc_add_frame_flags(ctx->srcCtx, frame,
                                     AV_BUFFERSRC_FLAG_KEEP_REF);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "videoFilterPush::not able to push frame\n");
    return ret;
  }

  return 0;
}

int avFilterPull(UmsAvFilter *ctx, AVFrame **frame) {
  int ret;
  ret = av_buffersink_get_frame(ctx->sinkCtx, *frame);
  return ret;
}