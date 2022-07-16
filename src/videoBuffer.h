#ifndef __VIDEO_BUFFER__
#define __VIDEO_BUFFER__

#include <libavutil/frame.h>

typedef struct VideoBuffer {
  AVFrame **doubleBuffer;  // Array of AVFrame pointers
  uint8_t selectedBuffer;  // 0 first buffer, 1 second buffer
  uint32_t frameCount;
  uint32_t bufOffset[2];
} VideoBuffer;

typedef void (*PushHandler)(VideoBuffer *buf, AVFrame **buffer,
                            uint32_t frameCount, uint8_t bufId);

/**
 * For processing the filler video we need to implement a double
 * buffer which can store 1s second of video data in each buffer.
 *
 * The buffer is a list of pointers to AVFrame objects. In the
 * initVideoBuffer function we will create an array of pointers
 * to AVFrames with allocated memory. We copy the input data into
 * the buffers so that input can release frame memory if needed.
 */
int videoBufferInit(VideoBuffer *buf, uint32_t frameCount,
                    enum AVPixelFormat pixFmt, int width, int height);

/**
 * After the buffer is initialized we need a function with which we can
 * copy input data into the double buffer. For this push frame will be called.
 *
 * pushFrame will need to tell the caller when double buffer is ready for
 * processing for this is will call a callback handler passing the array of
 * pointers The callback can then take the array and push it into the encoders.
 * The memory passed to the callback handler is not released or changed until
 * the callback handler has acknowledged processing.
 *
 * If no buffer is available (overrun) we drop the next 1 seconds of data.
 * This should actually never happens and will be done only for debugging.
 * If this happens we would need to check the design.
 *
 * Its important that Frame has the same width, height and pixel format then
 * defined in the buffer init function.
 *
 * If we do not want that the module pushes data we can set the handler to NULL
 * If handler is null we need to actively check and pull data from the buffer
 */
int videoBufferPush(VideoBuffer *buf, AVFrame *frame, PushHandler handler);

/**
 * Once the callback handler has processed all video data passed to it it needs
 * to call the bufferDone function passing the buffer id which has been
 * processed Buffer id can be 0 or 1 and is passed to the callback handler
 */
void videoBufferDone(VideoBuffer *buf, uint8_t bufId);

/**
 * We also need to have the possibility to reset the buffer
 * e.g. if rtmp input goes down to not mix old and new data
 */
void videoBufferReset(VideoBuffer *buf);

/**
 * In certain situations we would like to pull data from the output
 * instead if it being pushed to some other input.
 *
 * videoBufferPull will return the buffer start pointer and the number of
 * elements that can be processed.
 *
 * After processing is done videoBufferDone must be called as well.
 * If no buffer is ready it will return EAGAIN.
 *
 */
int videoBufferPull(VideoBuffer *buf, uint32_t *off, uint32_t *len,
                    uint32_t *bufId);

#endif