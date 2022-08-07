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

#include "config.h"

typedef struct AvBuffer {
  AVFrame **buffer;  // Array of AVFrame pointers
  // uint8_t selectedBuffer;  // 0 first buffer, 1 second buffer
  uint32_t frameCount;
  uint8_t wrPtr;
  uint8_t rdPtr;
  // uint32_t off;
  // uint32_t bufOffset[4];
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

int avBufferPull2(AvBuffer *buf, AVFrame **frame);
int avBufferPush2(AvBuffer *buf, AVFrame *frame);
uint8_t avBufferFull(AvBuffer *buf);

#endif