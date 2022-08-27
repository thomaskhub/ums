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
#ifndef __UTILS__
#define __UTILS__

#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
#include <libavutil/time.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "mux.h"

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

int getEmptyAvFrame(AVFrame **frame, int pixFmt, int width, int height,
                    enum AVSampleFormat smpFmt, int nbSamples,
                    uint64_t channelLayout, enum AVMediaType type);

/**
 * @brief take a YUV420P video frame and store it into a jpeg
 * file.
 *
 * @param frame
 * @param path
 * @return int
 */
int writeFrameToJpeg(AVFrame *frame, char *path);

/**
 * @brief convert ISO string time to epoch time
 *
 * @param isoTimestamp
 * @return time_t
 */
time_t isoTimeToEpoch(char *isoTimestamp);

/**
 * @brief get the current time as iso formated string
 *
 * @param isoTimeString
 * @return int
 */
int getNowAsIso(char **isoTimeString);

/**
 * @brief can be used to clean up dash dir. All
 * files in dash dir will be removed so be carefull what
 * directory is passed
 *
 * @param path
 */
void cleanDashDir(const char *path);

/**
 * @brief Create a directory if non existing and
 * we have permission to write to the specified
 * path.
 *
 * @param path
 * @return int
 */
int mkdirP(const char *path);
#endif