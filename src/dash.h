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
#ifndef __DASH__
#define __DASH__

#include "config.h"
#include "filters.h"
#include "mux.h"
#include "utils.h"

typedef struct DashCtxT {
  /**
   * array of video streams used for dash output
   */
  AVStream **dashStreams;
  /** dash output audio stream (single audio stream only)*/
  AVStream *dashASteam;
  uint8_t streamLen;
  AVFormatContext *dashOutCtx;
  AVRational timebase;

  /**path to the manifest file (index.mpd)*/
  char *dashIndexPath;

} DashCtxT;

/**
 * @brief Start the dash output muxer
 *
 * @param data
 * @param encoderCtx
 * @return int
 */
int startDash(DashCtxT *data, AVCodecContext **encoderCtx,
              AVCodecContext *aCodecCtx);

/**
 * @brief write a packet to the dash mux
 *
 * @param data
 * @param packet
 */
void dashWritePacket(DashCtxT *data, AVPacket *packet);

/**
 * @brief release all resources allocated for the dash output
 *
 * @param data
 */
void dashClose(DashCtxT *data);

#endif