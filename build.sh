#!/usr/bin/env bash
# Build ptr_inject.sh without Aide.
#   Linux/macOS/WSL : export ANDROID_NDK_HOME=/path/to/ndk ; ./build.sh
#   Termux          : pkg install clang ; ./build.sh
set -euo pipefail
cd "$(dirname "$0")"
OUT=out; mkdir -p "$OUT"
SRC=src/main/jni/inject.cpp
FLAGS="-std=c++17 -O2 -fno-rtti -fvisibility=hidden -ffunction-sections -fdata-sections -w -fPIE -pie -Wl,--gc-sections -Wl,--strip-all"

if [ -n "${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}" ]; then
  NDK="${ANDROID_NDK_HOME:-$ANDROID_NDK_ROOT}"
  CXX=$(ls "$NDK"/toolchains/llvm/prebuilt/*/bin/aarch64-linux-android28-clang++)
  "$CXX" $FLAGS -static-libstdc++ "$SRC" -o "$OUT/ptr_inject.sh"
elif command -v clang++ >/dev/null && [ "$(uname -m)" = "aarch64" ]; then   # Termux
  clang++ $FLAGS "$SRC" -o "$OUT/ptr_inject.sh"
else
  echo "Set ANDROID_NDK_HOME (NDK r25+) or run inside Termux with clang installed." >&2; exit 1
fi
chmod 755 "$OUT/ptr_inject.sh"; ls -lh "$OUT/ptr_inject.sh"
