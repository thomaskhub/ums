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
git -C fdk-aac pull 2>/dev/null || git clone --depth 1 https://github.com/mstorsjo/fdk-aac
cd fdk-aac
autoreconf -fiv
./configure --prefix="$BUILD" --disable-shared
make
make install

cd $SOURCE
wget -O ffmpeg-4.4.tar.bz2 https://ffmpeg.org/releases/ffmpeg-4.4.tar.bz2
tar xjvf ffmpeg-4.4.tar.bz2
cd ffmpeg-4.4
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
