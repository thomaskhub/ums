#include "output.h"

int Output::createStreams() {
  int ret = 0;
  int i = 0;

  for (i = 0; i < this->cfgLength; i++) {
    this->ouputStreams[i] = avformat_new_stream(this->fmtCtx, NULL);
    if (!this->ouputStreams[i]) {
      this->close();
      return AVERROR(EINVAL);
    }

    ret = avcodec_parameters_from_context(this->ouputStreams[i]->codecpar,
                                          this->config[i].codecContext);

    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "output::could not set codecpar\n");
      this->close();
      return ret;
    }
  }

  return ret;
}

int Output::open() {
  int ret = 0;

  ret = avformat_alloc_output_context2(&this->fmtCtx, NULL, "mpegts",
                                       this->filename.c_str());
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openOotput:: error %i\n", ret);
    return AVERROR(EINVAL);
  }

  if (!this->fmtCtx) {
    return AVERROR(ENOMEM);
  }

  ret = this->createStreams();
  if (ret < 0) {
    return ret;
  }

  if (!(this->fmtCtx->flags & AVFMT_NOFILE)) {
    ret = avio_open(&(this->fmtCtx)->pb, this->filename.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      this->close();
      return AVERROR(ENOENT);
    }
  }

  ret = avformat_write_header(this->fmtCtx, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "open output::could not write header\n");
    this->close();
  }

  return 0;
}

void Output::close() {
  avformat_free_context(this->fmtCtx);
}

int Output::push(AVPacket *packet, int idx) {

  AVPacket *recPacket;
  int ret;

  recPacket = av_packet_clone(packet);
  AVRational timebase = Config().TIMEBASE;
  recPacket->stream_index = idx;

  av_packet_rescale_ts(recPacket, timebase, this->ouputStreams[idx]->time_base);

  ret = av_interleaved_write_frame(this->fmtCtx, recPacket);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "output::could not write packet to file\n");
  }

  av_packet_unref(recPacket);

  return 0;
}
