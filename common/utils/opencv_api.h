/*
 * Copyright Telechips Inc.
 *
 * TCC Version 1.0
 *
 * This source code contains confidential information of Telechips.
 *
 * Any unauthorized use without a written permission of Telechips including not
 * limited to re-distribution in source or binary form is strictly prohibited.
 *
 * This source code is provided "AS IS" and nothing contained in this source code
 * shall constitute any express or implied warranty of any kind, including without
 * limitation, any warranty of merchantability, fitness for a particular purpose
 * or non-infringement of any patent, copyright or other third party intellectual
 * property right.
 * No warranty is made, express or implied, regarding the information's accuracy,
 * completeness, or performance.
 *
 * In no event shall Telechips be liable for any claim, damages or other
 * liability arising from, out of or in connection with this source code or
 * the use in the source code.
 *
 * This source code is provided subject to the terms of a Mutual Non-Disclosure
 * Agreement between Telechips and Company.
 */

#ifndef __OPENCV_API_H__
#define __OPENCV_API_H__

#include "NnType.h"

#define BASE_WIDTH 1920.0
#define BASE_HEIGHT 720.0
#define RGB(r, g, b) ((Color_t){(b), (g), (r)})

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _draw_info
{
	DRAW_INFO_FPS = 0,
	DRAW_INFO_NETWORK,
	DRAW_INFO_CPU,
	DRAW_INFO_MEMORY,
	DRAW_INFO_NPU
} draw_info_t;

typedef struct _color
{
	unsigned char b;
	unsigned char g;
	unsigned char r;
} Color_t;

typedef struct _box
{
	int xmin;
	int ymin;
	int xmax;
	int ymax;
	int cls;
	float score;
} Box_t;

typedef struct _point
{
	int x;
	int y;
} Point_t;

void cvDrawInfo(unsigned char *desBuffer, uint32_t outputWidth, uint32_t outputHeight, draw_info_t infoType, double value, int deviceIdx, int xPos, int yPos, double fontSize, const Color_t textColor);
void cvDrawBoxes(unsigned char *desBuffer, Box_t *boxes, int boxCount, uint32_t outputWidth, uint32_t outputHeight, uint32_t boxImageWidth, uint32_t boxImageHeight, const Color_t boxColor, const Color_t textColor, double fontSize, int textOffset);
void cvDrawCls(unsigned char *desBuffer, uint32_t width, uint32_t height, const int className, int xPos, int yPos, const Color_t textColor, double fontSize);
int cvLoadImage(unsigned char *desBuffer, const char *filePath, uint32_t *width, uint32_t *height);
void cvSaveImage(unsigned char *srcBuffer, const char *filePath, const uint32_t width, const uint32_t height);
/* PNG 저장: 3ch=그대로, 4ch=RGBA→BGR 변환 후 저장. OpenCV와 효율적으로 쓰려면 BGR 유지 권장 */
void cvSaveImageEx(unsigned char *srcBuffer, const char *filePath, uint32_t width, uint32_t height, int channels);

void cvResize(unsigned char *srcBuffer, uint32_t srcW, uint32_t srcH, unsigned char *desBuffer, uint32_t desW, uint32_t desH, int channels);

/* 왜곡 보정: src -> dst (imgproc만 사용, calib3d 불필요). K[9]=3x3 row-major, D[5]=표준 계수 (k1,k2,p1,p2,k3) */
void cvFisheyeUndistort(unsigned char *src, unsigned char *dst, uint32_t width, uint32_t height, int channels, const float *K, const float *D);
/* 맵 사전 계산: 초기화 시 1회 호출. 반환 핸들은 cvFisheyeUndistortMapRelease로 해제. */
void *cvFisheyeUndistortMapCreate(const float *K, const float *D, uint32_t width, uint32_t height);
/* 맵 사용 remap만 수행 (매 프레임 호출). mapHandle은 MapCreate 반환값. */
void cvFisheyeUndistortRemap(unsigned char *src, unsigned char *dst, uint32_t width, uint32_t height, int channels, void *mapHandle);
void cvFisheyeUndistortMapRelease(void **mapHandle);
void cvDrawLines(unsigned char *desBuffer, uint32_t width, uint32_t height, Point_t *P1, Point_t *P2, int N, const Color_t color, int thickness);
void cvDrawPoints(unsigned char *desBuffer, uint32_t width, uint32_t height, Point_t *points, int numPoints, int pointRadius, const Color_t pointColor);
void cvDrawCustomText(unsigned char *desBuffer, uint32_t width, uint32_t height, const char *customText, int xPos, int yPos, const Color_t textColor, double fontSize);

/* RGB → BGR 복사 (출력 경계에서 1회만 사용 시 연산 최소). src/dst는 width*height*3. */
void cvRgbToBgrCopy(const unsigned char *src, unsigned char *dst, uint32_t width, uint32_t height);

/* 180도 회전 (in-place). buffer는 width*height*channels. */
void cvRotate180(unsigned char *buffer, uint32_t width, uint32_t height, int channels);

/* 비동기 영상 녹화용 (NN 프로세스 내 VideoWriter). buffer는 RGB, AVI에도 RGB로 기록. */
void *cvVideoWriterCreate(const char *filePath, uint32_t width, uint32_t height, double fps);
int cvVideoWriterWrite(void *writer, unsigned char *buffer, uint32_t width, uint32_t height);
void cvVideoWriterRelease(void **writer);

#ifdef __cplusplus
}
#endif

#endif // __OPENCV_API_H__
