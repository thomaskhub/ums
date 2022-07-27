#include "utils.h"

int64_t startTime = 0;

void initPTS() { startTime = av_gettime_relative(); }

int64_t getPTS() { return (av_gettime_relative() - startTime); }

int64_t getPTScaled(AVRational timebase) {
  int64_t curPts = getPTS();
  return av_rescale(curPts, timebase.den, timebase.num);
}

int getFrameFromImage(AVFormatContext **ctx, char *path,
                      AVFrame **pictureFrame) {
  int ret;
  AVStream *videoStream, *audioStream;
  AVCodecContext *decCtx;
  AVCodec *decoder;
  AVPacket *pkt;
  struct SwsContext *swsCtx;

  ret = openInput(ctx, path, &audioStream, &videoStream);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::getFrameFromImage::could not open file\n");
    return ret;
  }

  ret = openDecoder(&decCtx, &decoder, videoStream);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::getFrameFromImage::Could not open video decoder\n");
    goto closeInput;
  }

  ret = getEmptyAvFrame(pictureFrame, decCtx->pix_fmt, decCtx->width,
                        decCtx->height, 0, 0, 0, AVMEDIA_TYPE_VIDEO);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::getFrameFromImage::not able to get empty video frame\n");
    goto closeCodec;
  }

  pkt = av_packet_alloc();
  if (!pkt) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::not able to allocate packet\n");
    ret = AVERROR(ENOMEM);
    goto freeFrame;
  }

  while (1) {
    ret = av_read_frame(*ctx, pkt);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR,
             "utils::getFrameFromImage::Could not read any packet from input "
             "file...\n");
      goto freePacket;
    }

    if (pkt->stream_index == videoStream->index) {
      ret = avcodec_send_packet(decCtx, pkt);
      if (ret < 0) {
        av_log(
            NULL, AV_LOG_FATAL,
            "Fatal::InputSwitch::Could not send packet to video decoder....\n");
        exit(1);
      }

      while (ret >= 0 || ret == AVERROR(EAGAIN)) {
        ret = avcodec_receive_frame(decCtx, *pictureFrame);
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
          av_log(NULL, AV_LOG_FATAL,
                 "Fatal::InputSwitch::Not able to read picture frame\n");
          exit(1);
        }
        break;
      }
      break;
    }
    av_packet_unref(pkt);
  }

  av_packet_unref(pkt);
  closeCodec(&decCtx);
  closeInput(ctx);
  return 0;

freePacket:
  av_packet_unref(pkt);
freeFrame:
  av_frame_free(pictureFrame);
closeCodec:
  closeCodec(&decCtx);
closeInput:
  closeInput(ctx);

  return ret;
}

int getEmptyAvFrame(AVFrame **frame, int pixFmt, int width, int height,
                    enum AVSampleFormat smpFmt, int nbSamples,
                    uint64_t channelLayout, enum AVMediaType type) {
  int ret;

  *frame = av_frame_alloc();
  if (!*frame) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::getFrameFromImage::not able to allocate video frame\n");
    return AVERROR(ENOMEM);
  }

  if (type == AVMEDIA_TYPE_VIDEO) {
    (*frame)->format = pixFmt;
    (*frame)->width = width;
    (*frame)->height = height;
  } else if (type == AVMEDIA_TYPE_AUDIO) {
    (*frame)->format = smpFmt;
    (*frame)->nb_samples = nbSamples;
    (*frame)->channel_layout = channelLayout;
  }

  ret = av_frame_get_buffer(*frame, 0);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::getFrameFromImage::not able get buffer for video frame\n");
    av_frame_free(frame);
    return AVERROR(ENOMEM);
  }
}

int writeFrameToJpeg(AVFrame *frame, char *path) {
  AVCodecContext *encCtx = NULL;
  AVFormatContext *outFmt;
  AVCodec *encoder;
  AVStream *videoStream;
  AVPacket *pkt;

  int ret = 0;
  ret = openOutput(&outFmt, path, NULL, &videoStream, "mjpeg");
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::not able to open output file\n");
    return ret;
  }

  encoder = avcodec_find_encoder_by_name("mjpeg");
  if (!encoder) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::not able to find encoder\n");
    ret = AVERROR(EFAULT);
    goto closeOutput;
  }

  encCtx = avcodec_alloc_context3(encoder);
  if (!encCtx) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::not able to alloc codec context\n");
    ret = AVERROR(EFAULT);
    goto closeOutput;
  }

  encCtx->width = frame->width;
  encCtx->height = frame->height;
  encCtx->time_base.den = 1;
  encCtx->time_base.num = 1;
  encCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;

  ret = avcodec_open2(encCtx, encoder, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::not able to open encoder\n");
    goto freeCodexCtx;
  }

  ret = avcodec_parameters_from_context(videoStream->codecpar, encCtx);
  if (ret < 0) {
    av_log(
        NULL, AV_LOG_ERROR,
        "utils::writeFrameToFile::could not set video stream codec params\n");
    goto freeCodexCtx;
  }

  av_dump_format(outFmt, 0, "", 1);

  ret = avcodec_send_frame(encCtx, frame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::not able to send frame to encoder\n");
    goto freeCodexCtx;
  }

  // flush the encoder as we only want one image to be available
  ret = avcodec_send_frame(encCtx, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::not able to flush frame to encoder\n");
    goto freeCodexCtx;
  }

  pkt = av_packet_alloc();
  if (!pkt) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::not able to allocate packet\n");
    ret = AVERROR(ENOMEM);
    goto freeCodexCtx;
  }

  // while (ret >=0) {
  ret = avcodec_receive_packet(encCtx, pkt);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::could not get encoded packet\n");
    ret = AVERROR(ENOMEM);
    goto freePacket;
  }

  ret = avformat_write_header(outFmt, NULL);
  if (ret < 0) {
    ret = AVERROR(ENOENT);
    goto freePacket;
  }

  ret = av_write_frame(outFmt, pkt);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::write packet to output failed\n");
    goto freePacket;
  }

  ret = av_write_trailer(outFmt);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "utils::writeFrameToFile::could not write trailer\n");
    goto freePacket;
  }

freePacket:
  av_packet_unref(pkt);
  av_packet_free(&pkt);
freeCodexCtx:
  avcodec_free_context(&encCtx);
closeEncoder:
  closeCodec(&encCtx);
closeOutput:
  closeOutput(&outFmt);
  return ret;
}

// TODO: return an negative error if sscanf failed with wrong format
time_t isoTimeToEpoch(char *isoTimestamp) {
  // 2021-09-26T16:46:02.642+00:00
  struct tm tstamp = {0};
  struct tm *utc;
  time_t convTime;
  int year, month, day, hour, minute, second, millisec, offHour, offMinute;
  int utcHour, utcMin, utcSec, tmp;

  sscanf(isoTimestamp, "%d-%d-%dT%d:%d:%d.%d+%d:%d", &year, &month, &day, &hour,
         &minute, &second, &millisec, &offHour, &offMinute);

  tstamp.tm_hour = hour;
  tstamp.tm_min = minute;
  tstamp.tm_sec = second;
  tstamp.tm_year = year - 1900;
  tstamp.tm_mon = month - 1;
  tstamp.tm_mday = day;

  convTime = mktime(&tstamp);
  tmp = (offHour * 3600 + offMinute * 60) - tstamp.tm_gmtoff;
  utcHour = tmp / 3600;
  utcMin = (tmp - utcHour * 3600) / 60;
  utcSec = (tmp - utcHour * 3600 - utcMin * 60);

  tstamp.tm_hour -= utcHour;
  tstamp.tm_min -= utcMin;
  tstamp.tm_sec -= utcSec;
  convTime = mktime(&tstamp);
  return convTime;
}

int getNowAsIso(char **isoTimeString) {
  // 2021-09-26T16:46:02.642+00:00
  time_t rawTime;
  struct tm *tstamp;
  int hourOff, minOff, ret;
  time(&rawTime);
  tstamp = localtime(&rawTime);

  (*isoTimeString) = malloc(32 * sizeof(char));
  hourOff = tstamp->tm_gmtoff / 3600;
  minOff = (tstamp->tm_gmtoff - hourOff * 3600) / 60;

  ret = snprintf((*isoTimeString), 32 * sizeof(char),
                 "%04d-%02d-%02dT%02d:%02d:%02d.%03d+%02d:%02d",
                 tstamp->tm_year + 1900, tstamp->tm_mon + 1, tstamp->tm_mday,
                 tstamp->tm_hour, tstamp->tm_min, tstamp->tm_sec, 0, hourOff,
                 minOff);
  return ret;
}

int cleanDir(const char *path) {
  char cmd[1024];
  cmd[0] = 0;
  strcat(cmd, "exec rm -r ");
  strcat(cmd, path);
  strcat(cmd, "/* > /dev/null");
  printf("%s\n", cmd);
  return system(cmd);
}

int mkdirP(const char *path) {
  char cmd[1024];
  cmd[0] = 0;
  strcat(cmd, "exec mkdir -p ");
  strcat(cmd, path);
  strcat(cmd, " > /dev/null");
  printf("%s\n", cmd);
  return system(cmd);
}