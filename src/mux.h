#ifndef __MUX_H__
#define __MUX_H__

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <unistd.h>

int openInput(AVFormatContext **inFmtCtx, char *filename,
              AVStream **audioStream, AVStream **videoStream);

void closeInput(AVFormatContext **inFmtCtx);

int openOutput(AVFormatContext **outFmtCtx, char *filename,
               AVStream **audioStream, AVStream **videoStream,
               const char *formatName);

void closeOutput(AVFormatContext **outFmtCtx);

void closeCodec(AVCodecContext **codec);

/**
 * @brief open a decoder based on a AVStream e.g. coming from an input file
 *
 * @param decCtx
 * @param decoder
 * @param stream
 * @return int
 */
int openDecoder(AVCodecContext **decCtx, AVCodec **decoder, AVStream *stream);

int initEncoder(AVCodecContext **encCtx, AVCodec **encoder, int codecId);
int openEncoder(AVCodecContext **encCtx, AVCodec **encoder,
                AVCodecParameters *codecpar, AVDictionary **opt);
#endif