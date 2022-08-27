#include "mux.h"

void closeInput(AVFormatContext **inFmtCtx) { avformat_close_input(inFmtCtx); }

int openInput(AVFormatContext **inFmtCtx, char *filename,
              AVStream **audioStream, AVStream **videoStream) {
  int ret, i;
  int foundAudio = 0;
  int foundVideo = 0;

  (*inFmtCtx) = avformat_alloc_context();
  if (!(*inFmtCtx)) {
    return AVERROR(ENOMEM);
  }

  ret = avformat_open_input(inFmtCtx, filename, 0, 0);
  if (ret < 0) {
    closeInput(inFmtCtx);
    return ret;
  }

  ret = avformat_find_stream_info((*inFmtCtx), 0);
  if (ret < 0) {
    closeInput(inFmtCtx);
    return ret;
  }

  *audioStream = NULL;
  *videoStream = NULL;
  // We extract first video and audio other files will not be supported
  for (i = 0; i < (*inFmtCtx)->nb_streams; i++) {
    if ((*inFmtCtx)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      *audioStream = (*inFmtCtx)->streams[i];
      foundAudio = 1;
    } else if ((*inFmtCtx)->streams[i]->codecpar->codec_type ==
               AVMEDIA_TYPE_VIDEO) {
      *videoStream = (*inFmtCtx)->streams[i];
      foundVideo = 1;
    }
    if (foundAudio && foundVideo) {
      break;
    }
  }

  if (!audioStream || !videoStream) {
    closeInput(inFmtCtx);
    return AVERROR_STREAM_NOT_FOUND;
  }

  return 0;
}

void closeOutput(AVFormatContext **outFmtCtx) {
  avformat_free_context(*outFmtCtx);
}

int openOutput(AVFormatContext **outFmtCtx, char *filename,
               AVStream **audioStream, AVStream **videoStream,
               const char *formatName) {
  int ret;

  avformat_alloc_output_context2(outFmtCtx, NULL, formatName, filename);
  if (!(*outFmtCtx)) {
    return AVERROR(ENOMEM);
  }

  (*videoStream) = avformat_new_stream((*outFmtCtx), NULL);
  if (!(*videoStream)) {
    closeOutput(outFmtCtx);
    return AVERROR(ENOMEM);
  }

  (*audioStream) = avformat_new_stream((*outFmtCtx), NULL);
  if (!(*audioStream)) {
    closeOutput(outFmtCtx);
    return AVERROR(ENOMEM);
  }

  if (!((*outFmtCtx)->flags & AVFMT_NOFILE)) {
    ret = avio_open(&(*outFmtCtx)->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
      closeOutput(outFmtCtx);
      return AVERROR(ENOENT);
    }
  }

  return 0;
}

void closeCodec(AVCodecContext **codec) { avcodec_free_context(codec); }

int openDecoder(AVCodecContext **decCtx, AVCodec **decoder, AVStream *stream) {
  int ret;
  (*decoder) = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!(*decoder)) {
    return AVERROR_DECODER_NOT_FOUND;
  }

  (*decCtx) = avcodec_alloc_context3((*decoder));
  if (!(*decCtx)) {
    return AVERROR(ENOMEM);
  }

  avcodec_parameters_to_context((*decCtx), stream->codecpar);

  ret = avcodec_open2((*decCtx), (*decoder), NULL);
  if (ret < 0) {
    avcodec_free_context(decCtx);
    return ret;
  }

  return 0;
}

int initEncoder(AVCodecContext **encCtx, AVCodec **encoder, int codecId) {
  (*encoder) = avcodec_find_encoder(codecId);
  if (!(*encoder)) {
    return AVERROR_UNKNOWN;
  }

  (*encCtx) = avcodec_alloc_context3((*encoder));
  if (!(*encCtx)) {
    return AVERROR_UNKNOWN;
  }

  return 0;
}

int openEncoder(AVCodecContext **encCtx, AVCodec **encoder,
                AVCodecParameters *codecpar, AVDictionary **opt) {
  int ret = 0;
  ret = avcodec_open2((*encCtx), (*encoder), opt);
  if (ret < 0) {
    return ret;
  }

  ret = avcodec_parameters_from_context(codecpar, (*encCtx));
  if (ret < 0) {
    return ret;
  }

  return 0;
}