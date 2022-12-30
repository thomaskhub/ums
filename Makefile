export PKG_CONFIG_PATH=./lib/ffmpeg/ffmpeg_build/lib/pkgconfig

CC=gcc
OBJECTS := src/main.c  src/mux.c src/rtmpInput.c src/output.c src/utils.c src/avBuffer.c src/inputSwitch.c src/filters.c src/dash.c src/audioEncoder.c

build: 
	$(CC) $(OBJECTS) -o ums  \
	-lpthread `pkg-config --libs libavcodec libavutil libavformat libavfilter libswresample libswscale --cflags` \
	-L/tmp/paho/build/output -l paho-mqtt3a

debug: 
	$(CC) -O0 -g $(OBJECTS) -o ums  \
	-L/tmp/paho/build/output -l paho-mqtt3a
	-lpthread `pkg-config --libs libavcodec libavutil libavformat libavfilter libswresample libswscale --cflags`

