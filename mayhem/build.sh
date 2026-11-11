#!/usr/bin/env bash
#
# mayhem/build.sh — build the VBA-M emulator core, the fuzz harnesses and the test oracle.
#
# Runs inside the commit image (mayhem/Dockerfile) as `mayhem` in /mayhem, offline. The upstream
# CMake build drags in git/submodule/vcpkg/wx/SDL; none of that is needed for the headless core
# (src/core: gb + gba + apu + base + fex), so its translation units are compiled directly with the
# same definitions cmake/Toolchain.cmake uses (C core, no link cable, debugger on).
#
#   /mayhem/fuzz_<t>              libFuzzer binary (sanitized core, DWARF 3)
#   /mayhem/fuzz_<t>-standalone   same harness behind the run-once driver
#   /mayhem/mayhem-build/test/    NORMAL-flag core + test programs that mayhem/test.sh runs
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

CORE=src/core
B="$SRC/mayhem-build"
rm -rf "$B"
mkdir -p "$B/gen/core/base" "$B/fuzz" "$B/test/bin"

# core/base/version_gen.h (what base/build-version.cmake produces)
sed -e 's/@VBAM_VERSION@/0.0.0/' \
    -e 's/@VBAM_VERSION_RELEASE@/1/' \
    -e 's/@VBAM_REVISION@/mayhem/' \
    -e 's/@VBAM_WIN_VERSION@/0,0,0,0/' \
    "$CORE/base/version_gen.h.in" > "$B/gen/core/base/version_gen.h"

# ---- core translation units ------------------------------------------------------------
{
  ls $CORE/apu/*.cpp
  ls $CORE/gb/*.cpp
  ls $CORE/gba/*.cpp $CORE/gba/internal/*.cpp | grep -v -e '/gbaLink\.cpp$' -e '/gbaSockClient\.cpp$'
  ls $CORE/base/*.cpp $CORE/base/internal/*.cpp $CORE/base/internal/*.c \
    | grep -v -e '/sdl_motion\.cpp$' -e '/file_util_libretro\.cpp$'
  ls $CORE/fex/fex/*.cpp $CORE/fex/unrar/*.cpp $CORE/fex/7z_C/*.c
} > "$B/sources.txt"

DEFS=(-DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DHAVE_ZLIB_H -DFINAL_VERSION -D__STDC_FORMAT_MACROS
      -DELF -DC_CORE -D__AMD64__ -D__X86_64__ -DNO_LINK -DGBA_LOGGING -DVBAM_ENABLE_DEBUGGER
      -DNO_ONLINEUPDATES -DNO_HTTPS '-DSYSCONF_INSTALL_DIR="/etc"' '-DPKGDATADIR="/usr/share/vbam"'
      '-DLOCALEDIR="/usr/share/locale"')
INCS=(-I"$SRC/src" -I"$B/gen" -I"$SRC/third_party/include" -I"$SRC/third_party/include/stb"
      -I"$SRC/third_party/include/nonstd" -I"$SRC/$CORE/fex")

# compile_all <outdir> <flags...>: every core TU -> <outdir>/<n>.o ; prints object list to <outdir>/objs.txt
compile_all() {
  local out="$1"; shift
  mkdir -p "$out"
  export OUT_DIR="$out" COMPILE_FLAGS="$*"
  local n=0
  : > "$out/jobs.txt"
  while read -r f; do
    n=$((n + 1))
    printf '%s\t%s/%04d.o\n' "$f" "$out" "$n" >> "$out/jobs.txt"
  done < "$B/sources.txt"
  cut -f2 "$out/jobs.txt" > "$out/objs.txt"
  # shellcheck disable=SC2016
  xargs -a "$out/jobs.txt" -d '\n' -P "$MAYHEM_JOBS" -I{} bash -c '
    set -euo pipefail
    IFS=$'"'"'\t'"'"' read -r src obj <<< "$1"
    case "$src" in
      *.c)   '"$CC"' -std=gnu11 $COMPILE_FLAGS -w -c "$src" -o "$obj" ;;
      *)     '"$CXX"' -std=gnu++17 $COMPILE_FLAGS -Wno-deprecated-copy -w -c "$src" -o "$obj" ;;
    esac' _ {}
}

# ---- 1) fuzz build: core + harnesses with sanitizers, DWARF 3 --------------------------
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1 ${DEFS[*]} ${INCS[*]}"
# shellcheck disable=SC2086
compile_all "$B/fuzz/core" $FUZZ_FLAGS
# shellcheck disable=SC2046
ar rcs "$B/fuzz/libvbamcore.a" $(cat "$B/fuzz/core/objs.txt")

$CXX -std=gnu++17 $FUZZ_FLAGS -w -c mayhem/vbam_stubs.cc -o "$B/fuzz/vbam_stubs.o"
$CXX -std=gnu++17 $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$B/fuzz/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/fuzz/standalone_main.o"

HARNESSES="fuzz_gba_rom fuzz_gb_rom fuzz_gba_state fuzz_gb_state fuzz_gba_cheats fuzz_gb_cheats fuzz_patch fuzz_archive"
for h in $HARNESSES; do
  $CXX -std=gnu++17 $FUZZ_FLAGS -w -Imayhem -c "mayhem/$h.cc" -o "$B/fuzz/$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE \
      "$B/fuzz/$h.o" "$B/fuzz/vbam_stubs.o" "$B/fuzz/lsan_off.o" "$B/fuzz/libvbamcore.a" -lz \
      -o "/mayhem/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS \
      "$B/fuzz/$h.o" "$B/fuzz/standalone_main.o" "$B/fuzz/vbam_stubs.o" "$B/fuzz/lsan_off.o" \
      "$B/fuzz/libvbamcore.a" -lz -o "/mayhem/$h-standalone"
done

# ---- 2) test build: NORMAL flags, no sanitizers ---------------------------------------
TEST_FLAGS="-O2 -DNDEBUG $COVERAGE_FLAGS ${DEFS[*]} ${INCS[*]}"
# shellcheck disable=SC2086
compile_all "$B/test/core" $TEST_FLAGS
# shellcheck disable=SC2046
ar rcs "$B/test/libvbamcore.a" $(cat "$B/test/core/objs.txt")

for t in video_capture_test pc_semantics_test; do
  # shellcheck disable=SC2086
  $CXX -std=gnu++17 $TEST_FLAGS -w "$CORE/test/$t.cpp" "$B/test/libvbamcore.a" -lz $COVERAGE_FLAGS \
      -o "$B/test/bin/$t"
done
# shellcheck disable=SC2086
$CXX -std=gnu++17 $TEST_FLAGS -w -Imayhem mayhem/kat_probe.cc mayhem/vbam_stubs.cc \
    "$B/test/libvbamcore.a" -lz $COVERAGE_FLAGS -o "$B/test/bin/kat_probe"

echo "build.sh: done"
