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

=========================================================
diff --git a/configure b/configure
index d77a55b653..e6aa327a05 100755
--- a/configure
+++ b/configure
@@ -5004,7 +5004,7 @@ probe_cc(){
    elif $_cc -v 2>&1 | grep -q clang && ! $_cc -? > /dev/null 2>&1; then
        _type=clang
        _ident=$($_cc --version 2>/dev/null | head -n1)
-        _depflags='-MMD -MF $(@:.o=.d) -MT $@'
+        _depflags='-MMD -MF $(@:.o=.d) -MT $@ -MJ $@.json'
        _cflags_speed='-O3'
        _cflags_size='-Oz'
        elif $_cc -V 2>&1 | grep -q Sun; then
========================================================

