#ifndef __FITLERS__
#define __FITLERS__

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>

typedef struct VideoFilter {
  AVFilterGraph *fGraph;
  AVFilterContext *sinkCtx;
  AVFilterContext *srcCtx;
  AVFilterInOut *in;
  AVFilterInOut *out;
} VideoFilter;

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
int initVideoFilter(VideoFilter *ctx, const char *fDesc, int width, int height,
                    int pixFmt, AVRational timebase, AVRational aspectRatio);

/**
 * @brief push a frame into the video filter
 *
 * @param ctx
 * @param frame
 * @return int >=0 on success, negaive AVERROR code otherwise
 */
int videoFilterPush(VideoFilter *ctx, AVFrame *frame);

/**
 * @brief pull a video frame from the filter output
 *
 * @param ctx
 * @param frame
 * @return int >=0 on success, negaive AVERROR code otherwise
 */
int videoFilterPull(VideoFilter *ctx, AVFrame **frame);

void videoFilterFree(VideoFilter *ctx);
#endif