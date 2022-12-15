#!/bin/bash
source ./config.sh

rm -rf ./dash
rm -rf rec

mkdir dash
mkdir rec

../ums -mode live \
    -rtmpIn rtmp://localhost:1935/live/input \
    -rtmpOut rtmp://localhost/live/output \
    -dash ./dash/index.mpd \
    -rec ./rec/rec.ts \
    -preFiller $TEST_BIN_PATH/english-pre-filler.jpg \
    -sessionFiller $TEST_BIN_PATH/english-session-filler.jpg \
    -postFiller $TEST_BIN_PATH/english-post-filler.jpg \
    -sessionStart "2022-12-13T14:50:00.000+05:30"
#Todo: to make the filler logic work we have to specify streamSTart, sessionStart and sessionEnd
#      parameters as well

#    -rtmpOut rtmp://localhost/live/output \
    #-rtmpIn rtmp://localhost:1935/live/input \
