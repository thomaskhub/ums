#include "filter.h"

int Filter::init() {
  const AVFilter *buffSrc;
  const AVFilter *buffSink;
  char inArgs[1024];
  char sinkArgs[512];

  const char *inbufStr = this->type == AVMEDIA_TYPE_VIDEO ? "buffer" : "abuffer";
  const char *outbufStr =
      this->type == AVMEDIA_TYPE_VIDEO ? "buffersink" : "abuffersink";

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

  this->in = avfilter_inout_alloc();
  if (!this->in) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not allocate inputs\n");
    return AVERROR(ENOMEM);
  }

  this->out = avfilter_inout_alloc();
  if (!this->out) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not allocate outputs\n");
    ret = AVERROR(ENOMEM);
    goto freeInput;
  }

  this->fGraph = avfilter_graph_alloc();
  if (!this->fGraph) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not allocate filter graph\n");
    ret = AVERROR(ENOMEM);
    goto freeOutput;
  }

  if (type == AVMEDIA_TYPE_VIDEO) {
    snprintf(inArgs, sizeof(inArgs),
             "pix_fmt=%d:time_base=%d/%d:video_size=%dx%d:pixel_aspect=%d/%d",
             this->pixFmt, this->timebase.num, this->timebase.den, this->width,
             this->height, this->aspectRatio.num, this->aspectRatio.den);
  } else if (type == AVMEDIA_TYPE_AUDIO) {
    snprintf(inArgs, sizeof(inArgs),
             "sample_fmt=%lu:time_base=%d/%d:sample_rate=%d:channel_layout=%lu",
             this->smpFmt, this->timebase.num, this->timebase.den, this->sampleRate,
             this->chanLayout);
  }

  ret = avfilter_graph_create_filter(&this->srcCtx, buffSrc, "in", inArgs, NULL,
                                     this->fGraph);

  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not create source buffer\n");
    ret = AVERROR(ENOMEM);
    goto freeFilterGraph;
  }

  ret = avfilter_graph_create_filter(&this->sinkCtx, buffSink, "out", NULL, NULL,
                                     this->fGraph);

  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not create sink buffer\n");
    ret = AVERROR(ENOMEM);
    goto freeFilterGraph;
  }

  this->out->name = av_strdup("in");
  this->out->filter_ctx = this->srcCtx;
  this->out->pad_idx = 0;
  this->out->next = NULL;

  this->in->name = av_strdup("out");
  this->in->filter_ctx = this->sinkCtx;
  this->in->pad_idx = 0;
  this->in->next = NULL;

  ret = avfilter_graph_parse_ptr(this->fGraph, this->filterString.c_str(), &this->in, &this->out, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not parse filter graph\n");
    goto freeFilterGraph;
  }

  ret = avfilter_graph_config(this->fGraph, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error::initAvFilter::could not configure graph\n");
    goto freeFilterGraph;
  }

  return 0;

freeFilterGraph:
  avfilter_graph_free(&this->fGraph);
freeOutput:
  avfilter_inout_free(&this->out);
freeInput:
  avfilter_inout_free(&this->in);

  return ret;
}