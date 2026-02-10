#!/bin/bash
# AI-G 보드용 ai_board(tcnnapp) 빌드 스크립트
# 사용 전에 SDK 환경을 source 하세요:
#   . /usr/local/oecore-x86_64/environment-setup-cortexa53-telechips-linux

set -e

SDK_ENV="/usr/local/oecore-x86_64/environment-setup-cortexa53-telechips-linux"
KERNEL_DIR="/usr/local/oecore-x86_64/sysroots/cortexa53-telechips-linux/lib/modules/5.10.223-tcc/build"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

if [ ! -f "$SDK_ENV" ]; then
    echo "Error: SDK not found at $SDK_ENV"
    echo "Install the SDK first, then run: . $SDK_ENV"
    exit 1
fi

echo "[1/3] Sourcing SDK environment..."
# shellcheck source=/dev/null
. "$SDK_ENV"

echo "[2/3] Configuring CMake..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DLINUX_KERNEL_DIR="$KERNEL_DIR" \
    -DVCF_INCLUDE_DIRS="$OECORE_TARGET_SYSROOT/usr/include"

echo "[3/3] Building..."
make -j"$(nproc)"

echo "Done. Binary: $BUILD_DIR/tcnnapp"
