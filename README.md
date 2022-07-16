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
