#ifndef __DASH__
#define __DASH__

#include "config.h"
#include "filters.h"
#include "mux.h"
#include "utils.h"

typedef struct DashCtxT {
  /**
   * @brief video and audio streams for the dash output
   * 0 - 6 are video streams (0 being the best qualit, 6 the works quality)
   * 7 is audio stream
   */
  AVStream** dashStreams;
  uint8_t streamLen;
  AVFormatContext* dashOutCtx;
  AVRational timebase;

  /**
   * @brief path to the manifest file (index.mpd)
   */
  char* dashIndexPath;

} DashCtxT;

int startDash(DashCtxT* data, AVCodecContext** encoderCtx);
void dashWritePacket(DashCtxT* data, AVPacket* packet);
void dashClose(DashCtxT* data);

#endif