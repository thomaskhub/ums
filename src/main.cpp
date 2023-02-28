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

  // Setup a single Encoder (just for testing and first implementation )
  EncoderConfig vEncCfg;
  vEncCfg.bitrate = 1440000;
  vEncCfg.codecId = AV_CODEC_ID_H264;
  vEncCfg.fps = 25;
  vEncCfg.gop = 100;
  vEncCfg.inHeight = 720;
  vEncCfg.inWidth = 1280;
  vEncCfg.inPixFormat = AV_PIX_FMT_YUV420P;
  vEncCfg.outHeight = 720;
  vEncCfg.outWidth = 1280;
  vEncCfg.timebase = {
      .num = 1,
      .den = 1000000,
  };

  Output mpegts = Output("/tmp/test.ts", "", NULL);

  Encoder videoEncoder = Encoder(vEncCfg);
  videoEncoder.output = &mpegts; // TODO: this is only for quick testing must be removed

  videoEncoder.init();
  mpegts.open();
  int ret = videoEncoder.setCodecPar(&mpegts.videoStream->codecpar);
  std::cout << "thomas -->  " << ret << std::endl;
  mpegts.writeHeader();

  //
  // Setup RTMP input
  //
  RtmpInput primaryInput = RtmpInput(config.primaryInput, NULL);
  primaryInput.videoEncoder = &videoEncoder;
  primaryInput.run();

  // Wait until it RTMP input finishes
  primaryInput.join();
}