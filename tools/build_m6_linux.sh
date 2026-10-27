#!/usr/bin/env bash
set -Eeuo pipefail
src=${1:?source snapshot}
build=${2:?fresh build directory}
kind=${3:?GCC, Clang or SAN}
logs=${4:?log directory}
export PATH=/usr/local/bin:/usr/bin:/bin
case "$kind" in
 GCC) cc=/usr/bin/gcc; type=Release; san=OFF ;;
 Clang) cc=/usr/local/bin/clang-linux; type=Release; san=OFF ;;
 SAN) cc=/usr/bin/gcc; type=Debug; san=ON ;;
 *) exit 2 ;;
esac
mkdir -p "$logs"
cmake -S "$src" -B "$build" -G Ninja -DCMAKE_C_COMPILER="$cc" \
 -DCMAKE_BUILD_TYPE="$type" -DSVT_AV2_WARNINGS_AS_ERRORS=ON \
 -DSVT_AV2_ENABLE_SANITIZERS="$san" > "$logs/$kind-configure.log" 2>&1
cmake --build "$build" --parallel 4 > "$logs/$kind-build.log" 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
 ctest --test-dir "$build" --output-on-failure --timeout 300 > "$logs/$kind-ctest.log" 2>&1
tail -5 "$logs/$kind-ctest.log"
