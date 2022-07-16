#include "videoBuffer.h"

int videoBufferInit(VideoBuffer *buf, uint32_t frameCount,
                    enum AVPixelFormat pixFmt, int width, int height) {
  int i = 0;
  int ret;

  buf->doubleBuffer = malloc(2 * frameCount * sizeof(AVFrame *));
  if (!buf->doubleBuffer) {
    printf("Could not allocate doubleBuffer array....\n");
    exit(1);
  }

  for (i = 0; i < 2 * frameCount; i++) {
    AVFrame *frame;
    frame = av_frame_alloc();
    if (!frame) {
      printf("Could not allocate frame....\n");
      exit(1);
    }

    frame->format = pixFmt;
    frame->width = width;
    frame->height = height;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
      printf("Could not allocate frame buffer....\n");
      exit(1);
      return ret;
    }

    buf->doubleBuffer[i] = frame;
    buf->frameCount = frameCount;
  }

  buf->selectedBuffer = 0;
  buf->bufOffset[0] = 0;
  buf->bufOffset[1] = 0;
  return 0;
}

int videoBufferPush(VideoBuffer *buf, AVFrame *frame, PushHandler handler) {
  uint32_t offset = buf->bufOffset[buf->selectedBuffer];
  AVFrame **handlerBuffer;
  int ret;

  if (buf->bufOffset[0] >= buf->frameCount &&
      buf->bufOffset[1] >= buf->frameCount) {
    printf("Warning!!!! We lost data both double buffers are full...\n");
    return AVERROR(EBUSY);
  }

  if (offset < buf->frameCount) {
    // copy frames until buffer is full
    ret = av_frame_copy(
        buf->doubleBuffer[offset + buf->frameCount * buf->selectedBuffer],
        frame);

    if (ret < 0) {
      return ret;
    }

    buf->bufOffset[buf->selectedBuffer]++;

  } else {
    // buffer is full so move to the next buffer
    // we can also then push the current buffer to the handler

    if (handler) {
      handlerBuffer =
          &(buf->doubleBuffer[buf->selectedBuffer * buf->frameCount]);

      handler(buf, handlerBuffer, buf->frameCount, buf->selectedBuffer);
    }

    buf->selectedBuffer = buf->selectedBuffer == 1 ? 0 : 1;

    offset = buf->bufOffset[buf->selectedBuffer] +
             buf->frameCount * buf->selectedBuffer;

    ret = av_frame_copy(buf->doubleBuffer[offset], frame);
    if (ret < 0) {
      return ret;
    }

    buf->bufOffset[buf->selectedBuffer]++;
  }

  return 0;
}

void videoBufferDone(VideoBuffer *buf, uint8_t bufId) {
  buf->bufOffset[bufId] = 0;
}

void videoBufferReset(VideoBuffer *buf) {
  buf->selectedBuffer = 0;
  buf->bufOffset[0] = 0;
  buf->bufOffset[1] = 0;
}

int videoBufferPull(VideoBuffer *buf, uint32_t *off, uint32_t *len,
                    uint32_t *bufId) {
  int page0Rdy = buf->bufOffset[0] == buf->frameCount;
  int page1Rdy = buf->bufOffset[1] == buf->frameCount;

  if (!page0Rdy && !page1Rdy) {
    return AVERROR(EAGAIN);
  }

  if (page0Rdy && !page1Rdy) {
    *off = 0;
    *len = buf->frameCount;
    *bufId = 0;
    return 0;
  }

  if (!page0Rdy && page1Rdy) {
    *off = buf->frameCount;
    *len = buf->frameCount;
    *bufId = 1;
    return 0;
  }

  if (page0Rdy && page1Rdy) {
    // this should not happen because it means overrun, but just in case
    // it happens handle it here.
    printf("Warning: videoBuffer: page 0 and page 1 are both full!!!\n");

    if (buf->selectedBuffer == 0) {
      // latest data is in buffer 0 so first push buffer 1 data.
      *off = buf->frameCount;
      *len = buf->frameCount;
      *bufId = 1;
      return 0;
    } else {
      *off = 0;
      *len = buf->frameCount;
      *bufId = 0;
      return 0;
    }
  }
  return AVERROR(AVERROR_UNKNOWN);
}