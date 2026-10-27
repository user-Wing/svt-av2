#!/usr/bin/env bash
set -Eeuo pipefail
src=/mnt/c/Private/ffmpeg-native/deps-full/AV2/svt-av2
base=/root/svt-av2-m4-ffmpeg-min
out="$src/dist/M4-ffmpeg-min"
mkdir -p "$base" "$out/build-info" "$src/artifacts/m4-ffmpeg-min"
exec > >(tee "$src/artifacts/m4-ffmpeg-min/build.log") 2>&1
export PATH=/usr/local/llvm-mingw/bin:/usr/local/bin:/usr/bin:/bin
if [[ ${1:-} != --ffmpeg-only ]]; then
python3 "$src/tools/ffmpeg-min/prepare.py" "$base"
mkdir -p "$base/src-svt"
tar -C "$src" -cf - CMakeLists.txt cmake include Source app | tar -C "$base/src-svt" -xf -
if [[ ! -f "$base/src-avm/CMakeLists.txt" ]]; then
 mkdir -p "$base/src-avm"
 tar -xzf "$src/artifacts/m4-a/debug-oracle-source/source.tar.gz" --strip-components=1 -C "$base/src-avm"
fi
common=(-G Ninja -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64
 -DCMAKE_C_COMPILER=x86_64-w64-mingw32-clang -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-clang++
 -DCMAKE_AR=/usr/local/llvm-mingw/bin/llvm-ar -DCMAKE_RANLIB=/usr/local/llvm-mingw/bin/llvm-ranlib
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$base/prefix")
cmake -S "$base/src-svt" -B "$base/build-svt" "${common[@]}" -DSVT_AV2_BUILD_TESTS=OFF -DSVT_AV2_BUILD_SHARED=OFF
cmake --build "$base/build-svt" --parallel 12
mkdir -p "$base/prefix/lib/pkgconfig" "$base/prefix/include"
cp "$base/src-svt/include/svt_av2.h" "$base/prefix/include/"
{
 echo "CREATE $base/prefix/lib/libSvtAv2.a"
 for lib in "$base/build-svt/lib/"*.a; do echo "ADDLIB $lib"; done
 echo SAVE
 echo END
} | llvm-ar -M
cat > "$base/prefix/lib/pkgconfig/SvtAv2.pc" <<EOF
prefix=$base/prefix
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: SvtAv2
Description: SVT-AV2 M4 static encoder
Version: 0.0.4
Libs: -L\${libdir} -lSvtAv2
Cflags: -I\${includedir}
EOF
cmake -S "$base/src-avm" -B "$base/build-avm" "${common[@]}" \
 -DAVM_TARGET_CPU=generic -DCONFIG_AV2_ENCODER=0 -DCONFIG_AV2_DECODER=1 \
 -DCONFIG_MULTITHREAD=0 -DENABLE_TESTS=0 -DENABLE_EXAMPLES=0 -DENABLE_TOOLS=0 \
 -DENABLE_DOCS=0 -DENABLE_NASM=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build "$base/build-avm" --parallel 12
cmake --install "$base/build-avm"
fi
export PKG_CONFIG_LIBDIR="$base/prefix/lib/pkgconfig"
mkdir -p "$base/build-ffmpeg"
cd "$base/build-ffmpeg"
if [[ ${1:-} != --ffmpeg-only || ! -f ffbuild/config.mak ]]; then
"$base/src-ffmpeg/configure" --prefix="$base/prefix" --arch=x86_64 --target-os=mingw32 \
 --enable-cross-compile --cc=x86_64-w64-mingw32-clang --cxx=x86_64-w64-mingw32-clang++ \
 --ar=llvm-ar --ranlib=llvm-ranlib --nm=llvm-nm --pkg-config=pkg-config --pkg-config-flags=--static \
 --extra-cflags="-I$base/prefix/include" --extra-ldflags="-L$base/prefix/lib -static" \
 --disable-autodetect --disable-everything --disable-network --disable-doc --disable-debug \
 --disable-x86asm --enable-small --enable-static --disable-shared --enable-ffmpeg --enable-ffprobe \
 --enable-libsvtav2 --enable-libavm --enable-encoder=libsvtav2,rawvideo,wrapped_avframe \
 --enable-decoder=libavm_av2,rawvideo --enable-demuxer=yuv4mpegpipe,rawvideo,ivf \
 --enable-muxer=ivf,rawvideo,yuv4mpegpipe,null --enable-filter=scale,format,null \
 --enable-protocol=file,pipe
fi
make -j12
cp ffmpeg.exe ffprobe.exe "$out/"
cp "$base/build-svt/bin/SvtAv2EncApp.exe" "$out/build-info/"
cp ffbuild/config.mak config.h config_components.h "$out/build-info/"
cp "$base/ffmpeg-baseline.json" "$out/build-info/"
llvm-objdump -p ffmpeg.exe > "$out/build-info/ffmpeg-pe.txt"
llvm-objdump -p ffprobe.exe > "$out/build-info/ffprobe-pe.txt"
mkdir -p "$out/source-patches" "$out/licenses"
cp "$src/tools/ffmpeg-min/"* "$out/source-patches/"
: > "$out/source-patches/libsvtav2.patch"
for name in configure libavcodec/Makefile libavcodec/allcodecs.c libavcodec/libavmdec.c; do
 diff --strip-trailing-cr -u "/mnt/c/Private/ffmpeg-native/ffmpeg-src/$name" "$base/src-ffmpeg/$name" >> "$out/source-patches/libsvtav2.patch" || [[ $? == 1 ]]
done
cp "$base/src-ffmpeg/COPYING.LGPLv2.1" "$out/licenses/FFmpeg-LGPLv2.1.txt"
cp "$base/src-avm/LICENSE" "$out/licenses/AVM-LICENSE.txt"
cp "$base/src-avm/PATENTS" "$out/licenses/AVM-PATENTS.txt"
cp "$src/LICENSE" "$out/licenses/SVT-AV2-LICENSE-status.txt"
echo BUILD_COMPLETE
