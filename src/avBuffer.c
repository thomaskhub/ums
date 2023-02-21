#include "avBuffer.h"

int addAudioSlice(AvBuffer *buf, AVFrame *frame) {
  AVFrame *slice;
  int ret;
  buf->nbSamples = frame->nb_samples;

  slice = av_frame_alloc();
  if (!frame) {
    av_log(NULL, AV_LOG_FATAL, "Could not allocate audio slice....\n");
    exit(1);
  }

  slice->format = frame->format;
  slice->nb_samples = frame->nb_samples;
  slice->channel_layout = frame->channel_layout;

  // av_log(NULL, AV_LOG_ERROR, "Thomas: aFormat %i\n", slice.)

  ret = av_frame_get_buffer(slice, 0);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "addAudioSlice::allocated frame buffer failed\n");
    exit(1);
    return ret;
  }

  ret = av_frame_copy(slice, frame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "addAudioSlice:: frame could not be copied %i\n", ret);
    return ret;
  }

  slice->pkt_dts = frame->pkt_dts;
  slice->pkt_duration = frame->pkt_duration;
  slice->pts = frame->pts;
  buf->buffer[buf->wrPtr] = slice;

  // point to the next element
  buf->wrPtr = (buf->wrPtr + 1) % FRAME_CNT;
  return 0;
}

int addVideoSlice(AvBuffer *buf, AVFrame *frame) {
  AVFrame *slice;
  int ret;
  buf->nbSamples = frame->nb_samples;

  slice = av_frame_alloc();
  if (!frame) {
    av_log(NULL, AV_LOG_FATAL, "Could not allocate video slice....\n");
    exit(1);
  }
  slice->format = frame->format;
  slice->width = frame->width;
  slice->height = frame->height;

  ret = av_frame_get_buffer(slice, 0);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "addAudioSlice::allocated frame buffer failed\n");
    exit(1);
    return ret;
  }

  ret = av_frame_copy(slice, frame);
  if (ret < 0) {
    return ret;
  }

  slice->pkt_dts = frame->pkt_dts;
  slice->pkt_duration = frame->pkt_duration;
  slice->pts = frame->pts;
  buf->buffer[buf->wrPtr] = slice;

  // point to the next element
  buf->wrPtr = (buf->wrPtr + 1) % FRAME_CNT;
  return 0;
}

int avBufferPush2(AvBuffer *buf, AVFrame *frame) {
  if (buf->buffer == NULL) {
    buf->buffer = malloc(FRAME_CNT * sizeof(AVFrame *));
    if (!buf->buffer) {
      av_log(NULL, AV_LOG_FATAL, "Could not allocate doubleBuffer array....\n");
      exit(1);
    }
  }

  if (avBufferFull2(buf)) {
    av_log(NULL, AV_LOG_WARNING, "avBufferPush2::%s buffer is full\n");
    return AVERROR(EAGAIN);
  }

  if (buf->type == AVMEDIA_TYPE_VIDEO) {
    return addVideoSlice(buf, frame);

  } else if (buf->type == AVMEDIA_TYPE_AUDIO) {
    return addAudioSlice(buf, frame);

  } else {
    av_log(NULL, AV_LOG_ERROR, "avBuffer::unsupported media type\n");
    exit(1);
  }
}

uint8_t avBufferFull2(AvBuffer *buf) {
  if (!buf->buffer) {
    return 0;
  }

  return (buf->wrPtr + 1) % FRAME_CNT == buf->rdPtr;
}

/**
 * @brief pull a frame from the buffer
 * its important that the application call av_frame_free
 * on the returned frame after its not needed so that memory
 * is released
 *
 * @param buf
 * @param frame
 * @return int
 */
int avBufferPull2(AvBuffer *buf, AVFrame **frame) {
  uint8_t empty = buf->wrPtr == buf->rdPtr;
  // av_log(NULL, AV_LOG_ERROR, "avBufferPull2::try to pull\n");
  if (empty || !buf->buffer) {
    // av_log(NULL, AV_LOG_ERROR, "avBufferPull2::video buffer is empty\n");
    return AVERROR(EAGAIN);
  }
  *frame = buf->buffer[buf->rdPtr];

  buf->rdPtr = (buf->rdPtr + 1) % FRAME_CNT;
  return 0;
}

int avBufferInit(AvBuffer *buf, uint32_t frameCount, enum AVPixelFormat pixFmt,
                 int width, int height, enum AVSampleFormat smpFmt,
                 int nbSamples, uint64_t channelLayout, enum AVMediaType type) {
  int i = 0;
  int ret;

  buf->type = type;
  buf->nbSamples = nbSamples;

  int tmp = frameCount * sizeof(AVFrame *);
  buf->buffer = malloc(frameCount * sizeof(AVFrame *));
  if (!buf->buffer) {
    av_log(NULL, AV_LOG_FATAL, "Could not allocate doubleBuffer array....\n");
    exit(1);
  }

  for (i = 0; i < frameCount; i++) {
    AVFrame *frame;
    frame = av_frame_alloc();
    if (!frame) {
      av_log(NULL, AV_LOG_FATAL, "Could not allocate frame....\n");
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
      av_log(NULL, AV_LOG_ERROR, "avBuffer::allocate frame buffer failed\n");
      exit(1);
      return ret;
    }

    buf->buffer[i] = frame;
  }

  buf->frameCount = frameCount;
  buf->wrPtr = 0;
  buf->rdPtr = 0;

  return 0;
}

void avBufferClear(AvBuffer *buf) {
  buf->wrPtr = 0;
  buf->rdPtr = 0;
}

uint8_t avBufferFull(AvBuffer *buf) {
  if (!buf->buffer) {
    return 0;
  }

  return (buf->wrPtr + 1) % buf->frameCount == buf->rdPtr;
}

int avBufferPush(AvBuffer *buf, AVFrame *frame) {
  int ret;
  uint8_t full = avBufferFull(buf);
  char *t = buf->type == AVMEDIA_TYPE_VIDEO ? "video" : "audio";

  // when we push the first data initalize it with the correct values
  // this way the first frame will decide the buffer frame settings
  if (!buf->buffer) {
    if (buf->type == AVMEDIA_TYPE_VIDEO) {
      ret = avBufferInit(buf, 3, frame->format, frame->width, frame->height, 0,
                         0, 0, AVMEDIA_TYPE_VIDEO);

    } else if (buf->type == AVMEDIA_TYPE_AUDIO) {
      ret = avBufferInit(buf, 3, 0, 0, 0, frame->format, frame->nb_samples,
                         frame->channel_layout, AVMEDIA_TYPE_AUDIO);
    }

    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "avBuffer::could not init %s buffer \n", t);
      exit(1);
    }
  }

  if (full) {
    av_log(NULL, AV_LOG_WARNING, "avBufferPush::%s buffer is full\n", t);
    return AVERROR(EAGAIN);
  }

  ret = av_frame_copy(buf->buffer[buf->wrPtr], frame);
  if (ret < 0) {
    return ret;
  }

  buf->buffer[buf->wrPtr]->pkt_dts = frame->pkt_dts;
  buf->buffer[buf->wrPtr]->pkt_duration = frame->pkt_duration;
  buf->buffer[buf->wrPtr]->pts = frame->pts;

  buf->wrPtr = (buf->wrPtr + 1) % buf->frameCount;
  return 0;
}

int avBufferPull(AvBuffer *buf, AVFrame **frame) {
  uint8_t empty = buf->wrPtr == buf->rdPtr;

  if (empty || !buf->buffer) {
    av_log(NULL, AV_LOG_DEBUG, "avBufferPull2::video buffer is empty\n");
    return AVERROR(EAGAIN);
  }
  *frame = buf->buffer[buf->rdPtr];
  buf->rdPtr = (buf->rdPtr + 1) % buf->frameCount;
  return 0;
}
