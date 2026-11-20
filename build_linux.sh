#!/bin/sh
# Builds Star Wars Trilogy Native on Linux (x86_64 or arm64): extracts the ROM,
# recompiles it, builds and leaves ./StarWarsTrilogy here, next to data/, res/
# and third_party/ which it plays from.
#
#   ./build_linux.sh [swtrilgy.zip]                  for this computer
#   ./build_linux.sh --arch aarch64 [swtrilgy.zip]   arm64 from an x86_64 PC (docs/LINUX.md)
#
# The ROM: the path given, else SWT_ROM_ZIP, else swtrilgy.zip here or in roms/
# (only needed the first time).
set -e
cd "$(dirname "$0")"

fail() {
    echo "$1" >&2
    exit 1
}

HOST=$(uname -m)
ARCH=$HOST
ZIP=$SWT_ROM_ZIP
while [ $# -gt 0 ]; do
    case "$1" in
    --arch) ARCH=$2; shift 2 ;;
    --arch=*) ARCH=${1#--arch=}; shift ;;
    -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
    *) ZIP=$1; shift ;;
    esac
done
case "$ARCH" in
arm64) ARCH=aarch64 ;;
amd64) ARCH=x86_64 ;;
esac
[ "$ARCH" = x86_64 ] || [ "$ARCH" = aarch64 ] || fail "Unknown architecture $ARCH: x86_64 or aarch64."

command -v python3 >/dev/null 2>&1 || fail "Python 3 not found: install python3 with your package manager."

if [ -z "$ZIP" ]; then
    for z in swtrilgy.zip roms/swtrilgy.zip; do
        [ -f "$z" ] && ZIP=$z && break
    done
fi
# drive_program.bin too: data/ from an older build can lack it (a set without
# epr-21119), and the game then stops at FAILED FEEDBACK LEVER INITIALIZE
if [ ! -f data/crom.bin ] || [ ! -f data/drive_program.bin ]; then
    [ -f "$ZIP" ] || fail "ROM not found: pass the path to swtrilgy.zip (./build_linux.sh ~/roms/swtrilgy.zip), set SWT_ROM_ZIP, or put it next to build_linux.sh."
    python3 tools/romtool.py "$ZIP" data
fi
python3 tools/recomp.py data/crom.bin build/gen

# CMake and Ninja: the ones installed, else private copies under build/ (nothing
# outside this folder changes)
tools_pip() {
    if [ ! -x build/tools/bin/pip ]; then
        mkdir -p build
        python3 -m venv build/tools || fail "Could not make build/tools: install python3-venv (or cmake and ninja themselves)."
    fi
    build/tools/bin/pip -q install "$@"
}
CMAKE=$(command -v cmake || true)
NINJA=$(command -v ninja || true)
if [ -z "$CMAKE" ] || [ -z "$NINJA" ]; then
    if [ ! -x build/tools/bin/ninja ]; then
        echo "Fetching CMake and Ninja into build/tools..."
        tools_pip cmake==4.4.4 ninja==1.13.2   # glslang needs CMake 3.27 or later
    fi
    CMAKE=$PWD/build/tools/bin/cmake
    NINJA=$PWD/build/tools/bin/ninja
fi
CTEST=$(dirname "$CMAKE")/ctest
[ "$ARCH" = "$HOST" ] || { [ "$ARCH" = aarch64 ] && [ "$HOST" = x86_64 ]; } ||
    fail "Cross-building is only set up from x86_64 to aarch64."

BUILD=build/linux-$ARCH
set -- -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="$NINJA"
if [ "$ARCH" != "$HOST" ]; then
    set -- "$@" -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/linux-aarch64.cmake"
elif command -v clang++ >/dev/null 2>&1; then
    # clang as on macOS (the generated files are large: it takes them well); g++ works too
    set -- "$@" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
fi
"$CMAKE" -S . -B "$BUILD" -G Ninja "$@"
"$CMAKE" --build "$BUILD"
"$CTEST" --test-dir "$BUILD" --output-on-failure

# The program is compiled from your ROM's code: it is yours alone, do not share it
if [ "$ARCH" = "$HOST" ]; then
    cp -f "$BUILD/StarWarsTrilogy" ./StarWarsTrilogy
    echo
    echo "Done: ./StarWarsTrilogy"
fi
