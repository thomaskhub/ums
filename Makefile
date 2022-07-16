export PKG_CONFIG_PATH=./lib/ffmpeg/ffmpeg_build/lib/pkgconfig

CC=gcc
OBJECTS := src/main.c  src/mux.c src/rtmpInput.c src/output.c src/utils.c src/videoBuffer.c src/inputSwitch.c src/filters.c

build: 
	$(CC) $(OBJECTS) -o ums  \
	-lpthread `pkg-config --libs libavcodec libavutil libavformat libavfilter libswresample libswscale --cflags`

debug: 
	$(CC) -g $(OBJECTS) -o ums  \
	-lpthread `pkg-config --libs libavcodec libavutil libavformat libavfilter libswresample libswscale --cflags`

