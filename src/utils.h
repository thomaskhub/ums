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

#include <dirent.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
#include <libavutil/time.h>
#include <libgen.h>
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

/**
 * @brief check if a file exists
 * returns 0 if id does not exist
 * returns 1 if it exists.
 * @param path
 * @return uint8_t
 */
uint8_t fileExists(const char *path);

/**
 * @brief Calculate the moving average
 *
 * @param data
 * @param periods
 * @return int64_t
 */
double movingAverage(int64_t *data, uint8_t periods, int64_t value);
double movingAverageDouble(double *data, uint8_t periods, double value);
void rescaleVideoFrame(AVFrame *frame);

/**
 * @brief Set the Audio Delay in the filter for AV sync if needed
 *
 * @param graph
 * @param delay
 */
void setAudioDelay(AVFilterGraph *graph, uint32_t delay);

#endif