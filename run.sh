#!/bin/bash
# 보드에서 tcnnapp 실행 스크립트
# 사용: ./run.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
sudo LD_LIBRARY_PATH="$SCRIPT_DIR" "$SCRIPT_DIR/tcnnapp" -o n -n yolov8s_quantized "$@"
