#ifndef ___RTMP__INPUT__
#define ___RTMP__INPUT__

#include "input.h"
#include "utils.h"
#include <unistd.h>

#include "decoder.h"
#include "encoder.h"
#include "inputSwitch.h"
#include <functional>

#define RTMP_WAIT_INPUT 0
#define RTMP_RUNNING 1
#define RTMP_STOPPED 2
#define RTMP_RETRY 3
class RtmpInput : public Input, public Decoder {
private:
  InputSwitch *inputSwitch;
  int inputId;

  const std::string AUDIO_FILTER = "aresample=44100,asetnsamples=n=1024:p=0,aformat=channel_layouts=mono,volume=1";
  const std::string VIDEO_FILTER = "scale=1280:720,format=yuv420p,fps=fps=25";

  pthread_t inputThread;
  int state;
  bool runThread;

  /**
   * Input audio and video decoders
   */
  Decoder *videoDecoder;
  Decoder *audioDecoder;
  AVCodecContext *audioDecCtx;
  AVCodecContext *videoDecCtx;
  AVFrame *videoOutFrame;
  AVFrame *audioOutFrame;
  AVPacket rtmpPacket;

  /**
   * @brief pthread worker to ensure auto recovery when input connection
   * is lost. It is responsible of RTMP packet/frame processing
   */
  static void *worker(void *data);

  /**
   * @brief Open the audio and video decoders and preparing them for
   * processing RTMP data
   *
   * @return int
   */
  int openDecoders();

  int init();

public:
  RtmpInput(std::string url, AVDictionary *opts) : Input(url, opts) {
    this->videoDecoder = new Decoder();
    this->audioDecoder = new Decoder();
  };
  ~RtmpInput() {
    this->stop();

    delete this->videoDecoder;
    delete this->audioDecoder;
  }

  /**
   * @brief Start the RTMP input, connects to the specified URL in the
   * constructor and starts processing incoming media data.
   *
   * @return int
   */
  int run();

  /**
   * @brief Stop the internal worker thread which will free all resources
   * and stop the RTMP input completely
   *
   */
  void stop();

  /**
   * @brief Needs to be called to wait for the worker thread to finish processing
   *
   */
  void join();

  /**
   * @brief Needs to be called to start RTMP packet processing
   *
   * @return int
   */
  int processLoop();

  void setInputSwitch(InputSwitch *input, int inputId) {
    this->inputId = inputId;
    this->inputSwitch = input;
  }
};

#endif