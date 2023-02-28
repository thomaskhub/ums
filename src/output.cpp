#include "output.h"

int Output::open() {
  int ret;

  ret = avformat_alloc_output_context2(&this->fmtCtx, NULL, "mpegts",
                                       this->filename.c_str());
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openOotput:: error %i\n", ret);
    return AVERROR(EINVAL);
  }

  if (!this->fmtCtx) {
    return AVERROR(ENOMEM);
  }

  this->videoStream = avformat_new_stream(this->fmtCtx, NULL);
  if (!this->videoStream) {
    this->close();
    return AVERROR(ENOMEM);
  }

  // this->audioStream = avformat_new_stream(this->fmtCtx, NULL);
  // if (!this->audioStream) {
  //   this->close();
  //   return AVERROR(ENOMEM);
  // }

  if (!(this->fmtCtx->flags & AVFMT_NOFILE)) {
    ret = avio_open(&(this->fmtCtx)->pb, this->filename.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      this->close();
      return AVERROR(ENOENT);
    }
  }

  return 0;
}

void Output::close() {
  avformat_free_context(this->fmtCtx);
}

int Output::pushAudio(AVPacket *packet, int idx) {
}

int tmp = 0;
int Output::pushVideo(AVPacket *packet, int idx) {

  AVPacket *recPacket;
  int ret;

  recPacket = av_packet_clone(packet);
  AVRational timebase = Config().TIMEBASE;
  recPacket->dts = tmp + 40000;
  recPacket->pts = tmp + 40000;
  tmp += 40000;

  // av_packet_rescale_ts(recPacket, timebase, this->videoStream->time_base);
  recPacket->stream_index = 0;
  // std::cout << "here 5" << std::endl;
  ret = av_interleaved_write_frame(this->fmtCtx, recPacket);
  // std::cout << "----------------------------" << ret << std::endl;
  av_packet_unref(recPacket);
  // std::cout << "here 6" << std::endl;
}

void Output::writeHeader() {
  int ret = avformat_write_header(this->fmtCtx, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "open output::could not write header\n");
    this->close();
  }
}