#include "avBuffer.h"

int avBufferInit(AvBuffer *buf, uint32_t frameCount, enum AVPixelFormat pixFmt,
                 int width, int height, enum AVSampleFormat smpFmt,
                 int nbSamples, uint64_t channelLayout, enum AVMediaType type) {
  int i = 0;
  int ret;

  buf->type = type;
  buf->nbSamples = nbSamples;

  if (type == AVMEDIA_TYPE_AUDIO) {
    frameCount = 1;  // 1 Frame with nbSamples wich will hold 1 second of audio
  }

  buf->buffer = malloc(4 * frameCount * sizeof(AVFrame *));
  if (!buf->buffer) {
    printf("Could not allocate doubleBuffer array....\n");
    exit(1);
  }

  for (i = 0; i < 4 * frameCount; i++) {
    AVFrame *frame;
    frame = av_frame_alloc();
    if (!frame) {
      printf("Could not allocate frame....\n");
      exit(1);
    }

    if (type == AVMEDIA_TYPE_VIDEO) {
      frame->format = pixFmt;
      frame->width = width;
      frame->height = height;
    } else if (type == AVMEDIA_TYPE_AUDIO) {
      frame->format = smpFmt;
      frame->nb_samples = nbSamples;
      frame->channel_layout = channelLayout;
    } else {
      av_log(NULL, AV_LOG_ERROR, "avBuffer::unsupported media type\n");
      exit(1);
    }

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
      printf("Could not allocate frame buffer....\n");
      exit(1);
      return ret;
    }

    buf->buffer[i] = frame;
    buf->frameCount = frameCount;
    buf->bufOffset[i] = 0;
  }

  buf->selectedBuffer = 0;
  buf->wrPtr = 0;
  buf->rdPtr = 0;
  buf->off = 0;
  return 0;
}

uint8_t avBufferFull(AvBuffer *buf) {
  return (buf->wrPtr + 1) % 4 == buf->rdPtr;
}

int avBufferPush2(AvBuffer *buf, AVFrame *frame) {
  uint8_t tmp = buf->wrPtr + 1;
  uint8_t full = (buf->wrPtr + 1) % 4 == buf->rdPtr;

  if (buf->type == AVMEDIA_TYPE_VIDEO) {
    int ret;

    if (full) {
      av_log(NULL, AV_LOG_WARNING, "avBufferPush2::video buffer is full\n");
      return AVERROR(EAGAIN);
    }

    uint8_t pageNo = buf->frameCount * buf->wrPtr;

    ret = av_frame_copy(buf->buffer[buf->off + pageNo], frame);
    if (ret < 0) {
      return ret;
    }

    buf->off++;
    if (buf->off >= buf->frameCount) {
      // we just pushed the last frame so switch to the next buffer and preare
      // it
      buf->wrPtr = (buf->wrPtr + 1) % 4;
      buf->off = 0;
    }

  } else if (buf->type == AVMEDIA_TYPE_AUDIO) {
    uint32_t freeSampleCnt = buf->nbSamples - buf->off;

    if (freeSampleCnt >= frame->nb_samples) {
      // we have enough space in the buffer to copy
      AVFrame *tmpFrame = buf->buffer[buf->wrPtr];
      memcpy(&(tmpFrame->data[0][buf->off]), &frame->data[0],
             frame->nb_samples);

      buf->off += frame->nb_samples;
    } else {
      // Full frame does not fit so we split it to complete current buffer
      // and write the next into the next buffer page, so first check if we
      // next buffer is available before accepting the frame
      if (full) {
        av_log(NULL, AV_LOG_ERROR, "avBufferPush2::audio buffer full\n");
        return AVERROR(EAGAIN);
      }

      uint32_t remaining = buf->nbSamples - buf->off;
      AVFrame *tmpFrame = buf->buffer[buf->wrPtr];

      // fill the last few samples into the current buffer
      memcpy(&(tmpFrame->data[0][buf->off]), &frame->data[0], remaining);

      // then switch and push the remaining frames in the next buffer
      buf->off += remaining;
      buf->wrPtr = (buf->wrPtr + 1) % 4;

      memcpy(&(tmpFrame->data[0][buf->off]),
             &(frame->data[0][frame->nb_samples - remaining]), remaining);
      buf->off = remaining;
    }
  }
}

int avBufferPull2(AvBuffer *buf, uint32_t *off, uint32_t *len) {
  uint8_t empty = buf->wrPtr == buf->rdPtr;
  // uint8_t empty = buf->wrPtr == 0 && buf->rdPtr == 0;
  if (empty) {
    av_log(NULL, AV_LOG_DEBUG, "avBufferPull2::video buffer is empty\n");
    return AVERROR(EAGAIN);
  }

  uint8_t pageNo = buf->frameCount * buf->rdPtr;
  *off = pageNo;
  *len = buf->type == AVMEDIA_TYPE_VIDEO ? buf->frameCount : 1;
  return 0;
}

int avBufferDone2(AvBuffer *buf) {
  // printf("Debug::\n");
  int i;
  uint8_t pageNo = buf->frameCount * buf->rdPtr;
  for (i = 0; i < buf->frameCount; i++) {
    // av_frame_unref(buf->buffer[buf->off + pageNo]);
  }
  buf->rdPtr = (buf->rdPtr + 1) % 4;
}

int avBufferPush(AvBuffer *buf, AVFrame *frame, PushHandler handler) {
  uint32_t offset = buf->bufOffset[buf->selectedBuffer];
  AVFrame **handlerBuffer;
  int ret;

  if (buf->type == AVMEDIA_TYPE_VIDEO) {
    if (buf->bufOffset[0] >= buf->frameCount &&
        buf->bufOffset[1] >= buf->frameCount &&
        buf->bufOffset[2] >= buf->frameCount) {
      av_log(NULL, AV_LOG_WARNING, "avBuffer::video double buffer overflow!\n");

      return AVERROR(EBUSY);
    }
  } else if (buf->type == AVMEDIA_TYPE_AUDIO) {
    if (buf->bufOffset[0] >= buf->nbSamples &&
        buf->bufOffset[1] >= buf->nbSamples &&
        buf->bufOffset[2] >= buf->nbSamples) {
      av_log(NULL, AV_LOG_WARNING,
             "avBuffer:: audio double buffer overflow!\n");

      return AVERROR(EBUSY);
    }
  } else {
    av_log(NULL, AV_LOG_WARNING, "avBuffer:: unsupported media type pushed!\n");
    return AVERROR(EBUSY);
  }

  if (buf->type == AVMEDIA_TYPE_VIDEO) {
    if (offset < buf->frameCount) {
      // copy frames until buffer is full
      ret = av_frame_copy(
          buf->buffer[offset + buf->frameCount * buf->selectedBuffer], frame);

      if (ret < 0) {
        return ret;
      }

      buf->bufOffset[buf->selectedBuffer]++;

    } else {
      // buffer is full so move to the next buffer
      // we can also then push the current buffer to the handler

      if (handler) {
        handlerBuffer = &(buf->buffer[buf->selectedBuffer * buf->frameCount]);
        handler(buf, handlerBuffer, buf->frameCount, buf->selectedBuffer);
      }

      buf->selectedBuffer =
          buf->selectedBuffer == 2 ? 0 : buf->selectedBuffer + 1;

      offset = buf->bufOffset[buf->selectedBuffer] +
               buf->frameCount * buf->selectedBuffer;

      ret = av_frame_copy(buf->buffer[offset], frame);
      if (ret < 0) {
        return ret;
      }

      buf->bufOffset[buf->selectedBuffer]++;
    }
  } else if (buf->type == AVMEDIA_TYPE_AUDIO) {
    uint32_t offset = buf->bufOffset[buf->selectedBuffer];
    uint32_t freeSampleCnt = buf->nbSamples - offset;

    if (freeSampleCnt >= frame->nb_samples) {
      // we have enough space in the double buffer to copy
      AVFrame *tmpFrame = buf->buffer[buf->selectedBuffer];
      memcpy(&(tmpFrame->data[0][offset]), &frame->data[0], frame->nb_samples);
      offset += frame->nb_samples;
      buf->bufOffset[buf->selectedBuffer] = offset;
    } else {
      // Full frame does not fit so we split it to complete current buffer
      // and write the next into the next buffer page
      uint32_t remaining = buf->nbSamples - offset;
      AVFrame *tmpFrame = buf->buffer[buf->selectedBuffer];
      memcpy(&(tmpFrame->data[0][offset]), &frame->data[0], remaining);
      buf->bufOffset[buf->selectedBuffer] += remaining;

      // current buffer is full so switch to the next buffer
      buf->selectedBuffer =
          buf->selectedBuffer == 2 ? 0 : buf->selectedBuffer + 1;

      // if (buf->bufOffset[buf->selectedBuffer] >= buf->nbSamples) {
      //   av_log(NULL, AV_LOG_FATAL,
      //          "avBuffer::audio double buffer is full we lost data");
      //   exit(1);
      //   // if we lost data AV sync will not be able to recover so we
      //   // stop the application....
      // }

      tmpFrame = buf->buffer[buf->selectedBuffer];
      offset = buf->bufOffset[buf->selectedBuffer];
      memcpy(&(tmpFrame->data[0][offset]),
             &(frame->data[0][frame->nb_samples - remaining]), remaining);
      buf->bufOffset[buf->selectedBuffer] = remaining;
    }
  }
  return 0;
}

void avBufferDone(AvBuffer *buf, uint8_t bufId) { buf->bufOffset[bufId] = 0; }

void avBufferReset(AvBuffer *buf) {
  buf->selectedBuffer = 0;
  buf->bufOffset[0] = 0;
  buf->bufOffset[1] = 0;
  buf->bufOffset[2] = 0;
}

int avBufferPull(AvBuffer *buf, uint32_t *off, uint32_t *len, uint32_t *bufId) {
  int page0Rdy = buf->type == AVMEDIA_TYPE_VIDEO
                     ? buf->bufOffset[0] == buf->frameCount
                     : buf->bufOffset[0] == buf->nbSamples;

  int page1Rdy = buf->type == AVMEDIA_TYPE_VIDEO
                     ? buf->bufOffset[1] == buf->frameCount
                     : buf->bufOffset[1] == buf->nbSamples;

  int page2Rdy = buf->type == AVMEDIA_TYPE_VIDEO
                     ? buf->bufOffset[2] == buf->frameCount
                     : buf->bufOffset[2] == buf->nbSamples;

  // if (!page0Rdy && !page1Rdy) {
  //   return AVERROR(EAGAIN);
  // }

  int checkVal = (page0Rdy << 2 | page1Rdy << 1 | page2Rdy);
  //  000 buffer empty
  //  001 page 2 ready
  //  010 page 1 ready
  //  011 page 1,2 ready to read page 2 first
  //  100 page 0 ready
  //  101 page 0 read and page 2 ready --> take page 3 first
  //  110 page 0 ready page 1 ready --> read page 0
  //  111 page0,1,2 ready read page 0

  if (checkVal == 0b000) {
    return AVERROR(EAGAIN);
  }

  if (checkVal == 0b111) {
    av_log(NULL, AV_LOG_WARNING, "avBuffer:: video buffer overflow!\n");
    return AVERROR(ENOMEM);
  }

  if (checkVal & 0b100 && checkVal != 0b101) {
    // if first page is ready take it, if first and 2nd ready take 2nd as it
    // is older data
    *off = 0;
    *len = buf->type == AVMEDIA_TYPE_VIDEO ? buf->frameCount : 1;
    *bufId = 0;
    return 0;
  }

  if (checkVal & 0b010) {
    *off = buf->frameCount;
    *len = buf->type == AVMEDIA_TYPE_VIDEO ? buf->frameCount : 1;
    *bufId = 1;
    return 0;
  }

  if (checkVal & 0b001) {
    *off = buf->frameCount;
    *len = buf->type == AVMEDIA_TYPE_VIDEO ? buf->frameCount : 1;
    *bufId = 2;
    return 0;
  }

  // switch (checkVal) {
  //   case 0b100:
  //     // only page0 is ready, so read page 0
  //     break;

  //   case 0b110:
  //     // only page0 and page1 is ready
  //     break;

  //   case 0b110:
  //     // only page0 and page1 is ready
  //     break;

  //   case 0b111:
  //     // only page0 and page1 and page2 is ready
  //     break;

  //   default:
  //     break;
  // }

  // if (page0Rdy && !page1Rdy && !page2Rdy) {
  //   *off = 0;
  //   *len = buf->type == AVMEDIA_TYPE_VIDEO ? buf->frameCount : 1;
  //   *bufId = 0;
  //   return 0;
  // }

  // if (!page0Rdy && page1Rdy) {
  //   *off = buf->frameCount;
  //   *len = buf->type == AVMEDIA_TYPE_VIDEO ? buf->frameCount : 1;
  //   *bufId = 1;
  //   return 0;
  // }

  // if (page0Rdy && page1Rdy) {
  //   // this should not happen because it means overrun, but just in case
  //   // it happens handle it here.
  //   av_log(NULL, AV_LOG_WARNING, "avBuffer:: video double buffer
  //   overflow!\n");

  //   // exit(1); //TODO:enable this again

  //   // if (buf->selectedBuffer == 0) {
  //   //   // latest data is in buffer 0 so first push buffer 1 data.
  //   //   *off = buf->frameCount;
  //   //   *len = buf->frameCount;
  //   //   *bufId = 1;
  //   //   return 0;
  //   // } else {
  //   //   *off = 0;
  //   //   *len = buf->frameCount;
  //   //   *bufId = 0;
  //   //   return 0;
  //   // }
  // }
  return AVERROR(AVERROR_UNKNOWN);
}

void avBufferClose(AvBuffer *buf) {
  int i;
  for (i = 0; i < 2 * buf->frameCount; i++) {
    av_frame_free(&buf->buffer[i]);
  }
}