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

#ifndef __CAMERA_API_H__
#define __CAMERA_API_H__

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <errno.h>
#include <linux/videodev2.h>

#ifdef CAMERA_USE_VCF
#include <vcf_api.h>
#endif


/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define CAMERA_DEV_NAME_0 "/dev/video0"
#define CAMERA_DEV_NAME_1 "/dev/video1"
#define CAMERA_DEV_NAME_2 "/dev/video2"
#define CAMERA_DEV_NAME_3 "/dev/video3"

#define CAM_RETRY_CNT 100

#define TELECHIPS_DEBUG
#ifdef TELECHIPS_DEBUG
    #define app_debug_printf(...) printf(__VA_ARGS__)
#else
    #define app_debug_printf(...) do {} while (0)
#endif

typedef enum cam_status
{
	CAMERA_STATUS_IDLE = 0,
	CAMERA_STATUS_OPENED,
	CAMERA_STATUS_STREAMING
} cam_status_t;

//remain support format
typedef enum cam_format
{
	CAMERA_FORMAT_RGB32 = 0,
	CAMERA_FORMAT_RGB24
} cam_format_t;

typedef void (*CameraHandle)(void);

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

int32_t CameraCreate(CameraHandle *pContext);
int32_t CameraDestroy(CameraHandle handle);
int32_t CameraOpenDevice(CameraHandle handle, char* inputDevName);
int32_t CameraCloseDevice(CameraHandle handle);
int32_t CameraSetConfig(CameraHandle handle, uint32_t width, uint32_t height);
uint32_t CameraGetBuffer(CameraHandle handle, uint8_t **virtualAddr, uint64_t *baseOffset);
uint32_t CameraReleaseBuffer(CameraHandle handle);
cam_status_t CameraGetStatus(CameraHandle handle);
#endif //__CAMERA_API_H__
