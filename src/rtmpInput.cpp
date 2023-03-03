#include "rtmpInput.h"

void *RtmpInput::worker(void *data) {
  RtmpInput *input = (RtmpInput *)(data);
  int ret;

  // Will be in this loop until error occurs or user stops it
  ret = input->processLoop();
  return NULL;
}

int RtmpInput::run() {
  this->runThread = true;
  pthread_create(&this->inputThread, NULL, this->worker, (void *)this);
  return 0;
};

void RtmpInput::join() {
  pthread_join(this->inputThread, NULL);
}

void RtmpInput::stop() {
  if (this->videoOutFrame)
    av_frame_free(&this->videoOutFrame);

  if (this->audioOutFrame)
    av_frame_free(&this->audioOutFrame);

  if (this->audioDecoder)
    this->audioDecoder->close();

  if (this->videoDecoder)
    this->videoDecoder->close();

  this->Input::close();
  this->runThread = false;
}

int RtmpInput::openDecoders() {
  int ret;

  ret = this->audioDecoder->open(this->Input::audioStream, this->AUDIO_FILTER);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not open audio decoder\n");
    this->Input::close();
    return ret;
  }

  ret = this->videoDecoder->open(this->Input::videoStream, this->VIDEO_FILTER);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not open video decoder\n");
    this->audioDecoder->close();
    this->Input::close();
  }

  return ret;
}

std::ofstream rtmpVLog("/tmp/rtmpVideoOut.txt");
std::ofstream rtmpALog("/tmp/rtmpAudioOut.txt");

int RtmpInput::init() {
  int ret;
  ret = getEmptyVideoFrame(
      &this->videoOutFrame, Config().VIDEO_PIX_FMT, Config().VIDEO_WIDTH,
      Config().VIDEO_HEIGHT);

  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "rtmpInputStart::could not get empty frame\n");
    return ret;
  }

  ret = getEmptyAudioFrame(&this->audioOutFrame, (AVSampleFormat)Config().AUDIO_SAMPLE_FMT,
                           Config().AUDIO_NB_SAMPLES, Config().AUDIO_CH_LAYOUT,
                           Config().AUDIO_RATE);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "rtmpInputStart::could not get empty frame\n");
    av_frame_free(&this->videoOutFrame);
    return ret;
  }

  return 0;
}

int RtmpInput::processLoop() {
  int ret;

  if ((ret = this->init()) < 0) {
    return ret;
  };

  while (this->runThread && ret >= 0) {
    ret = this->Input::openFile(true, true);
    this->state = RTMP_WAIT_INPUT;
    /**
     * If RTMP inout is not availble yet we wait for some time and retry.
     * This is implemented to automatically recover if input is reconnecting
     */
    if (ret < 0) {
      av_log(NULL, AV_LOG_DEBUG, "rtmpInput::Could not open rtmp input\n");
      ret = 0;
      this->state = RTMP_RETRY;
      sleep(3);
      continue;
    }

    if (this->openDecoders() < 0) {
      continue;
    }

    this->state = RTMP_RUNNING;

    while (this->runThread) {

      if (this->Input::readPacket(&this->rtmpPacket) < 0) {
        break;
      }

      if (rtmpPacket.stream_index == this->Input::audioStream->index) {
        ret = this->audioDecoder->sendPacket(&rtmpPacket, &audioOutFrame);

        if (audioOutFrame->best_effort_timestamp < this->getStartTime()) {
          av_frame_unref(audioOutFrame);
          continue; // read the nect packet and drop this frame until we
                    // reache the start
        }

        if (ret < 0 && ret != AVERROR(EAGAIN)) {
          this->stop();
          break;
        };

        if (ret == 0) {
          rtmpALog << audioOutFrame->pts << "," << audioOutFrame->pkt_dts << "," << audioOutFrame->best_effort_timestamp << "," << audioOutFrame->pkt_duration << std::endl;
          this->inputSwitch->pushAudio(audioOutFrame, this->inputId);
        }
        av_frame_unref(this->audioOutFrame);

      } else if (rtmpPacket.stream_index == this->Input::videoStream->index) {
        ret = this->videoDecoder->sendPacket(&rtmpPacket, &videoOutFrame);
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
          this->stop();
          break;
        };

        if (videoOutFrame->best_effort_timestamp < this->getStartTime()) {
          av_frame_unref(videoOutFrame);
          continue;
        }

        if (ret == 0) {
          rtmpVLog << videoOutFrame->pts << "," << videoOutFrame->pkt_dts << "," << videoOutFrame->best_effort_timestamp << "," << videoOutFrame->pkt_duration << std::endl;
          this->inputSwitch->pushVideo(videoOutFrame, this->inputId);
        }
        av_frame_unref(this->videoOutFrame);
      }

      av_packet_unref(&this->rtmpPacket);
    }

    this->state = RTMP_STOPPED;
    ret = 0;
  }

  return 0;
}
