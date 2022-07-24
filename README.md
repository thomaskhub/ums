# Introduction - Uninterruptible Media Server

This media server is capable of reading RTMP streams comming from
tools like [OBS - Open Brodacaster Studio](https://obsproject.com/) and
outputs it as DASH/HLS/RTMP formats. It can also record the incoming video
as an MPEGTS video file.

The main purpose of this media player is to implement a very stable platform
for low bandwidht scenarios as they can happen in countries like India or other non
developed countries. The focus is on a reliable video streaming platform
where content should not get interrupted, or recovers automatically in failure
states , without user interaction. It is intended to be able to run even on a
180kbps connection.

It is not been designed to deliver maximum video and audio quality. If high quality
streaming is required other open source implementations might be a better choice.
E.g. the ffmpeg command line tool could be used directly for this.

# Installation

## Install dependencies

```bash
#install dependencies for ubuntu 20.04 (might work for other versions also)
cd ./lib/ffmpeg/ubuntu-20.04.sh
```

## Compile FFMPEG-libav from source

This project is based on the FFMPEG libraries which need to be compiled. A
script compatible with Ubuntu 20.04 is available and needs to be executed
first to build the right FFMPEG libraries and components from sourc code.

```bash
cd ./lib/ffmpeg
./compile.sh
```

## Setup visual studio code build / debug environment

1. Install the following packets in visual studio code:

- [C/C++](https://code.visualstudio.com/docs/languages/cpp)
- [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)
- [VS Code Makefile tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.makefile-tools)

You can refer to this link in order to configure the Makefile tool to run / debug the application
[Makefile Tool Tutorial](https://devblogs.microsoft.com/cppblog/now-announcing-makefile-support-in-visual-studio-code/)

2. Setup the Clang config for automatic formating

```
file -> settings -> Extensions --> C/C++ --> Clang_format_fallback Style = google
```

3. Setup make file in vscode for debugging or production respectively
   ![MakefileSettingsDebug](doc/images/makefile-debug.png)
   ![MakefileSettingsDebug](doc/images/makefile-build.png)

# Start RTMP docker

```bash
cd docker
./build.sh

docker run -d -p 1935:1935 --name nginx-rtmp nginx-rtmp
```

# System description

**Work in progress**
![System](./doc/images/media-platform-ums.jpg)

- System is connected to an rtmp server
- the rtmp input is first normalized to 720p with 25fps and YUV420p pixel format
- then the rtmp input video is sliced into 1 second chunks
  - 1 seconds chunk to allow easy switching between filler videos and RTMP input as AV sync will be easy to maintain
- the rtmp inputSwitch.c will read the rtmp chunk buffer and forward the frames to the output modules.
  - If rtmp input goes down the switch uses preloaded frames from a jpg file instead so that stream never interrupts.
  - three different fillers are being played depending on the wallclock time and the event configuration
- the main output module will forward the video files to another RMTP server (16-07-2022 Implemented until here)
  - it can also optionaly store the video chunks in a MPEGTS file
  - and it also forwards it the the Dash muxer
- As Dash/HLS are ABR protocols each output needs to create different bandwidth and resolutions.

  - x264 encoder is implemented in the output.c file.

  # Implementation Status

  Currently the input switching is implemented where the system plays filler if the RTMP input goes down, other wise it plays the RTMP input source. From there
  it pushes the data to an RTMP output. Currently only the video path is implemented.

  Next things to do:

  - test mpegts recording in main output module
  - make parameters defined in main.c file changable with command line arguments
  - implement the Dash / HLS output modules and the Dash muxer
  - implement the audio path
