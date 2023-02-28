#include "decoder.h"

int Decoder::open(AVStream *stream, std::string filterString) {
  int ret;
  this->filterString = filterString;
  this->stream = stream;

  this->decoder = (AVCodec *)avcodec_find_decoder(stream->codecpar->codec_id);
  if (!decoder) {
    return AVERROR(EINVAL);
  }

  this->decCtx = avcodec_alloc_context3(this->decoder);
  if (!decCtx) {
    return AVERROR(ENOMEM);
  }

  avcodec_parameters_to_context(this->decCtx, stream->codecpar);

  ret = avcodec_open2(this->decCtx, this->decoder, NULL);
  if (ret < 0) {
    avcodec_free_context(&this->decCtx);
    return ret;
  }

  this->frame = av_frame_alloc();
  if (!frame) {
    av_log(NULL, AV_LOG_ERROR, "could not alloc audio frame\n");
    avcodec_free_context(&this->decCtx);
    return AVERROR(ENOMEM);
  }

  if (this->decCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
    this->frame->nb_samples = this->decCtx->frame_size;
    this->frame->format = this->decCtx->sample_fmt;
    this->frame->channel_layout = this->decCtx->channel_layout;
  } else {
    this->frame->format = this->decCtx->pix_fmt;
    this->frame->width = this->decCtx->width;
    this->frame->height = this->decCtx->height;
  }

  ret = av_frame_get_buffer(this->frame, 0);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "could not get audio frame\n");
    av_frame_free(&this->frame);
    return AVERROR(ENOMEM);
  }

  return 0;
}

void Decoder::close() {
  av_frame_free(&this->frame);
  avcodec_free_context(&this->decCtx);
}

int Decoder::sendPacket(AVPacket *packet, AVFrame **outFrame) {
  int ret = avcodec_send_packet(this->decCtx, packet);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not send packet to audio decoder....\n");
    return ret;
  }

  while (ret >= 0) {
    ret = avcodec_receive_frame(this->decCtx, this->frame);

    if (ret == AVERROR(EAGAIN)) {
      break;
    }

    if (ret < 0) {
      return ret;
    }

    // init filter if its the first call
    if (this->isFirstFrame) {
      this->isFirstFrame = false;
      if (this->decCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
        this->filter = Filter(this->filterString, this->frame->format,
                              this->frame->sample_rate, this->frame->channel_layout,
                              this->stream->time_base);
        ret = this->filter.init();
        if (ret < 0) {
          av_log(NULL, AV_LOG_ERROR,
                 "rtmpInput::could not init audio filter\n");
          return ret;
        }
      } else if (this->decCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
        this->filter = Filter(this->filterString, this->frame->width,
                              this->frame->height, this->frame->format,
                              this->frame->sample_aspect_ratio, this->stream->time_base);
        ret = this->filter.init();
        if (ret < 0) {
          av_log(NULL, AV_LOG_ERROR,
                 "rtmpInput::could not init video filter\n");
          return ret;
        }
      }
    }
    ret = av_buffersrc_add_frame_flags(this->filter.srcCtx, this->frame,
                                       AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "videoFilterPush::not able to push frame\n");
      return ret;
    }

    av_frame_unref(this->frame);

    ret = av_buffersink_get_frame(this->filter.sinkCtx, *outFrame);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "videoFilterPull::not able to pull frame %i\n", ret);
      return ret;
    }
  }

  return 0;
}
