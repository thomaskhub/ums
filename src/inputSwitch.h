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
#ifndef __INPUT_SWITCH__
#define __INPUT_SWITCH__

#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <pthread.h>

#include "config.h"
#include "filters.h"
#include "mux.h"
#include "rtmpInput.h"
#include "utils.h"
#include "videoBuffer.h"

typedef void (*PushVideo)(AVFrame *frame);
typedef void (*PushAudio)(AVFrame *frame);
typedef int (*RtmpIsActive)(void);

typedef struct Filler {
  AVFrame *vPreFiller;
  AVFrame *vSessionFiller;
  AVFrame *vPostFiller;
  AVFrame *aPreFiller;
  AVFrame *aSessionFiller;
  AVFrame *aPostFiller;
  time_t streamStart;
  time_t sessionStart;
  time_t sessionEnd;
} Filler;

pthread_t inputSwitchThread;

/**
 * @brief initialize the input switch module
 *
 * @param _vPush callback to pass on video frames
 * @param _aPush callback to pass on audio frames
 * @param _checkRtmp callback to check if rtmp is running or not
 * @param preFiller path to the prefiller image file
 * @param sessionFiller path to the sessionFiller image file
 * @param postFiller path to the post session filler image file
 * @return int 0 on success, AVERROR code otherwise
 */
int inputSwitchInit(PushVideo _vPush, PushAudio _aPush, RtmpIsActive _checkRtmp,
                    char *preFiller, char *sessionFiller, char *postFiller,
                    char *streamStart, char *sessionStart, char *sessionEnd);

/**
 * @brief returns a frame derived from a jpeg file and normalized to
 * VIDEO_WIDHT, VIDEO_HEIGHT, VIDEO_PIX_FMT values.
 *
 * This frame can be used as a filler video. If the function returned
 * successfull its the users responsibility to call av_frame_free once the
 * frame is not needed any longer.
 *
 * @param path
 * @param frame
 * @return int 0 on success, AVERROR code otherwise
 */
int prepareVideoFiller(char *path, AVFrame **frame);
#endif