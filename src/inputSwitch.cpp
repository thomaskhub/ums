#include "inputSwitch.h"

int InputSwitch::pushAudio(AVFrame *frame, int inputId) {
  int ret;
  for (int i = 0; i < this->audioEncCnt; i++) {

    frame->pts = this->audioPts;
    frame->pkt_dts = this->audioPts;
    frame->best_effort_timestamp = this->audioPts;
    frame->pkt_duration = 1000000 * 1024 / 44100;
    ret = this->audioEncoders[i]->push(frame);
    // TODO: how to handle errors?
    this->audioPts += 1000000 * 1024 / 44100;
  }

  return 0;
}

int InputSwitch::pushVideo(AVFrame *frame, int inputId) {
  int ret;
  for (int i = 0; i < this->videoEncCnt; i++) {
    // TODO: PTS must be set properly

    frame->pts = this->videoPts;
    frame->pkt_dts = this->videoPts;
    frame->best_effort_timestamp = this->videoPts;
    frame->pkt_duration = 1000000 / 25;
    ret = this->videoEncoders[i]->push(frame);
    // TODO: how to handle errors?
    this->videoPts += 1000000 / 25;
  }

  return 0;
}

int InputSwitch::addAudioEncoder(Encoder *encoder) {
  if (this->audioEncCnt >= 32) {
    return AVERROR(ENOMEM);
  }

  this->audioEncoders[this->audioEncCnt] = encoder;
  this->audioEncCnt++;
  return 0;
};

int InputSwitch::addVideoEncoder(Encoder *encoder) {
  if (this->videoEncCnt >= 32) {
    return AVERROR(ENOMEM);
  }

  this->videoEncoders[this->videoEncCnt] = encoder;
  this->videoEncCnt++;

  return 0;
};
