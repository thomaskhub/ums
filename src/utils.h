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
#ifndef __UTILS__
#define __UTILS__

#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
#include <libavutil/time.h>
#include <string.h>
#include <time.h>

#include "mux.h"

void initPTS();
int64_t getPTS();
int64_t getPTScaled(AVRational timebase);

/**
 * @brief Get the Frame From Image object
 *
 * @param ctx
 * @param path
 * @param pictureFrame a pointer to a AVFrame pointer for which we will
 * allocate memory. User must free ths with av_frame_free if no longer needed
 * @return int
 */
int getFrameFromImage(AVFormatContext **ctx, char *path,
                      AVFrame **pictureFrame);

int getEmptyVideoFrame(AVFrame **frame, int pixFmt, int width, int height);

/**
 * @brief take a YUV420P video frame and store it into a jpeg
 * file.
 *
 * @param frame
 * @param path
 * @return int
 */
int writeFrameToJpeg(AVFrame *frame, char *path);

time_t isoTimeToEpoch(char *isoTimestamp);
int getNowAsIso(char **isoTimeString);
int cleanDir(const char *path);
int mkdirP(const char *path);
#endif