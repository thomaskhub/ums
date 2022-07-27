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
#ifndef __FITLERS__
#define __FITLERS__

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>

typedef struct AvFilter {
  AVFilterGraph *fGraph;
  AVFilterContext *sinkCtx;
  AVFilterContext *srcCtx;
  AVFilterInOut *in;
  AVFilterInOut *out;
} AvFilter;

/**
 * @brief initialize a video filter
 *
 * init a video filter chain with input and output buffers.
 * the filter description string can be anything ffmpeg supports.
 *
 * @param ctx video filter context
 * @param fDesc filter description string
 * @param width the width of the input video frame
 * @param height  the height of the input video frame
 * @param pixFmt the pixel format of input video frame
 * @param timebase the timebase of the input video
 * @param aspectRatio pixel aspect ratio of vieo frame
 * @return int >= 0 in case of success, a negative AVERROR otherwise
 */
int initAvFilter(AvFilter *ctx, const char *fDesc, int width, int height,
                 int pixFmt, AVRational timebase, AVRational aspectRatio,
                 uint64_t smpFmt, int sampleRate, uint64_t chanLayout,
                 enum AVMediaType type);

/**
 * @brief push a frame into the video filter
 *
 * @param ctx
 * @param frame
 * @return int >=0 on success, negaive AVERROR code otherwise
 */
int avFilterPush(AvFilter *ctx, AVFrame *frame);

/**
 * @brief pull a video frame from the filter output
 *
 * @param ctx
 * @param frame
 * @return int >=0 on success, negaive AVERROR code otherwise
 */
int avFilterPull(AvFilter *ctx, AVFrame **frame);

/**
 * @brief Release all resources allocated for the video filter
 *
 * @param ctx
 */
void videoFilterFree(AvFilter *ctx);
#endif