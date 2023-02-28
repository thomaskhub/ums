CXX=g++
FFMPEG_LIBS= 	libavcodec \
				libavutil \
				libavfilter \
				libswresample \
				libswscale \


CXXFLAGS := $(shell pkg-config --cflags $(FFMPEG_LIBS)) $(CFLAGS)
LDLIBS := $(shell pkg-config --libs $(FFMPEG_LIBS)) $(LDLIBS)

OBJECTS := src/main.cpp src/config.cpp src/input.cpp  src/rtmpInput.cpp src/decoder.cpp src/filter.cpp src/utils.cpp  \
			src/encoder.cpp src/output.cpp

all: 
	$(CXX) $(OBJECTS) -Wno-deprecated-declarations -o ums $(CXXFLAGS) $(LDLIBS) \
		-ljansson \
		-lpaho-mqtt3c 

debug: 
	$(CXX) -O0 -g $(OBJECTS) -o ums $(CXXFLAGS) $(LDLIBS) \
		-ljansson \
		-lpaho-mqtt3c 

clean: 
	rm ums

