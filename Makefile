.PHONY: test-*

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
	
test-video:
	./ums --rtmpIn ./test/data/short.mp4 \
		--rtmpOut rtmp://nginx-rtmp/live/output \
		--dash ./test/dash \
		--rec ./test/rec \
		--preFiller ./test/data/english-pre-filler.jpg \
		--sessionFiller ./test/data/english-session-filler.jpg \
		--postFiller ./test/data/english-post-filler.jpg

test-live:
	./ums --rtmpIn rtmp://nginx-rtmp:1935/live/input \
		--rtmpOut rtmp://nginx-rtmp/live/output \
		--dash ./test/dash \
		--rec ./test/rec \
		--preFiller ./test/data/english-pre-filler.jpg \
		--sessionFiller ./test/data/english-session-filler.jpg \
		--postFiller ./test/data/english-post-filler.jpg
