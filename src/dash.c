#include "dash.h"

static int openDash(DashCtxT* data, AVCodecContext** encoderCtx) {
  int ret, i;
  AVDictionary* opts = NULL;
  data->dashOutCtx = NULL;

  ret = avformat_alloc_output_context2(&data->dashOutCtx, NULL, "dash",
                                       data->dashIndexPath);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openDash::could not open output\n");
    goto end;
  }

  for (i = 0; i < data->streamLen; i++) {
    data->dashStreams[i] = avformat_new_stream(data->dashOutCtx, NULL);
  }

  // check if all have been setup properly
  for (i = 0; i < data->streamLen; i++) {
    if (!data->dashStreams[i]) {
      av_log(NULL, AV_LOG_ERROR, "openDash::could not create stream %i\n", i);
      goto closeOutput;
    }
  }

  //   if (!data->dashOutCtx->flags & AVFMT_NOFILE) {
  //     ret =
  //         avio_open(&data->dashOutCtx->pb, data->dashIndexPath,
  //         AVIO_FLAG_WRITE);

  //     if (ret < 0) {
  //       goto closeOutput;
  //     }
  //   }

  for (i = 0; i < data->streamLen; i++) {
    ret = avcodec_parameters_from_context(data->dashStreams[i]->codecpar,
                                          encoderCtx[i]);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR,
             "openDash::could not setup video codec params for stream: %i\n",
             i);
      goto closeOutput;
    }
  }

  av_dict_set(&opts, "init_seg_name", "init$RepresentationID$.$ext$", 0);
  av_dict_set(&opts, "media_seg_name", "$RepresentationID$.$Number%05d$.$ext$",
              0);
  av_dict_set(&opts, "use_template", "1", 0);
  av_dict_set(&opts, "use_timeline", "1", 0);
  av_dict_set(&opts, "seg_duration", "4", 0);
  av_dict_set(&opts, "hls_playlist", "1", 0);
  av_dict_set(&opts, "window_size", "15", 0);
  av_dict_set(&opts, "adaptation_sets", "id=0,streams=v id=1,streams=a", 0);

  ret = avformat_write_header(data->dashOutCtx, &opts);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "openRtmp::could not write rtmp out header\n");
    goto closeOutput;
  }

  return 0;

closeOutput:
  closeOutput(&data->dashOutCtx);
end:
  data->dashOutCtx = NULL;
  return ret;
}

int startDash(DashCtxT* data, AVCodecContext** encoderCtx) {
  int ret;
  data->dashOutCtx = NULL;

  data->dashStreams = malloc(data->streamLen * sizeof(AVStream*));
  if (!data->dashStreams) {
    av_log(NULL, AV_LOG_ERROR,
           "startDash::not able to allocate stream memory\n");
    return AVERROR(ENOMEM);
  }

  ret = openDash(data, encoderCtx);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "startDash::could not open dash. Ret = %i\n",
           ret);
    return ret;
  }
}

void dashWritePacket(DashCtxT* data, AVPacket* packet) {
  int ret;
  ret = av_interleaved_write_frame(data->dashOutCtx, packet);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "dashWritePacket::could not write to mux\n");
  }
}
