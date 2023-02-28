#include "encoder.h"

void Encoder::initAudio() {}

void Encoder::initVideo() {
  int ret;

  AVRational sampleAspectRatio;
  sampleAspectRatio.den = 1;
  sampleAspectRatio.num = 1;

  AVRational framerate;
  framerate.den = this->config.fps;
  framerate.num = 1;

  this->ctx->height = this->config.inHeight;
  this->ctx->height = this->config.outHeight;
  this->ctx->width = this->config.outWidth;
  this->ctx->sample_aspect_ratio = sampleAspectRatio;
  this->ctx->pix_fmt = (AVPixelFormat)this->config.inPixFormat;

  this->ctx->time_base = this->config.timebase;
  this->ctx->framerate = framerate;
  this->ctx->bit_rate = this->config.bitrate;
  this->ctx->keyint_min = this->config.gop / 10;
  this->ctx->gop_size = this->config.gop;
  this->ctx->rc_buffer_size = this->config.bitrate;
  this->ctx->rc_max_rate = this->config.bitrate;
  this->ctx->colorspace = AVCOL_SPC_BT709;
  this->ctx->color_trc = AVCOL_TRC_BT709;
  this->ctx->color_primaries = AVCOL_PRI_BT709;
  this->ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;

  av_opt_set(this->ctx->priv_data, "level", "3.1", 0);
  av_opt_set(this->ctx->priv_data, "preset", "veryfast", 0);

  av_opt_set(this->ctx->priv_data, "profile", "high", 0);
  av_opt_set_int(this->ctx->priv_data, "sc_threshold", 0, 0);
  av_opt_set_int(this->ctx->priv_data, "forced-idr", 1, 0);
}

int Encoder::init() {
  int ret = 0;
  this->codec = (AVCodec *)avcodec_find_encoder((AVCodecID)this->config.codecId);
  if (!this->codec) {
    return AVERROR(EINVAL);
  }

  this->ctx = avcodec_alloc_context3(this->codec);
  if (!this->ctx) {
    return AVERROR(EINVAL);
  }

  if (this->config.type == AVMEDIA_TYPE_AUDIO) {
    this->initAudio();
  } else if (this->config.type == AVMEDIA_TYPE_VIDEO) {
    this->initVideo();
  } else {
    avcodec_free_context(&this->ctx);
    return AVERROR(EINVAL);
  }

  ret = avcodec_open2(this->ctx, this->codec, NULL);
  if (ret < 0) {
    avcodec_free_context(&this->ctx);
  }

  return ret;
}

int Encoder::push(AVFrame *frame) {
  int ret;
  AVPacket *packet;
  // std::cout << "" << std::endl;
  // std::cout << "" << std::endl;
  // std::cout << "" << std::endl;
  // std::cout << "Encoder PTS:" << frame->pts << std::endl;
  // std::cout << "Encoder Packcet DTS:" << frame->pkt_dts << std::endl;
  // std::cout << "Encoder Packcet Pos:" << frame->pkt_pos << std::endl;
  // std::cout << "Encoder Packcet Duration:" << frame->pkt_duration << std::endl;
  // std::cout << "Encoder Packcet Size:" << frame->pkt_size << std::endl;

  frame->pkt_dts = frame->pts;

  // run through encoder
  ret = avcodec_send_frame(this->ctx, frame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "output::Could not copy frame to encoder %i\n", ret);
    exit(1); // TODO: can we handle this more gracefully?
  }

  if (this->config.type == AVMEDIA_TYPE_VIDEO) {
    while (ret >= 0) {
      ret = avcodec_receive_packet(this->ctx, packet);
      if (ret < 0) {
        // if (ret == AVERROR(EAGAIN)) {
        break;
      }
      output->pushVideo(packet, 0);
      av_packet_unref(packet);
    }

    return 0;
  }
}

int Encoder::pull(AVPacket **packet) { return 0; }