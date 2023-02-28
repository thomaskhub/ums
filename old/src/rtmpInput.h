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
#ifndef __RTMP_INPUT__
#define __RTMP_INPUT__

#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <pthread.h>

#include "avBuffer.h"
#include "config.h"
#include "filters.h"
#include "mux.h"
#include "utils.h"
typedef struct {
  int audioSampleRate;
  uint64_t audioChannelLayout;
  int audioChannels;
  int audioSampleFormat;
  AVRational audioTimeBase;
} RtmpInputInfo;

AvBuffer rtmpInVBuffer;
AvBuffer rtmpInABuffer;

typedef struct {
  char *url;
  void (*audioCallback)(AVFrame *frame, AVRational framerate);
  void (*videoCallback)(AVFrame *frame, AVRational framerate);
} RtmpWorkerData;

void rtmpInputStart(char *url);
void rtmpInputStop();
void rtmpInputJoin();
AVRational rtmpGetVideoTimebase();
AVRational rtmpGetAudioTimebase();

/**
 * @brief function to get the rtmp input status
 * 1 means its running, 0 means its not running
 *
 * @return int
 */
int rtmpIsRunning();

#endif