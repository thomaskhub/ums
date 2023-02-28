WORKDIR=$(pwd)
SOURCE="$WORKDIR/ffmpeg_sources"
BIN="$WORKDIR/bin"
BUILD="$WORKDIR/ffmpeg_build"

cd $SOURCE/ffmpeg-4.4
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
