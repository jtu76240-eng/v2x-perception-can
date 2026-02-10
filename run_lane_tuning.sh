#!/bin/bash
# Run lane_tuning.py with LD_LIBRARY_PATH so cv2 finds libxcb/libXau/libXdmcp/libbsd/libmd
OPENCV_LIBS="$HOME/.local/lib/python3.10/site-packages/opencv_python_headless.libs"
EXTRA_LIBS="$HOME/.local/lib"
export LD_LIBRARY_PATH="${EXTRA_LIBS}:${OPENCV_LIBS}:${LD_LIBRARY_PATH}"
exec nice -n 10 python3 "$(dirname "$0")/lane_tuning.py" --no-display "$@"

