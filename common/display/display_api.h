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

#ifndef __DISPLAYAPI_H__
#define __DISPLAYAPI_H__
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

#include "tcc_overlay_ioctl.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define DISPLAY_DEV_NAME_0 "/dev/overlay"
#define DISPLAY_DEV_NAME_1 "/dev/overlay1"
#define DISPLAY_DEV_NAME_2 "/dev/overlay2"

typedef enum _display_format
{
    DISPLAY_FORMAT_RGB454            = 8,  /* This enum constants is not supported. */
    DISPLAY_FORMAT_RGB444            = 9,  /* This enum constants is not supported. */
    DISPLAY_FORMAT_RGB565            = 10, /* This enum constants is not supported. */
    DISPLAY_FORMAT_RGB555            = 11, /* This enum constants is not supported. */
    DISPLAY_FORMAT_ARGB8888          = 12,
    DISPLAY_FORMAT_ARGB6666_4        = 13, /* This enum constants is not supported. */
    DISPLAY_FORMAT_RGB888            = 14,
    DISPLAY_FORMAT_ARGB6666_3        = 15, /* This enum constants is not supported. */
    DISPLAY_FORMAT_COMPRESS_DATA     = 16, /* This enum constants is not supported. */
    DISPLAY_FORMAT_YUV420_sp         = 24, /* This enum constants is not supported. */
    DISPLAY_FORMAT_YUV422_sp         = 25, /* This enum constants is not supported. */
    DISPLAY_FORMAT_YUV422_sq0        = 26, /* This enum constants is not supported. */
    DISPLAY_FORMAT_YUV422_sq1        = 27, /* This enum constants is not supported. */
    DISPLAY_FORMAT_YUV420_inter      = 28, /* This enum constants is not supported. */
    DISPLAY_FORMAT_YUV420_inter_NV21 = 29, /* This enum constants is not supported. */
    DISPLAY_FORMAT_YUV422_inter      = 30, /* This enum constants is not supported. */
    DISPLAY_FORMAT_MAX
} display_format_t;

typedef struct _display_context
{
	int32_t display_fd;
} display_context_t;

typedef void (*DisplayHandle)(void);

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
int32_t DisplayCreate(DisplayHandle *pHandle);
int32_t DisplayDestroy(DisplayHandle handle);
int32_t DisplayOpenDevice(DisplayHandle handle, char* outputLcdPath);
int32_t DisplayCloseDevice(DisplayHandle handle);
int32_t DisplayShow(DisplayHandle handle, uint64_t baseAddr, uint32_t desX, uint32_t desY, uint32_t desW, uint32_t desH);

#endif //__DISPLAYAPI_H__
