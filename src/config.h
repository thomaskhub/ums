/**
* Copyright (C) 2022  Thomas Kinder
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

#ifndef __MEDIA_CONFIG___
#define __MEDIA_CONFIG___

#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 720
#define VIDEO_FRAME_RATE 25
#define VIDEO_TIMEBASE_NUM 1
#define VIDEO_TIMEBASE_DEN 1000
#define VIDEO_PTS_OFF (uint32_t)(VIDEO_TIMEBASE_DEN / VIDEO_FRAME_RATE)
#define VIDEO_PIX_FMT AV_PIX_FMT_YUV420P
#define AUDIO_SAMPLE_FMT AV_SAMPLE_FMT_FLTP
#define AUDIO_NB_SAMPLES 1024
#define AUDIO_RATE 44100
#define AUDIO_CH_LAYOUT AV_CH_LAYOUT_MONO
#define AUDIO_PTS_OFF (uint32_t)(VIDEO_TIMEBASE_DEN / AUDIO_RATE)

// 44 frames with 1024 samples each ->
#define AUDIO_BUF_FRAME_CNT 45

#define FILLER_VIDEO_FILTER "scale=1280:720,format=yuv420p"

#define RTMPIN_VIDEO_FILTER "scale=1280:720,format=yuv420p,fps=fps=25"
#define RTMPIN_AUDIO_FILTER "aresample=44100,aformat=channel_layouts=mono"

#endif
