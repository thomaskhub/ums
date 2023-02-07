CC=gcc
OBJECTS := src/main.c  src/mux.c src/rtmpInput.c src/output.c src/utils.c src/avBuffer.c src/inputSwitch.c src/filters.c src/dash.c src/audioEncoder.c src/mqtt.c

build: 
	$(CC) $(OBJECTS) -o ums  \
	-lpthread `pkg-config --libs libavcodec libavutil libavformat libavfilter libswresample libswscale --cflags` \
	-ljansson \
	-lpaho-mqtt3c 

debug: 
	$(CC) -O0 -g $(OBJECTS) -o ums  \
	-lpthread `pkg-config --libs libavcodec libavutil libavformat libavfilter libswresample libswscale --cflags` \
	-ljansson \
	-lpaho-mqtt3c 
	

