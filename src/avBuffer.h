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
  AVFrame **buffer;
  uint32_t frameCount;
  uint8_t wrPtr;
  uint8_t rdPtr;
  enum AVMediaType type;
  int nbSamples;
} AvBuffer;

/**
 * @brief initialize a simple ring buffer
 *
 * @param buf buffer object
 * @param frameCount number of frames the buffer should be able to store
 * @param pixFmt pixel format if its a video buffer
 * @param width image widht
 * @param height  image height
 * @param smpFmt sample format in case its a audio buffer
 * @param nbSamples number of samples per frame for audio frames
 * @param channelLayout audio channel layout
 * @param type either AV_MEDIA_AUDIO or AV_MEDIA_VIDEO
 * @return int
 */
int avBufferInit(AvBuffer *buf, uint32_t frameCount, enum AVPixelFormat pixFmt,
                 int width, int height, enum AVSampleFormat smpFmt,
                 int nbSamples, uint64_t channelLayout, enum AVMediaType type);

/**
 * @brief pull a frame from the buffer if available
 * return negative value if no frame is ready
 *
 * @param buf
 * @param frame
 * @return int
 */
int avBufferPull(AvBuffer *buf, AVFrame **frame);

/**
 * @brief push a frame into the ring buffer
 * return negative value if buffer is full
 *
 * @param buf
 * @param frame
 * @return int
 */
int avBufferPush(AvBuffer *buf, AVFrame *frame);

/**
 * @brief check if buffer is full
 * can be used to check if buffer is ready before pushing
 * and frame into it
 *
 * @param buf
 * @return uint8_t
 */
uint8_t avBufferFull(AvBuffer *buf);

#endif