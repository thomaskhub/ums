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
#ifndef __OUTPUT__
#define __OUTPUT__

#include <pthread.h>

#include "config.h"
#include "dash.h"
#include "filters.h"
#include "mux.h"
#include "utils.h"

typedef struct {
  const char* name;
  int64_t bitrate;
  char* url;   // output rtmp url. If NULL nor rtmp output enabled
  char* path;  // path to recodring file. if null recodring is disabled
  int gop;
  int inWidth;
  int inHeight;
  int outWidth;
  int outHeight;
  int format;
  enum AVMediaType type;

  /**
   * @brief Dash output stream index
   */
  int streamIdx;
  uint8_t filterEna;

  AVRational timebase;
  AVFormatContext* rtmpOutCtx;
  AVRational sampleAspectRatio;

  AVFormatContext* recCtx;
  AVStream *outVideoRec, *outAudioRec;
  VideoFilter vFilter;
  char filterDesc[128];

  // AVStream* outVideoStream;  // output video stream that can be used by
  // others
  AVStream *outVideoRtmp, *outAudioRtmp;
  AVCodecContext* videoEncCtx;
  AVCodec* videoEncoder;
  AVPacket* packet;
  AVFrame* encoderFrame;

  DashCtxT* dashCtx;

} OutputCtxT;

/**
 * @brief take the inptu audio frame and push it through
 * the encoder and then either into rtmp out, recodring out
 * or into an AVStream that can be used by other muxers
 *
 * @param frame
 */
void outputWriteAudioFrame(AVFrame* frame);
void outputWriteVideoFrame(OutputCtxT* data, AVFrame* frame);

int startOutput(OutputCtxT* data);

#endif