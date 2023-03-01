#include "./config.h"
#include "encoder.h"
#include "output.h"
#include "rtmpInput.h"
#include <iostream>
#include <libavutil/log.h>
#include <unistd.h>
using namespace std;

int main(int argc, char **argv) {
  Config config = Config();

  bool isValidConfig = config.validateInput();
  if (!isValidConfig) {
    // exit(1);
  }

  config.primaryInput = "rtmp://localhost:1935/live/test"; // TODO: debugging only

  InputSwitch inSwitch = InputSwitch();

  // Setup a single Encoder (just for testing and first implementation )
  EncoderConfig encCfg1280x720;
  encCfg1280x720.codecId = AV_CODEC_ID_H264;
  encCfg1280x720.bitrate = 1440000;
  encCfg1280x720.fps = 25;
  encCfg1280x720.gop = 100;
  encCfg1280x720.inHeight = 720;
  encCfg1280x720.inWidth = 1280;
  encCfg1280x720.inPixFormat = AV_PIX_FMT_YUV420P;
  encCfg1280x720.outHeight = 720;
  encCfg1280x720.outWidth = 1280;
  encCfg1280x720.timebase = {
      .num = 1,
      .den = 1000000,
  };
  encCfg1280x720.type = AVMEDIA_TYPE_VIDEO;

  EncoderConfig encCfgAudio;
  encCfgAudio.type = AVMEDIA_TYPE_AUDIO;
  encCfgAudio.codecId = AV_CODEC_ID_AAC;
  encCfgAudio.bitrate = 64000;
  encCfgAudio.sampleFormat = Config().AUDIO_SAMPLE_FMT;
  encCfgAudio.sampleRate = Config().AUDIO_RATE;
  encCfgAudio.channelLayout = Config().AUDIO_CH_LAYOUT;

  // Setup the encoders first
  Encoder enc1280x720 = Encoder(encCfg1280x720);
  Encoder encAudio = Encoder(encCfgAudio);

  enc1280x720.init();
  encAudio.init();

  inSwitch.addAudioEncoder(&encAudio);
  inSwitch.addVideoEncoder(&enc1280x720);

  // after we have the encoders ready we can setup the outputs
  OutputConfig mpegtTsCfg[2];
  mpegtTsCfg[0].codecContext = enc1280x720.getCodecContext();
  mpegtTsCfg[1].codecContext = encAudio.getCodecContext();

  Output mpegts = Output("/tmp/test.ts", &mpegtTsCfg[0], 2, OUTPUT_MODE_MPEGTS);

  // now we can hook up the outpt
  enc1280x720.addOutput(&mpegts, 0);
  encAudio.addOutput(&mpegts, 1);

  mpegts.open();

  //
  // Setup RTMP input
  //
  RtmpInput primaryInput = RtmpInput(config.primaryInput, NULL);
  primaryInput.setInputSwitch(&inSwitch, 0);
  primaryInput.run();

  // Wait until it RTMP input finishes
  primaryInput.join();
}