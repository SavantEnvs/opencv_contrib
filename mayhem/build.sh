#!/usr/bin/env bash
#
# mayhem/build.sh — build the fuzz harnesses and the functional-test probe for opencv_contrib.
#
# opencv_contrib modules only build inside OpenCV's own build system. The Dockerfile builds OpenCV proper
# once (uninstrumented, static) into /opt/toolchains/deps/opencv-5.x-71babee3; here the contrib modules
# under test are compiled straight from modules/<m>/src with the fuzz flags and linked against it, so the
# fuzzed code is the contrib code. Offline and re-runnable: nothing is fetched, objects are only rebuilt
# when their source is newer.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

OCV=/opt/toolchains/deps/opencv-5.x-71babee3
OCV_INC="$OCV/include/opencv5"
MODS="line_descriptor wechat_qrcode img_hash ximgproc"
BUILD="$SRC/mayhem/build-fuzz"      # sanitized contrib modules + harness objects
NBUILD="$SRC/mayhem/build-normal"   # contrib modules with the project's normal flags, for the test probe
mkdir -p "$BUILD" "$NBUILD"

# Installed headers, then the private headers (core/private.hpp, ...) that only the OpenCV source tree has.
INCS=("-I$OCV_INC" "-I$SRC/mayhem/include")
for m in core imgproc geometry features objdetect dnn video stereo calib flann photo imgcodecs; do
  INCS+=("-I$OCV/src/modules/$m/include")
done
for m in $MODS; do INCS+=("-I$SRC/modules/$m/include"); done

# The fuzz build needs SanCov on the library code (not only the harness TU), and no leak detection
# (lsan_off.cc). -fsanitize=fuzzer-no-link is unconditional so an empty SANITIZER_FLAGS still records edges.
FUZZ_CXXFLAGS="-std=c++17 -O1 $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
NORM_CXXFLAGS="-std=c++17 -O2 $COVERAGE_FLAGS"

# compile_module <m> <objdir> <flags...>: every modules/<m>/src/**/*.cpp -> <objdir>/<m>/*.o -> lib<m>.a
compile_module() {
  local m="$1" od="$2"; shift 2
  local flags="$*"
  mkdir -p "$od/$m"
  local srcs=()
  while IFS= read -r f; do srcs+=("$f"); done < <(find "modules/$m/src" -name '*.cpp' | LC_ALL=C sort)
  printf '%s\n' "${srcs[@]}" | xargs -P "$MAYHEM_JOBS" -I{} bash -c '
    f="{}"; o="'"$od/$m"'/$(echo "${f#modules/'"$m"'/src/}" | tr / _).o"
    if [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then
      '"$CXX"' '"$flags"' '"${INCS[*]}"' -D__OPENCV_BUILD=1 -I"modules/'"$m"'/src" -c "$f" -o "$o.tmp" && mv "$o.tmp" "$o"
    fi'
  rm -f "$od/lib$m.a"
  ar rcs "$od/lib$m.a" "$od/$m"/*.o
}

for m in $MODS; do
  compile_module "$m" "$BUILD" $FUZZ_CXXFLAGS
done

OCV_LIBS=("$OCV"/lib/libopencv_{stereo,video,photo,objdetect,dnn,features,geometry,calib,imgcodecs,imgproc,flann,core}.a)
THIRD=("$OCV"/lib/opencv5/3rdparty/*.a)
SYSLIBS=(-ldl -lpthread -lrt -lm)

# Plain-C run-once driver and the leak-sanitizer opt-out, shared by every harness.
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$BUILD/standalone_main.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$BUILD/lsan_off.o"

# harness name -> contrib modules it links
declare -A HARNESS_MODS=(
  [fuzz_edlines]="line_descriptor"
  [fuzz_lsd]="line_descriptor"
  [fuzz_wechat_qrcode]="wechat_qrcode"
  [fuzz_img_hash]="img_hash"
  [fuzz_ximgproc]="ximgproc"
)
for h in "${!HARNESS_MODS[@]}"; do
  mlibs=()
  for m in ${HARNESS_MODS[$h]}; do mlibs+=("$BUILD/lib$m.a"); done
  $CXX $FUZZ_CXXFLAGS "${INCS[@]}" -Imayhem/harnesses -c "mayhem/harnesses/$h.cc" -o "$BUILD/$h.o"
  link=("${mlibs[@]}" -Wl,--start-group "${OCV_LIBS[@]}" "${THIRD[@]}" -Wl,--end-group "${SYSLIBS[@]}")
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$BUILD/$h.o" "$BUILD/lsan_off.o" "${link[@]}" -o "/mayhem/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$BUILD/$h.o" "$BUILD/standalone_main.o" "$BUILD/lsan_off.o" "${link[@]}" -o "/mayhem/$h-standalone"
done

# Functional-test probe: the same contrib modules with normal flags (no sanitizers, no SanCov).
for m in $MODS; do
  compile_module "$m" "$NBUILD" $NORM_CXXFLAGS
done
$CXX $NORM_CXXFLAGS "${INCS[@]}" mayhem/kat.cc \
  "$NBUILD/libwechat_qrcode.a" "$NBUILD/libline_descriptor.a" "$NBUILD/libimg_hash.a" "$NBUILD/libximgproc.a" \
  -Wl,--start-group "${OCV_LIBS[@]}" "${THIRD[@]}" -Wl,--end-group "${SYSLIBS[@]}" \
  -o "$NBUILD/kat"
