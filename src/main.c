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
#include "output.h"
#include "rtmpInput.h"
#include "utils.h"
#include <MQTTAsync.h>

#if !defined(_WIN32)
#include <unistd.h>
#else
#include <windows.h>
#endif
 
#if defined(_WRS_KERNEL)
#include <OsWrapper.h>
#endif
 
#define TOPIC       "ums"
#define PAYLOAD     "Hello World!"
#define TIMEOUT     10000L
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
char *dashManifestPath = NULL;
char *recordPath = NULL;
char *preFiller = NULL;
char *sessionFiller = NULL;
char *postFiller = NULL;
char *streamStart = NULL;
char *sessionStart = NULL;
char *sessionEnd = NULL;
int disc_finished = 0;
int subscribed = 0;
int finished = 0;


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
    {.bitrate = 140000,
     .outWidth = 256,
     .outHeight = 144,
     .type = AVMEDIA_TYPE_VIDEO},
};

AudioEncCtx aOutCfg = {.bitrate = 64000};
static DashCtxT dashCtx;
static int cfgLength;
static AVCodecContext **dashCodecList;
static MQTTAsync client;
MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
char* broker_address = "tcp://127.0.0.1:1883";
char* clientid = "ums";
int QOS = 1;

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

  aOutCfg.packet->duration = frame->pkt_duration,

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
  printf("Signal recieved %i\n", signum);
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

    if (dashManifestPath == NULL) {
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

    if (streamStart == NULL)
      getNowAsIso(&streamStart);

    if (sessionStart == NULL)
      getNowAsIso(&sessionStart);

    if (sessionEnd == NULL)
      getNowAsIso(&sessionEnd);

    if (!startsWith(rtmpInUrl, "rtmp://") && !startsWith(rtmpInUrl, "rtmps://")) {
      printf("Error: rtmpIn is not a valid rtmp url --> %s\n", rtmpInUrl);
      return -1;
    }

    if (rtmpOutUrl != NULL) {
      if (!startsWith(rtmpOutUrl, "rtmp://") && !startsWith(rtmpOutUrl, "rtmps://")) {
        printf("Error: rtmpOut is not a valid rtmp url --> %s\n", rtmpOutUrl);
        return -1;
      }
    }

    // check if manifest file path exists
    strcpy(tmpString, dashManifestPath);
    dirname(tmpString);
    dir = opendir(tmpString);
    if (!dir) {
      closedir(dir);
      printf("Error: dash output directory cannot be opened\n");
      return -1;
    }
    closedir(dir);

    // check if recording path exists
    strcpy(tmpString, recordPath);
    dirname(tmpString);
    dir = opendir(tmpString);
    if (!dir) {
      closedir(dir);
      printf("Error: recording output directory cannot be opened");
      return -1;
    }
    closedir(dir);

    return 0;
  }

  printf("Error: unsupported mode --> %s\n", mode);
  return -1;
}

void parse_input(char* message)
{
  char *ptr, *arg; 

  printf("\nparsing message\n");
  // Parse the input parameters
  ptr = strtok_r(message, " ",&arg);
  while (ptr != NULL) {

    printf("%s\n", ptr);
    switch (*ptr) {
    case OPT_RTMP_IN:
      rtmpInUrl = strtok_r(NULL," ", &arg);
      break;
    case OPT_RTMP_OUT:
      rtmpOutUrl = strtok_r(NULL," ", &arg);
      break;
    case OPT_MODE:
      mode = strtok_r(NULL," ", &arg);
      break;
    case OPT_DASH:
      dashManifestPath = strtok_r(NULL," ", &arg);
      break;
    case OPT_REC:
      recordPath = strtok_r(NULL," ", &arg);
      break;
    case OPT_PRE_FILLER:
      preFiller = strtok_r(NULL," ", &arg);
      break;
    case OPT_SESSION_FILLER:
      sessionFiller = strtok_r(NULL," ", &arg);
      break;
    case OPT_POST_FILLER:
      postFiller = strtok_r(NULL," ", &arg);
      break;
    case OPT_STREAM_START:
      streamStart = strtok_r(NULL," ", &arg);
      break;
    case OPT_SESSION_START:
      sessionStart = strtok_r(NULL," ", &arg);
      break;
    case OPT_SESSION_END:
      sessionEnd = strtok_r(NULL," ", &arg);
      break;

    default:
      break;
    }
    ptr = strtok_r(NULL," ", &arg);
  }
  //printf("%s\n", mode);
}


void connlost(void *context, char *cause)
{
        MQTTAsync client = (MQTTAsync)context;
        MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
        int rc;
 
        av_log(NULL,AV_LOG_DEBUG, "\nConnection lost\n");
        if (cause)
                av_log(NULL, AV_LOG_ERROR, "     cause: %s\n", cause);
 
        av_log(NULL, AV_LOG_DEBUG,"Reconnecting\n");
        conn_opts.keepAliveInterval = 20;
        conn_opts.cleansession = 1;
        if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
        {
                av_log(NULL, AV_LOG_ERROR,"Failed to start connect, return code %d\n", rc);
                finished = 1;
        }
}
 
 
int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    printf("message received\n");
    av_log(NULL,AV_LOG_DEBUG, "Message arrived\n");
    printf("message arrived on topic %s. message is %s\n", topicName,(char*)message->payload);
    av_log(NULL, AV_LOG_DEBUG, "     topic: %s\n", topicName);
    av_log(NULL, AV_LOG_DEBUG, "   message: %.*s\n", message->payloadlen, (char*)message->payload);
    if (strcmp(topicName, TOPIC) ==0)
    {
      parse_input((char*)message->payload);
    }
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 0;
}
 
void onDisconnectFailure(void* context, MQTTAsync_failureData* response)
{
        av_log(NULL,AV_LOG_ERROR, "Disconnect failed, rc %d\n", response->code);
        disc_finished = 1;
}
 
void onDisconnect(void* context, MQTTAsync_successData* response)
{
        av_log(NULL,AV_LOG_DEBUG,"Successful disconnection\n");
        disc_finished = 1;
}
 
void onSubscribe(void* context, MQTTAsync_successData* response)
{
        av_log(NULL, AV_LOG_DEBUG, "Subscribe succeeded\n");
        subscribed = 1;
}
 
void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
        av_log(NULL,AV_LOG_ERROR,"Subscribe failed, rc %d\n", response->code);
        finished = 1;
}
 
 
void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
  printf("connection failure %d", response->code);
        av_log(NULL, AV_LOG_ERROR,"Connect failed, rc %d\n", response->code);
        finished = 1;
}
 
 
void onConnect(void* context, MQTTAsync_successData* response)
{
        MQTTAsync client = (MQTTAsync)context;
        MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
        int rc;
 
        av_log(NULL,AV_LOG_DEBUG,"Successful connection\n");
 
        av_log(NULL,AV_LOG_DEBUG,"Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n\n", TOPIC, clientid, QOS);
        opts.onSuccess = onSubscribe;
        opts.onFailure = onSubscribeFailure;
        opts.context = client;
        if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS)
        {
                av_log(NULL,AV_LOG_ERROR, "Failed to start subscribe, return code %d\n", rc);
                finished = 1;
        }
}
 

int mqtt_connection()
{
    int clientCreateCode, connectionStartCode, callbackCode;

    MQTTAsync client;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;

    av_log(NULL, AV_LOG_DEBUG, "creating new mqtt client");

    clientCreateCode = MQTTAsync_create(&client, broker_address, clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (clientCreateCode != MQTTASYNC_SUCCESS)
    {
      av_log(NULL, AV_LOG_ERROR,"Failed to create client, return code %d\n", clientCreateCode);
      exit(1);
    }
    if ((callbackCode = MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL)) != MQTTASYNC_SUCCESS)
    {
      av_log(NULL, AV_LOG_ERROR, "Failed to set callbacks, return code %d\n", callbackCode);
      callbackCode = EXIT_FAILURE;
    }

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = onConnect;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = client;
      
    av_log(NULL, AV_LOG_DEBUG, "connecting to mqtt broker");

    connectionStartCode= MQTTAsync_connect(client, &conn_opts);
    
    if (connectionStartCode!= MQTTASYNC_SUCCESS)
    {
        av_log(NULL,AV_LOG_ERROR, "Failed to connect, return code %d\n", connectionStartCode);
        exit(1);
    }
    while (!subscribed && !finished)
      sleep(1);
    
    if (finished)
    {
      av_log(NULL, AV_LOG_DEBUG, "mqtt connection ended");
    }

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

  // if no parameters are being passes show the help
  /*if (argc < 2) {
    showHelp();
    return 1;
  }*/

  mqtt_connection();
  printf("mqtt connectin Successful\n");
  //printf("%s\n", mode);

  // before we continue validate the input data
  if (validateInput() < 0) {
    return 1;
  };

  printf("input validated successfully\n");
  // Rtmp output is optional, so if set we enable rtmp output for the high res output
  if (rtmpOutUrl) {
    vOutCfg[0].url = rtmpOutUrl;
  }

  // MPEGTS recording is optional, so if set we enable recording for the high res output
  if (recordPath) {
    vOutCfg[0].path = recordPath;
  }

  // av_log_set_level(AV_LOG_QUIET);
  signal(SIGINT, signalCloseHandler);
  // mtrace();

  dashDirName = dirname(strdup(dashManifestPath));
  ret = mkdirP(dashDirName);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "main::could not create dash dir\n");
    exit(1);
  }

  cleanDashDir(dashDirName);

  av_log(NULL, AV_LOG_DEBUG, "main::going to start the media server\n");
  cfgLength = sizeof(vOutCfg) / sizeof(vOutCfg[0]);

  dashCodecList = malloc(cfgLength * sizeof(AVCodecContext *));
  if (!dashCodecList) {
    av_log(NULL, AV_LOG_ERROR, "main::could not allocated codec list. abort.");
    exit(1);
  }

  rtmpInputStart(rtmpInUrl);
  printf("rtmp input started\n");
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
      av_log(NULL, AV_LOG_WARNING, "main::could not start output. stream index = %i\n", i);
      continue;
    }

    if (vOutCfg[i].type == AVMEDIA_TYPE_VIDEO) {
      dashCodecList[i] = vOutCfg[i].videoEncCtx;
      dashCtx.timebase = vOutCfg[i].timebase; // take timebase from any output,
                                              // as they are all the same
    }
  }

  dashCtx.dashIndexPath = dashManifestPath;
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