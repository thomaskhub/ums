#include "packager.h"

void Packager::setStartPts() {
  if (!this->firstVideoFrame && !this->firstAudioFrame) {
    if (this->firstAudioPts >= this->firstVideoPts) {
      this->ptsStart = this->firstAudioPts;
    } else {
      this->ptsStart = this->firstVideoPts;
    }
  }
}

int Packager::pushAudio(AVFrame *frame) {
  if (this->firstAudioFrame) {
    this->firstAudioFrame = false;
    this->firstAudioPts = frame->best_effort_timestamp;
    this->setStartPts();
  }

  // TODO: push frame in shift register
}

int Packager::pushVideo(AVFrame *frame) {
  if (this->firstVideoFrame) {
    this->firstVideoFrame = false;
    this->firstVideoPts = frame->best_effort_timestamp;
    this->setStartPts();
  }

  // TODO: push frame into shift register
}