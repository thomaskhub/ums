#
# Simple test that takes a video pushes it with ffmpeg to
# the local rtmp server and displays the video with mpv player.
# If the video is visible it means that rtmp is setup correctly
# and that test shoul be able to run.
#
source ./config.sh
VIDEO_001="$TEST_BIN_PATH/001.mp4"

$FFMPEG -re -i $VIDEO_001 -c copy -f flv rtmp://localhost/live/input &

mpv rtmp://localhost/live/input
