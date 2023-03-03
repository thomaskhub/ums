#include "inputSwitch.h"

std::ofstream vLog("/tmp/videFilterOut.txt");
std::ofstream aLog("/tmp/audioFilterOut.txt");

int InputSwitch::pushAudio(AVFrame *frame, int inputId) {
  int ret;
  aLog << frame->pts << " " << frame->pkt_dts << " " << frame->best_effort_timestamp << " " << frame->nb_samples << "" << frame->pkt_duration << std::endl;
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
  vLog << frame->pts << " " << frame->pkt_dts << " " << frame->best_effort_timestamp << " " << std::endl;

  for (int i = 0; i < this->videoEncCnt; i++) {

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
