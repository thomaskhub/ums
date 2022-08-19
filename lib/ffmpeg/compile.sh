WORKDIR=$(pwd)
SOURCE="$WORKDIR/ffmpeg_sources"
BIN="$WORKDIR/bin"
BUILD="$WORKDIR/ffmpeg_build"

mkdir -p $SOURCE $BIN

cd $SOURCE
wget https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.bz2
tar xjvf nasm-2.15.05.tar.bz2
cd nasm-2.15.05
./autogen.sh
PATH="$BIN:$PATH" ./configure --prefix="$BUILD" --bindir="$BIN"
make
make install

cd $SOURCE
git -C x264 pull 2>/dev/null || git clone --depth 1 https://code.videolan.org/videolan/x264.git
cd x264
PATH="$BIN:$PATH" PKG_CONFIG_PATH="$BUILD/lib/pkgconfig" ./configure --prefix="$BUILD" --bindir="$BIN" --enable-static --enable-pic
PATH="$BIN:$PATH" make
make install

cd $SOURCE
# wget -O ffmpeg-4.4.tar.bz2 https://ffmpeg.org/releases/ffmpeg-4.4.tar.bz2
# tar xjvf ffmpeg-4.4.tar.bz2
#
# We where facing the issue in AAC that coeeficient is near INF. We found as
# one solution which worked. Not the nicest way to do this but at least the app
# seems to be running nicely.
#
# https://github.com/FreeRDP/FreeRDP/issues/6072
#
git clone https://github.com/thomaskhub/FFmpeg.git ffmpeg-4.4
cd ffmpeg-4.4
git checkout release/4.4-acc-near-inf-error

PATH="$BIN:$PATH" PKG_CONFIG_PATH="$BUILD/lib/pkgconfig" ./configure \
    --prefix="$BUILD" \
    --pkg-config-flags="--static" \
    --extra-cflags="-I$BUILD/include" \
    --extra-ldflags="-L$BUILD/lib" \
    --extra-libs="-lpthread -lm" \
    --ld="g++" \
    --bindir="$BIN" \
    --enable-gpl \
    --enable-gnutls \
    --enable-libx264 \
    --extra-libs="-lpthread"

PATH="$BIN:$PATH" make
make install
hash -r
