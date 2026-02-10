#!/bin/bash
# 빌드된 tcnnapp을 원격 보드로 SCP 배포 (tcnnapp이 링크하는 OpenCV 라이브러리 함께 복사)
# 사용: ./deploy.sh   (WSL/호스트 PC에서 실행. 먼저 ./build.sh 로 빌드 필요)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BINARY="$SCRIPT_DIR/build/tcnnapp"

SCP_TARGET="topst@192.168.0.25"
SCP_PATH="/home/topst/my_tcnn"

# SDK sysroot (WSL/호스트에 설치된 툴체인 경로). 보드에는 이 경로가 없음.
SDK_SYSROOT="${OECORE_TARGET_SYSROOT:-/usr/local/oecore-x86_64/sysroots/cortexa53-telechips-linux}"
SDK_LIB="$SDK_SYSROOT/usr/lib"

# tcnnapp이 실제로 링크하는 OpenCV 라이브러리 (core, imgproc, imgcodecs, videoio).
# 스크립트는 SDK에서 사용 가능한 가장 높은 4.5.x(.so) 파일을 자동 탐지합니다.
# 필요시 환경변수 OPENCV_VERSION=4.5.4 로 강제할 수 있습니다.
if [ -n "${OPENCV_VERSION:-}" ]; then
    VER="$OPENCV_VERSION"
    OPENCV_LIBS=(
        "libopencv_core.so.$VER"
        "libopencv_imgproc.so.$VER"
        "libopencv_imgcodecs.so.$VER"
        "libopencv_videoio.so.$VER"
    )
else
    OPENCV_COMPONENTS=(core imgproc imgcodecs videoio)
    OPENCV_LIBS=()
    for comp in "${OPENCV_COMPONENTS[@]}"; do
        libpath=$(ls "$SDK_LIB"/libopencv_${comp}.so.* 2>/dev/null | sort -V | tail -n1 || true)
        if [ -n "$libpath" ]; then
            OPENCV_LIBS+=("$(basename "$libpath")")
        fi
    done
    if [ ${#OPENCV_LIBS[@]} -eq 0 ]; then
        # 기존 기본값(도구체인에 없을 경우 사용자에게 알리기 위해 유지)
        OPENCV_LIBS=("libopencv_core.so.4.5.5" "libopencv_imgproc.so.4.5.5" "libopencv_imgcodecs.so.4.5.5" "libopencv_videoio.so.4.5.5")
    fi
fi

# SSH 연결 다중화: 비밀번호를 한 번만 입력하면 이후 ssh/scp 재사용
CTRL_SOCK="/tmp/deploy-ssh-$$"
SSH_OPTS="-o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 -o ControlMaster=auto -o ControlPath=$CTRL_SOCK -o ControlPersist=60"

cleanup() { ssh -o ControlPath="$CTRL_SOCK" -O exit "$SCP_TARGET" 2>/dev/null || true; }
trap cleanup EXIT

if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found. Run ./build.sh first."
    exit 1
fi

echo "Deploying to ${SCP_TARGET}:${SCP_PATH}..."
if ! ssh $SSH_OPTS "$SCP_TARGET" "mkdir -p $SCP_PATH"; then
    echo "SSH failed. Check connection or run: ssh-copy-id $SCP_TARGET"
    exit 1
fi
if ! scp $SSH_OPTS "$BINARY" "$SCRIPT_DIR/run.sh" "$SCRIPT_DIR/run_lane_tuning.sh" "$SCRIPT_DIR/lane_tuning.py" "$SCRIPT_DIR/tools/read_nn_shm_and_record.py" "$SCP_TARGET:$SCP_PATH/"; then
    echo "SCP failed. Check key auth: ssh-copy-id $SCP_TARGET"
    exit 1
fi

# OpenCV .so 복사 (보드에 없으면 tcnnapp 실행 불가)
for lib in "${OPENCV_LIBS[@]}"; do
    if [ -f "$SDK_LIB/$lib" ]; then
        scp $SSH_OPTS "$SDK_LIB/$lib" "$SCP_TARGET:$SCP_PATH/"
        echo "Copied $lib to board."
    else
        echo "Note: $SDK_LIB/$lib not found. If tcnnapp fails with missing $lib, run deploy from WSL/host where SDK is installed."
    fi
done
echo "On board run: LD_LIBRARY_PATH=$SCP_PATH ./tcnnapp -o rtpm"

echo "Done. $SCP_TARGET:$SCP_PATH/tcnnapp"
