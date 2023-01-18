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
#include <MQTTClient.h>
#include <dirent.h>
#include <getopt.h>
#include <libavutil/log.h>
#include <libgen.h>
#include <mcheck.h>
#include <signal.h>

#include "audioEncoder.h"
#include "config.h"
#include "dash.h"
#include "inputSwitch.h"
#include "mqtt.h"
#include "output.h"
#include "rtmpInput.h"
#include "utils.h"

#define OPT_MODE 0
#define OPT_RTMP_IN 1
#define OPT_RTMP_OUT 2
#define OPT_DASH 3
#define OPT_REC 4
#define OPT_PRE_FILLER 5
#define OPT_SESSION_FILLER 6
#define OPT_POST_FILLER 7
#define OPT_STREAM_START 8
#define OPT_SESSION_START 9
#define OPT_SESSION_END 10

volatile GlobalT global;
pthread_t inputSwitchThread;

static struct option longOptions[] = {
    {"mode", required_argument, 0, OPT_MODE},
    {"rtmpIn", required_argument, 0, OPT_RTMP_IN},
    {"rtmpOut", required_argument, 0, OPT_RTMP_OUT},
    {"dash", required_argument, 0, OPT_DASH},
    {"rec", required_argument, 0, OPT_REC},
    {"preFiller", required_argument, 0, OPT_PRE_FILLER},
    {"sessionFiller", required_argument, 0, OPT_SESSION_FILLER},
    {"postFiller", required_argument, 0, OPT_POST_FILLER},
    {"streamStart", required_argument, 0, OPT_STREAM_START},
    {"sessionStart", required_argument, 0, OPT_SESSION_START},
    {"sessionEnd", required_argument, 0, OPT_SESSION_END},
    {0, 0, 0, 0},
};

char *mode = NULL;
char *rtmpInUrl = NULL;
char *rtmpOutUrl = NULL;
char *dashPath = NULL;
char *recordPath = NULL;
char *preFiller = NULL;
char *sessionFiller = NULL;
char *postFiller = NULL;
char *streamStart = NULL;
char *doorOpen = NULL;
char *sessionStart = NULL;
char *sessionEnd = NULL;

Mqtt mqttContext;

// Output configuration
OutputCtxT vOutCfg[] = {
    {.name = "main",
     .bitrate = 2880000,
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
    {.bitrate = 50000,
     .outWidth = 256,
     .outHeight = 144,
     .type = AVMEDIA_TYPE_VIDEO},
};

AudioEncCtx aOutCfg = {.bitrate = 64000};
static DashCtxT dashCtx;
static int cfgLength;
static AVCodecContext **dashCodecList;

/**
 * @brief show help on how to use this application
 */
void showHelp() {
  printf("ums - uniterruptible media server\n");
  printf("Usage: ums [GNU long options] ...\n");

  printf("GNU long options:\n");
  printf("\t--mode [mode]                : can be live, vod, vas (video-as-stream)\n");
  printf("\t--rtmpIn url                 : url is rtmp consume url video\n");
  printf("\t--rtmpOut url                : url is rtmp produce url (optional)\n");
  printf("\t--dash path                  : directory in which to store dash/hls output data\n");
  printf("\t--rec path                   : path to store the MPEGTS recording (optional)\n");
  printf("\t--preF path                  : path to the pre-session filler jpg\n");
  printf("\t--sessionF path              : path to the session filler jpg\n");
  printf("\t--postF path                 : path to the post-session filler jpg\n");
  printf("\t--streamStart ISO_TIMESTAMP  : stream start ISO formated string\n");
  printf("\t--sessionStart ISO_TIMESTAMP : session start ISO formated string\n");
  printf("\t--sessionEnd ISO_TIMESTAMP   : session end ISO formated string\n");
}

// TODO: remove this and replace this with MQTT thread
void *_statsWorker(void *args) {
  double inputSwitchDuration = 0;
  double inputSwitchFps = 0;
  double inputSwitchSR = 0;
  while (1) {
    av_usleep(3000000);

    inputSwitchDuration = (av_gettime_relative() - global.inputSwitchStartTime) / 1000000.0;
    inputSwitchFps = (double)global.inputSwitchVFrameCnt / inputSwitchDuration;
    inputSwitchSR = (double)global.inputSwitchAFrameCnt / inputSwitchDuration;

    printf("inputSwitch:: Video FPS = %f, Audio Sample Rate = %f\n", inputSwitchFps, inputSwitchSR);
    printf("main::runtime = %f\n", (av_gettime_relative() - global.mainStartTime) / 1000000.0);
    printf("rtmpInput::status = %s\n", global.rtmpInputStatus);

    // Send and forget
    MQTTClient_message message = MQTTClient_message_initializer;
    MQTTClient_token token;

    char payload[2048];
    sprintf(payload, "{\"cmd\":\"update\",\"payload\":{\"fps\":\"%f\",\"sps\":\"%f\", \"rtmpInputStatus\":\"%s\"}}", inputSwitchFps, inputSwitchSR, global.rtmpInputStatus);

    message.payload = payload;
    message.payloadlen = strlen(payload);
    message.qos = 2;
    message.retained = 0;
    MQTTClient_publishMessage(mqttContext.client, "testing", &message, &token);
  }
}

/**
 * @brief write frame to output modules
 * this callback is triggered when the input module has a new audio frame
 * which needs to be processes. First the audio frame is encoded according
 * to our output specification (mono, 64kbits datarate, 44100kHz sampling).
 * We will have only one audio datarate for the DASH ABR.
 *
 * Then it writes the frame to all the output modules which are:
 *  - dash / hls
 *  - rtmp output
 *  - MPEGTS recording
 *
 * @param frame
 */
void switchPushAFrame(AVFrame *frame) {
  int i, cfgLength, ret;
  aOutCfg.frame = frame;

  ret = audioEncoderRun(&aOutCfg);
  if (ret < 0) {
    return;
  }

  cfgLength = sizeof(vOutCfg) / sizeof(vOutCfg[0]);
  for (i = 0; i < cfgLength; i++) {
    outputWriteAudioPacket(&vOutCfg[i]);
  }
  av_packet_unref(aOutCfg.packet);
}

/**
 * @brief forwards video frame to output modules
 * It forwards the video frame (720p) to the output modules. Inside the output
 * output module the video frame is encoded in the differenc resolutions needed
 * for the Dash / Hls ABR
 * @param frame
 */
void switchPushVFrame(AVFrame *frame) {
  int i, cfgLength, ret;

  cfgLength = sizeof(vOutCfg) / sizeof(vOutCfg[0]);
  for (i = 0; i < cfgLength; i++) {
    outputWriteVideoFrame(&vOutCfg[i], frame);
  }
}

/**
 * This is not 100% needed but to find memory leak with mtrace better to close
 * everything at the
 */
void signalCloseHandler(int signum) {
  int i;
  inputSwitchClose();
  dashClose(&dashCtx);

  for (i = 0; i < cfgLength; i++) {
    outputClose(&vOutCfg[i]);
  }

  rtmpInputStop();
  rtmpInputJoin(); // wait for rtmp input thread to be terminated
  free(dashCodecList);
  exit(signum);
}

/**
 * @brief check if string a starts with string b
 *
 * @param a
 * @param b
 * @return int
 */
int startsWith(const char *a, const char *b) {
  if (strncmp(a, b, strlen(b)) == 0) {
    return 1;
  }
  return 0;
}

/**
 * @brief input validation
 * depending on the operating mode we will check if all parameters are set
 * and if the defined parameters are actually correct.
 *
 * @return int
 */
int validateInput() {
  int ret;
  char tmpString[2048];
  DIR *dir;

  if (!mode) {
    printf("Error: mode is not set\n");
    return -1;
  }

  //
  // validate inputs for live mode
  //
  if (!strcmp(mode, "live")) {
    if (rtmpInUrl == NULL) {
      printf("Error: rtmpIn argument is null\n");
      return -1;
    }

    if (dashPath == NULL) {
      printf("Error: dash argument is null\n");
      return -1;
    }

    if (preFiller == NULL || sessionFiller == NULL || postFiller == NULL) {
      printf("Error: filler configuration incorrect\n");
      return -1;
    }

    // check filler files only in live mode, all other modes do not need this
    if (strcmp(mode, "live") == 0) {

      if (!fileExists(preFiller)) {
        printf("Error: pre filler file not found\n");
        return -1;
      }

      if (!fileExists(sessionFiller)) {
        printf("Error: session filler file not found\n");
        return -1;
      }

      if (!fileExists(postFiller)) {
        printf("Error: post filler file not found\n");
        return -1;
      }
    }

    // TODO: check if now as default time is ok or if this validation
    // should be done differently
    if (streamStart == NULL)
      getNowAsIso(&streamStart);

    if (sessionStart == NULL)
      getNowAsIso(&sessionStart);

    if (sessionEnd == NULL)
      getNowAsIso(&sessionEnd);

    if (doorOpen == NULL)
      getNowAsIso(&sessionEnd);

    if (!startsWith(rtmpInUrl, "rtmp://") && !startsWith(rtmpInUrl, "rtmps://")) {
      printf("Error: rtmpInUrl is not a valid rtmp url --> %s\n", rtmpInUrl);
      return -1;
    }

    if (rtmpOutUrl != NULL) {
      if (!startsWith(rtmpOutUrl, "rtmp://") && !startsWith(rtmpOutUrl, "rtmps://")) {
        printf("Error: rtmpOut is not a valid rtmp url --> %s\n", rtmpOutUrl);
        return -1;
      }
    }

    // check if manifest file path exists
    if (!dashPath) {
      av_log(NULL, AV_LOG_ERROR, "dash parameter not defined\n");
      return -1;
    }

    printf("Dash manifest path --> %s\n", dashPath);
    strcpy(tmpString, dashPath);
    // dirname(tmpString);
    dir = opendir(tmpString);
    if (!dir) {
      // closedir(dir);
      printf("Error: dash output directory cannot be opened %s\n", tmpString);
      return -1;
    }
    closedir(dir);

    // check if recording path exists
    if (!recordPath) {
      av_log(NULL, AV_LOG_ERROR, "recordPath parameter not defined\n");
      return -1;
    }

    strcpy(tmpString, recordPath);
    dir = opendir(tmpString);
    if (!dir) {
      printf("Error: recording output directory cannot be opened\n");
      return -1;
    }
    closedir(dir);

    return 0;
  }

  printf("Error: unsupported mode --> %s\n", mode);
  return -1;
}

/**
 * @brief Of cource the entry point to the application :)
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char **argv) {
  int ret, i, c, optionIndex;
  char *dashDirName;

  char *mqttUrl = getenv("mqttUrl");

  // Start mqtt only if url was provided
  if (mqttUrl != NULL) {
    mqttStart(&mqttContext, mqttUrl);
    rtmpInUrl = getenv("rtmpInUrl");
    rtmpOutUrl = getenv("rtmpOutUrl");
    mode = getenv("mode");
    dashPath = getenv("dash");
    recordPath = getenv("recordPath");
    preFiller = getenv("preFiller");
    sessionFiller = getenv("sessionFiller");
    postFiller = getenv("postFiller");
    streamStart = getenv("rtmpOutUrl");
    doorOpen = getenv("doorOpen");
    sessionStart = getenv("rtmpOutUrl");
    sessionEnd = getenv("rtmpOutUrl");

  } else {

    // if no parameters are being passes show the help
    if (argc < 2) {
      showHelp();
      return 1;
    }
    // Parse the input parameters
    while (1) {
      c = getopt_long_only(argc, argv, "", longOptions, &optionIndex);
      if (c == -1) {
        break;
      }

      switch (c) {
      case OPT_RTMP_IN:
        rtmpInUrl = optarg;
        break;
      case OPT_RTMP_OUT:
        rtmpOutUrl = optarg;
        break;
      case OPT_MODE:
        mode = optarg;
        break;
      case OPT_DASH:
        dashPath = optarg;
        break;
      case OPT_REC:
        recordPath = optarg;
        break;
      case OPT_PRE_FILLER:
        preFiller = optarg;
        break;
      case OPT_SESSION_FILLER:
        sessionFiller = optarg;
        break;
      case OPT_POST_FILLER:
        postFiller = optarg;
        break;
      case OPT_STREAM_START:
        streamStart = optarg;
        break;
      case OPT_SESSION_START:
        sessionStart = optarg;
        break;
      case OPT_SESSION_END:
        sessionEnd = optarg;
        break;

      default:
        exit(1);
        break;
      }
    }
  }
  // before we continue validate the input data
  if (validateInput() < 0) {
    return 1;
  };

  av_log_set_level(AV_LOG_ERROR);

  // Rtmp output is optional, so if set we enable rtmp output for the high res output
  if (rtmpOutUrl) {
    vOutCfg[0].url = rtmpOutUrl;
  }

  // MPEGTS recording is optional, so if set we enable recording for the high res output
  if (recordPath) {
    char rPath[512];
    sprintf(rPath, "%s/rec.ts", recordPath);
    vOutCfg[0].path = rPath;
  }

  signal(SIGINT, signalCloseHandler);
  cleanDashDir(dashPath);

  av_log(NULL, AV_LOG_DEBUG, "main::going to start the media server\n");
  cfgLength = sizeof(vOutCfg) / sizeof(vOutCfg[0]);

  dashCodecList = malloc(cfgLength * sizeof(AVCodecContext *));
  if (!dashCodecList) {
    av_log(NULL, AV_LOG_ERROR, "main::could not allocated codec list. abort.\n");
    exit(1);
  }

  pthread_create(&inputSwitchThread, NULL, _statsWorker, NULL);
  rtmpInputStart(rtmpInUrl);

  ret = audioEncoderInit(&aOutCfg);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "main::audio encoder init failed\n");
    exit(1);
  }

  global.mainStartTime = av_gettime_relative();

  for (i = 0; i < cfgLength; i++) {
    av_log(NULL, AV_LOG_DEBUG, "Going to setup output = %s\n", vOutCfg[i].name);
    vOutCfg[i].streamIdx = i;
    vOutCfg[i].dashCtx = &dashCtx;
    vOutCfg[i].audioEnc = &aOutCfg;
    ret = startOutput(&vOutCfg[i]);
    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING, "main::could not start output. stream index = %i\n", i);
      continue;
    }

    if (vOutCfg[i].type == AVMEDIA_TYPE_VIDEO) {
      dashCodecList[i] = vOutCfg[i].videoEncCtx;
      dashCtx.timebase = vOutCfg[i].timebase; // take timebase from any output,
                                              // as they are all the same
    }
  }

  char dPath[1024];
  sprintf(dPath, "%s/index.mpd", dashPath);
  dashCtx.dashIndexPath = dPath;
  printf("PAth --> %s \n", dashCtx.dashIndexPath);

  dashCtx.streamLen = cfgLength;
  ret = startDash(&dashCtx, dashCodecList, aOutCfg.encCtx);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "main::could not start dash output...");
  }

  ret = inputSwitchInit(switchPushVFrame, switchPushAFrame, rtmpIsRunning,
                        preFiller, sessionFiller, postFiller, streamStart,
                        sessionStart, sessionEnd);
  rtmpInputJoin();
}