#ifndef __INPUT_SWITCH__
#define __INPUT_SWITCH__

#include "encoder.h"
#include <fstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

class InputSwitch {
private:
  Encoder *audioEncoders[32];
  int audioEncCnt;

  Encoder *videoEncoders[32];
  int videoEncCnt;

  uint64_t videoPts;
  uint64_t audioPts;

  std::ofstream Log;

public:
  InputSwitch() {
    this->videoPts = 0;
    this->audioPts = 0;
    this->audioEncCnt = 0;
    this->videoEncCnt = 0;
  };

  /**
   * @brief Push a frame into the input switch
   *
   * @param frame
   * @param inputId: select in which input buffer to store the data (primary, secondary etc)
   * @return int
   */
  int pushAudio(AVFrame *frame, int inputId);
  int pushVideo(AVFrame *frame, int inputId);

  int addAudioEncoder(Encoder *encoder);
  int addVideoEncoder(Encoder *encoder);
};

#endif