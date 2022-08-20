/**
* Copyright (C) 2022  The World
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
#ifndef __AUDIO__ENCODER__
#define __AUDIO__ENCODER__

#include <libavcodec/avcodec.h>

#include "config.h"
#include "mux.h"

typedef struct {
  /*desired bitrate of the output audio stream*/
  int64_t bitrate;

  /*encoded packet which can be forwarded to muxers*/
  AVPacket *packet;

  /*frame that contains the data to be encoded*/
  AVFrame *frame;

  // internal use
  AVCodecContext *encCtx;
  AVCodec *encoder;
  AVRational timebase;
} AudioEncCtx;

/**
 * @brief initialize audio encoder
 *
 * @param ctx
 * @return int
 */
int audioEncoderInit(AudioEncCtx *ctx);

/**
 * @brief convert one audio frame
 *
 * @param ctx
 * @return int
 */
int audioEncoderRun(AudioEncCtx *ctx);

#endif