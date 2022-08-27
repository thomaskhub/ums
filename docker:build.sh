#!/bin/sh
pkg-config --libs libavcodec libavutil libavformat libavfilter libswresample libswscale --cflags
#export PKG_CONFIG_PATH=/source/lib/ffmpeg/ffmpeg_build/lib/pkgconfig
make
