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
#include <libavutil/log.h>

#include "config.h"
#include "inputSwitch.h"
#include "output.h"
#include "rtmpInput.h"
#include "utils.h"

// TODO: these defines need to be passed to the main fucntion
//       via command line argumens and should not be statically
//       compiled within the code
#define RTMP_IN_URL "rtmp://localhost/live/input"
#define RTMP_OUT_URL "rtmp://localhost/live/output"
#define RECORD_PATH "/tmp/testing.ts"
#define RECORD_PATH2 "/tmp/testing2.ts"
#define preFiller "/home/thomas/Pictures/a.jpg"
#define sessionFiller "/home/thomas/Pictures/a.jpg"
#define postFiller "/home/thomas/Pictures/a.jpg"

// Output configuration
OutputCtxT vOutCfg[] = {{
    .name = "main",
    .bitrate = 2880000,
    .url = RTMP_OUT_URL,
    .path = RECORD_PATH,
    .gop = 100,
    .inWidth = VIDEO_WIDTH,
    .inHeight = VIDEO_HEIGHT,
    .outWidth = VIDEO_WIDTH,
    .outHeight = VIDEO_HEIGHT,
    .format = VIDEO_PIX_FMT,
    .timebase = {.num = VIDEO_TIMEBASE_NUM, .den = VIDEO_TIMEBASE_DEN},
    .sampleAspectRatio = {.num = 1, .den = 1},
}};
// {
//     .name = "low",
//     .bitrate = 150000,
//     .path = RECORD_PATH2,
//     .gop = 100,
//     .inWidth = VIDEO_WIDTH,
//     .inHeight = VIDEO_HEIGHT,
//     .outWidth = 256,
//     .outHeight = 144,
//     .format = VIDEO_PIX_FMT,
//     .timebase = {.num = VIDEO_TIMEBASE_NUM, .den = VIDEO_TIMEBASE_DEN},
//     .sampleAspectRatio = {.num = 1, .den = 1},
// }};

// TODO: implement the audio processing
void switchPushAFrame(AVFrame* frame) {}

// Called when frame should be written to the output, push the frame to all
// outputs modules which in turn will change bitrate, and resosluiont and
// then output dash / hls. Stream 0 will also output rtmp or mpegts recording
// if enabled.
void switchPushVFrame(AVFrame* frame) {
  int i, cfgLength, ret;
  cfgLength = sizeof(vOutCfg) / sizeof(vOutCfg[0]);
  // if (frame->width == 0) return;
  for (i = 0; i < cfgLength; i++) {
    AVFrame* tmpFrame = av_frame_clone(frame);
    outputWriteVideoFrame(&vOutCfg[i], tmpFrame);
    av_frame_unref(tmpFrame);
  }
}

int main() {
  int ret, i, cfgLength;
  // av_log_set_level(AV_LOG_DEBUG);
  av_log(NULL, AV_LOG_DEBUG, "Main::going to start the media server\n");

  rtmpInputStart(RTMP_IN_URL, NULL, NULL);

  cfgLength = sizeof(vOutCfg) / sizeof(vOutCfg[0]);
  for (i = 0; i < cfgLength; i++) {
    av_log(NULL, AV_LOG_DEBUG, "Going to setup output = %s\n", vOutCfg[i].name);
    ret = startOutput(&vOutCfg[i]);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR,
             "main::could not start output. abort. code = %i\n", ret);
      exit(1);
    }
  }

  // TODO: streamStart, sessionStart, sessionEnd also must come from cmd
  // arguments
  char *streamStart, *sessionStart, *sessionEnd;
  getNowAsIso(&streamStart);
  getNowAsIso(&sessionStart);
  getNowAsIso(&sessionEnd);

  ret = inputSwitchInit(switchPushVFrame, switchPushAFrame, rtmpIsRunning,
                        preFiller, sessionFiller, postFiller, streamStart,
                        sessionStart, sessionEnd);

  rtmpInputJoin();
}