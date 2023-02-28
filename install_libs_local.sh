mkdir -p libs/ffmpeg_build

cd libs

# git clone https://github.com/eclipse/paho.mqtt.c.git paho
# cd paho
# git checkout v1.3.12
# make
# sudo make install

cd ~/ffmpeg_sources &&
    wget -nc https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.bz2 &&
    tar xjvf nasm-2.15.05.tar.bz2 &&
    cd nasm-2.15.05 &&
    ./autogen.sh &&
    PATH="$HOME/bin:$PATH" ./configure --prefix="$HOME/ffmpeg_build" --bindir="$HOME/bin" &&
    make &&
    make install

cd ~/ffmpeg_sources &&
    git -C x264 pull 2>/dev/null || git clone --depth 1 https://code.videolan.org/videolan/x264.git &&
    cd x264 &&
    PATH="$HOME/bin:$PATH" PKG_CONFIG_PATH="$HOME/ffmpeg_build/lib/pkgconfig" ./configure --prefix="$HOME/ffmpeg_build" --bindir="$HOME/bin" --enable-static --enable-pic &&
    PATH="$HOME/bin:$PATH" make &&
    make install

git clone https://github.com/FFmpeg/FFmpeg.git ffmpeg-5.1
cd ffmpeg-5.1
git checkout release/5.1

PATH="$HOME/bin:$PATH" PKG_CONFIG_PATH="$HOME/ffmpeg_build/lib/pkgconfig" ./configure \
    --prefix="$HOME/ffmpeg_build" \
    --pkg-config-flags="--static" \
    --extra-cflags="-I$HOME/ffmpeg_build/include" \
    --extra-ldflags="-L$HOME/ffmpeg_build/lib" \
    --extra-libs="-lpthread -lm" --ld="g++" --enable-gpl --enable-gnutls --enable-libx264

make
make install

sudo apt-get install libjansson-dev -y
