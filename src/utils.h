#ifndef __UTILS__
#define __UTILS__

#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
#include <libavutil/time.h>
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
#endif