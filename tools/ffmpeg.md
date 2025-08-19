% cd ~/ffmpeg/
% git clone --branch n7.1 https://git.ffmpeg.org/ffmpeg.git ffmpeg_src

% mkdir ~/ffmpeg/build
% mkdir ~/ffmpeg/installed
% cd ~/ffmpeg/build

../ffmpeg_src/configure --disable-programs --disable-doc --enable-network --disable-shared --enable-static \
   --sysroot="$(xcrun --sdk iphoneos --show-sdk-path)" \
   --enable-cross-compile \
   --arch=arm64 \
   --prefix=../installed \
   --cc="xcrun --sdk iphoneos clang -arch arm64" \
   --cxx="xcrun --sdk iphoneos clang++ -arch arm64" \
   --extra-ldflags="-miphoneos-version-min=16.0" \
   --install-name-dir='@rpath' \
   --disable-audiotoolbox

../ffmpeg_src/configure --disable-programs --disable-doc --enable-network --disable-shared --enable-static \
   --sysroot="$(xcrun --sdk iphoneos --show-sdk-path)" \
   --enable-cross-compile \
   --arch=arm64 \
   --prefix=../installed \
   --cc="xcrun --sdk iphoneos clang -arch arm64" \
   --cxx="xcrun --sdk iphoneos clang++ -arch arm64" \
   --extra-ldflags="-miphoneos-version-min=16.0" \
   --install-name-dir='@rpath' \
   --disable-audiotoolbox
