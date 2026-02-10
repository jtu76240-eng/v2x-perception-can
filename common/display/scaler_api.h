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

#ifndef __SCALERAPI_H__
#define __SCALERAPI_H__
/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/poll.h>
#include "tcc_scaler_ioctrl.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define ALIGN_BIT  (0x8-1)
#define BIT_0      (3)
#define GET_ADDR_YUV42X_spY(Base_addr) 		(((((unsigned int)Base_addr) + ALIGN_BIT)>> BIT_0)<<BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y) 	(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT)>> BIT_0)<<BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y) 	(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y) 	(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#define SCALER_DEV_NAME_0 "/dev/scaler1"
#define SCALER_DEV_NAME_1 "/dev/scaler3"

typedef enum _scaler_index
{
	SCALER_INDEX_0 = 0,
	SCALER_INDEX_1,
	SCALER_INDEX_MAX
} scaler_index_t;

typedef enum _scaler_format
{
    SCALER_FORMAT_RGB454            = 8,  /* This enum constants is not supported. */
    SCALER_FORMAT_RGB444            = 9,  /* This enum constants is not supported. */
    SCALER_FORMAT_RGB565            = 10, /* This enum constants is not supported. */
    SCALER_FORMAT_RGB555            = 11, /* This enum constants is not supported. */
    SCALER_FORMAT_ARGB8888          = 12,
    SCALER_FORMAT_ARGB6666_4        = 13, /* This enum constants is not supported. */
    SCALER_FORMAT_RGB888            = 14,
    SCALER_FORMAT_ARGB6666_3        = 15, /* This enum constants is not supported. */
    SCALER_FORMAT_COMPRESS_DATA     = 16, /* This enum constants is not supported. */
    SCALER_FORMAT_YUV420_sp         = 24, /* This enum constants is not supported. */
    SCALER_FORMAT_YUV422_sp         = 25, /* This enum constants is not supported. */
    SCALER_FORMAT_YUV422_sq0        = 26, /* This enum constants is not supported. */
    SCALER_FORMAT_YUV422_sq1        = 27, /* This enum constants is not supported. */
    SCALER_FORMAT_YUV420_inter      = 28, /* This enum constants is not supported. */
    SCALER_FORMAT_YUV420_inter_NV21 = 29, /* This enum constants is not supported. */
    SCALER_FORMAT_YUV422_inter      = 30, /* This enum constants is not supported. */
    SCALER_FORMAT_MAX
} scaler_format_t;

typedef struct _scaler_size_info
{
	uint64_t pmap;		// physical address of image
	uint32_t width;		// image width
	uint32_t height;	// image height
	scaler_format_t format; // image format
} scaler_size_info_t;

typedef struct _scaler_context
{
	int32_t scalerFd[SCALER_INDEX_MAX];
} scaler_context_t;

typedef void (*ScalerHandle)(void);

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
int32_t ScalerCreate(ScalerHandle *pHandle);
int32_t ScalerDestroy(ScalerHandle handle);
int32_t ScalerOpenDevice(ScalerHandle handle, const char *scalerDevName, scaler_index_t scalerIndex);
int32_t ScalerCloseDevice(ScalerHandle handle, scaler_index_t scalerIndex);
int32_t ScalerResize(ScalerHandle handle, scaler_index_t scalerIndex, scaler_size_info_t scalerSrc, scaler_size_info_t scalerDes);
int32_t ScalerCropResize(ScalerHandle handle, scaler_index_t scalerIndex, scaler_size_info_t scalerSrc, scaler_size_info_t scalerDes, uint32_t cropXMin, uint32_t cropYMin, uint32_t cropXMax, uint32_t cropYMax);
int32_t ScalerPoll(ScalerHandle handle, scaler_index_t scalerIndex);
int32_t ScalerGetStatus(ScalerHandle handle, scaler_index_t scalerIndex);


#endif // __SCALERAPI_H__
