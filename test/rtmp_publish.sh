#!/bin/bash
source ./config.sh
$FFMPEG -re -i $TEST_BIN_PATH/001.mp4 -c copy -f flv rtmp://localhost/live/input &
