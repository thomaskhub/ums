#include "avChunker.h"

int AVChunker::push(AVFrame *frame) {
  // For now for debuging of the rtmp input we just dum everything to a TS file
  // running it through an encoder and MpegTS output
}

int AVChunker::pull(AVFrame *frame) {
}