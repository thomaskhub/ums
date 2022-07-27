/**
* Copyright (C) 2022  Thomas Kinder
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.

* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/
#ifndef __VIDEO_BUFFER__
#define __VIDEO_BUFFER__

#include <libavutil/frame.h>

typedef struct AvBuffer {
  AVFrame **buffer;        // Array of AVFrame pointers
  uint8_t selectedBuffer;  // 0 first buffer, 1 second buffer
  uint32_t frameCount;
  uint8_t wrPtr;
  uint8_t rdPtr;
  uint32_t off;
  uint32_t bufOffset[4];
  enum AVMediaType type;
  int nbSamples;
} AvBuffer;

typedef void (*PushHandler)(AvBuffer *buf, AVFrame **buffer,
                            uint32_t frameCount, uint8_t bufId);

/**
 * For processing the filler video we need to implement a double
 * buffer which can store 1s second of video data in each buffer.
 *
 * The buffer is a list of pointers to AVFrame objects. In the
 * initVideoBuffer function we will create an array of pointers
 * to AVFrames with allocated memory. We copy the input data into
 * the buffers so that input can release frame memory if needed.
 *
 * Audio and Video buffer will work a little differently internally.
 * For audio the we are allocation 44 frames each of which will hold
 * 1024 in total we could save 45,056 samples. As we a running at a sample rate
 * of 44100k we will have 43 frames with 1024 samples and the last frame will
 * only have 68 samples. This is needed to store exactly one second in the
 * buffer. But the last incoming audio frame will also have 1024, so 1024-68
 * frames need to be stored at the beginning of the next buffer
 */
int avBufferInit(AvBuffer *buf, uint32_t frameCount, enum AVPixelFormat pixFmt,
                 int width, int height, enum AVSampleFormat smpFmt,
                 int nbSamples, uint64_t channelLayout, enum AVMediaType type);

int avBufferPull2(AvBuffer *buf, uint32_t *off, uint32_t *len);
int avBufferPush2(AvBuffer *buf, AVFrame *frame);
int avBufferDone2(AvBuffer *buf);
uint8_t avBufferFull(AvBuffer *buf);
/**
 * After the buffer is initialized we need a function with which we can
 * copy input data into the double buffer. For this push frame will be
 * called.
 *
 * pushFrame will need to tell the caller when double buffer is ready for
 * processing for this is will call a callback handler passing the array of
 * pointers The callback can then take the array and push it into the
 * encoders. The memory passed to the callback handler is not released or
 * changed until the callback handler has acknowledged processing.
 *
 * If no buffer is available (overrun) we drop the next 1 seconds of data.
 * This should actually never happens and will be done only for debugging.
 * If this happens we would need to check the design.
 *
 * Its important that Frame has the same width, height and pixel format then
 * defined in the buffer init function.
 *
 * If we do not want that the module pushes data we can set the handler to
 * NULL If handler is null we need to actively check and pull data from the
 * buffer
 */
int avBufferPush(AvBuffer *buf, AVFrame *frame, PushHandler handler);

/**
 * Once the callback handler has processed all video data passed to it it needs
 * to call the bufferDone function passing the buffer id which has been
 * processed Buffer id can be 0 or 1 and is passed to the callback handler
 */
void avBufferDone(AvBuffer *buf, uint8_t bufId);

/**
 * We also need to have the possibility to reset the buffer
 * e.g. if rtmp input goes down to not mix old and new data
 */
void avBufferReset(AvBuffer *buf);

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
int avBufferPull(AvBuffer *buf, uint32_t *off, uint32_t *len, uint32_t *bufId);

void avBufferClose(AvBuffer *buf);

#endif