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
#include <libgen.h>
#include <mcheck.h>
#include <signal.h>

#include "audioEncoder.h"
#include "config.h"
#include "dash.h"
#include "inputSwitch.h"
#include "output.h"
#include "rtmpInput.h"
#include "utils.h"

// TODO: these defines need to be passed to the main fucntion
//       via command line argumens and should not be statically
//       compiled within the code
#define RTMP_IN_URL "rtmp://localhost/live/input"
#define RTMP_OUT_URL "rtmp://localhost/live/output"
#define DASH_MANIFEST_PATH "/tmp/dash/index.mpd"
#define RECORD_PATH "/tmp/testing.ts"
#define preFiller "/home/thomas/Pictures/a.jpg"
#define sessionFiller "/home/thomas/Pictures/a.jpg"
#define postFiller "/home/thomas/Pictures/a.jpg"

// Output configuration
OutputCtxT vOutCfg[] = {
    {.name = "main",
     .bitrate = 2880000,
     .url = RTMP_OUT_URL,
     .path = RECORD_PATH,
     .outWidth = VIDEO_WIDTH,
     .outHeight = VIDEO_HEIGHT,
     .type = AVMEDIA_TYPE_VIDEO},
    {.bitrate = 1440000,
     .outWidth = 960,
     .outHeight = 540,
     .type = AVMEDIA_TYPE_VIDEO},
    {.bitrate = 720000,
     .outWidth = 960,
     .outHeight = 540,
     .type = AVMEDIA_TYPE_VIDEO},
    {.bitrate = 540000,
     .outWidth = 640,
     .outHeight = 360,
     .type = AVMEDIA_TYPE_VIDEO},
    {.bitrate = 280000,
     .outWidth = 512,
     .outHeight = 288,
     .type = AVMEDIA_TYPE_VIDEO},
    {.bitrate = 140000,
     .outWidth = 384,
     .outHeight = 216,
     .type = AVMEDIA_TYPE_VIDEO},
    {.bitrate = 140000,
     .outWidth = 256,
     .outHeight = 144,
     .type = AVMEDIA_TYPE_VIDEO},
};

AudioEncCtx aOutCfg = {.bitrate = 64000};

void switchPushAFrame(AVFrame* frame) {
  int i, cfgLength, ret, pts;
  aOutCfg.frame = frame;
  pts = frame->pkt_pts;

  ret = audioEncoderRun(&aOutCfg);
  if (ret < 0) {
    return;
  }

  cfgLength = sizeof(vOutCfg) / sizeof(vOutCfg[0]);
  for (i = 0; i < cfgLength; i++) {
    outputWriteAudioPacket(&vOutCfg[i]);
  }
}

void switchPushVFrame(AVFrame* frame) {
  int i, cfgLength, ret;

  cfgLength = sizeof(vOutCfg) / sizeof(vOutCfg[0]);
  for (i = 0; i < cfgLength; i++) {
    // AVFrame* clone = av_frame_clone(frame);
    outputWriteVideoFrame(&vOutCfg[i], frame);
    // av_frame_free(&clone);
  }
}

static DashCtxT dashCtx;
static int cfgLength;
static AVCodecContext** dashCodecList;

/**
 * This is not 100% needed but to find memory leak with mtrace better to close
 * everything at the
 */
void signalCloseHandler(int signum) {
  int i;
  printf("Signal recieved %i\n", signum);
  inputSwitchClose();
  dashClose(&dashCtx);

  for (i = 0; i < cfgLength; i++) {
    outputClose(&vOutCfg[i]);
  }

  rtmpInputStop();
  rtmpInputJoin();  // wait for rtmp input thread to be terminated
  free(dashCodecList);
  exit(signum);
}

int main() {
  int ret, i;

  char* dashDirName;

  // av_log_set_level(AV_LOG_QUIET);
  signal(SIGINT, signalCloseHandler);
  // mtrace();

  dashDirName = dirname(strdup(DASH_MANIFEST_PATH));
  ret = mkdirP(dashDirName);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "main::could not create dash dir\n");
    exit(1);
  }

  ret = cleanDir(dashDirName);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "main::could not clean dash dir\n");
    exit(1);
  }

  av_log(NULL, AV_LOG_DEBUG, "main::going to start the media server\n");
  cfgLength = sizeof(vOutCfg) / sizeof(vOutCfg[0]);

  dashCodecList = malloc(cfgLength * sizeof(AVCodecContext*));
  if (!dashCodecList) {
    av_log(NULL, AV_LOG_ERROR, "main::could not allocated codec list. abort.");
    exit(1);
  }

  rtmpInputStart(RTMP_IN_URL);
  // rtmpInputJoin();  // TODO: remove this, only to test and implement audio in
  // the input

  ret = audioEncoderInit(&aOutCfg);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "main::audio encoder init failed\n");
    exit(1);
  }

  for (i = 0; i < cfgLength; i++) {
    av_log(NULL, AV_LOG_DEBUG, "Going to setup output = %s\n", vOutCfg[i].name);
    vOutCfg[i].streamIdx = i;
    vOutCfg[i].dashCtx = &dashCtx;
    vOutCfg[i].audioEnc = &aOutCfg;
    ret = startOutput(&vOutCfg[i]);
    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING,
             "main::could not start output. stream index = %i\n", i);
      // exit(1)
      continue;
    }

    if (vOutCfg[i].type == AVMEDIA_TYPE_VIDEO) {
      dashCodecList[i] = vOutCfg[i].videoEncCtx;
      dashCtx.timebase = vOutCfg[i].timebase;  // take timebase from any output,
                                               // as they are all the same
    }
  }

  dashCtx.dashIndexPath = DASH_MANIFEST_PATH;
  dashCtx.streamLen = cfgLength;
  ret = startDash(&dashCtx, dashCodecList);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "main::could not start dash output...");
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