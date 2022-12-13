/**
* Copyright (C) 2022  The World
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.

* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/
#ifndef __MUX_H__
#define __MUX_H__

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <unistd.h>

int openInput(AVFormatContext **inFmtCtx, char *filename, AVStream **audioStream,
              AVStream **videoStream, AVInputFormat *fmt, AVDictionary **opts);

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

/**
 * @brief initialize an encoder object
 *
 * @param encCtx
 * @param encoder
 * @param codecId
 * @return int
 */
int initEncoder(AVCodecContext **encCtx, AVCodec **encoder, int codecId);

/**
 * @brief open the previously initilaized encoder
 *
 * @param encCtx
 * @param encoder
 * @param codecpar
 * @param opt
 * @return int
 */
int openEncoder(AVCodecContext **encCtx, AVCodec **encoder,
                AVCodecParameters *codecpar, AVDictionary **opt);
#endif