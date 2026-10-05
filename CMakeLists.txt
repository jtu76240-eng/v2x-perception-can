cmake_minimum_required(VERSION 2.8)
project(tcnnapp)

add_definitions(-DTCC7500)
# 개발용: 1초에 1프레임만 처리. 끄려면 아래 줄 주석 처리.
# add_definitions(-DDEV_LIMIT_1FPS)
# FishEye 왜곡 제거 전처리. 끄려면 아래 줄 주석 처리.
# add_definitions(-DUSE_FISHEYE_UNDISTORT)
# 직선이 안쪽으로 휘어 보이면 보정 강도 낮추기 (기본 0.75, 예: 0.6~0.8)
# add_definitions(-DFISHEYE_UNDISTORT_STRENGTH=1.00f)
# net=1 모델 연산 비활성화 (net=0만 실행). net=1 다시 쓰려면 아래 줄 주석 처리.
# add_definitions(-DDISABLE_NETWORK_1)
# 인터랙티브 메뉴 (stdin 입력). 끄려면 아래 줄 주석 처리.
add_definitions(-DINTERACTIVE_MODE)
# 기본 디버그 로그 모드. 0=끔, 1=콘솔 출력(LOG), 2=파일 출력(FILE). 주석 처리 시 LOG(1).
add_definitions(-DDEFAULT_DEBUG_MODE=0)

SET(TCNN_VERSION_MAJOR "2")
SET(TCNN_VERSION_MINOR "0")
SET(TCNN_VERSION_PATCH "4")

# OpenCV
find_package(OpenCV REQUIRED)
find_package(visionprotocol REQUIRED)
find_package(tcndnpu REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${LINUX_KERNEL_DIR}/include/video/telechips)
include_directories(${VCF_INCLUDE_DIRS})
include_directories(
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/common/camera
    ${CMAKE_SOURCE_DIR}/common/display
    ${CMAKE_SOURCE_DIR}/common/message
    ${CMAKE_SOURCE_DIR}/common/utils
    ${CMAKE_SOURCE_DIR}/common/video
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
include_directories(json-c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lpthread -ljson-c -ldl")
set(SOURCES
    src/NnAppMain.c
    src/NnCan.c
    src/NnDebug.c
    src/NnMemory.c
    src/NnNeuralNetwork.c
    src/NnPerf.c
    src/NnRtpm.c
    src/NnSignalHandler.c
)
set(SOURCES_COMMON
    common/camera/camera_api.c
    common/display/display_api.c
    common/display/scaler_api.c
    common/message/message_api.c
    common/utils/opencv_api.cpp
    common/utils/perf_api.c
    common/utils/time_api.c
)

# calib3d 제외. VideoWriter용 videoio 추가
set(LIBRARIES
    opencv_videoio opencv_imgcodecs opencv_imgproc opencv_core
    dl json-c visionprotocol tcndnpu
)

add_executable(tcnnapp ${SOURCES} ${SOURCES_COMMON})
target_link_libraries(tcnnapp ${LIBRARIES})
# target_compile_options(tcnnapp PRIVATE -Wall)
target_compile_options(tcnnapp PRIVATE -Wall -Wextra -Werror)
install(TARGETS tcnnapp DESTINATION bin)
