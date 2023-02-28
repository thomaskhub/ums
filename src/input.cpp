#include "input.h"

void Input::close() {
  if (this->fmtCtx) {
    avformat_close_input(&this->fmtCtx);
  }
}

int Input::openFile(bool hasAudio, bool hasVideo) {
  int ret, i;
  bool foundAudio = false;
  bool foundVideo = false;

  this->fmtCtx = avformat_alloc_context();
  if (!this->fmtCtx) {
    return AVERROR(ENOMEM);
  }

  ret = avformat_open_input(&this->fmtCtx, this->filename.c_str(), 0, &this->opts);
  if (ret < 0) {
    this->close();
    return ret;
  }

  ret = avformat_find_stream_info(this->fmtCtx, 0);
  if (ret < 0) {
    this->close();
    return ret;
  }

  this->audioStream = NULL;
  this->videoStream = NULL;

  // We extract first video and audio other files will not be supported
  for (i = 0; i < fmtCtx->nb_streams; i++) {
    if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !foundAudio) {
      this->audioStream = fmtCtx->streams[i];
      foundAudio = true;

    } else if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !foundVideo) {
      this->videoStream = fmtCtx->streams[i];
      foundVideo = true;
    }

    if (foundAudio && foundVideo) {
      break;
    }
  }

  if ((hasAudio && this->audioStream == NULL) || (hasVideo && this->videoStream == NULL)) {
    close();
    return AVERROR(EIO);
  }

  return 0;
}

int Input::readPacket(AVPacket *packet) {
  if (!this->fmtCtx) {
    return AVERROR(EINVAL);
  }

  int ret = av_read_frame(this->fmtCtx, packet);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not rad any packet from rtmp...\n");
  }

  return ret;
}
