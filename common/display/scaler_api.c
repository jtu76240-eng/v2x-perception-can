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

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include "scaler_api.h"

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
// static int scalerFd[SCALER_INDEX_MAX] = {-1, -1};

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */
int32_t ScalerCreate(ScalerHandle *pHandle)
{
	int32_t status = 0;
	scaler_context_t *pContext;

	*pHandle = NULL;

	pContext = malloc(sizeof(*pContext));
	if (pContext == NULL)
	{
		return -1;
	}

	memset(pContext, 0, sizeof(*pContext));

	*pHandle = (ScalerHandle)pContext;

	return status;
}

int32_t ScalerDestroy(ScalerHandle handle)
{
	int32_t status = 0;

	if (handle)
	{
		free(handle);
	}

	return status;
}

int32_t ScalerOpenDevice(ScalerHandle handle, const char *scalerDevName, scaler_index_t scalerIndex)
{
	int32_t status = -1;

	int32_t fd;
	scaler_context_t *pContext = (scaler_context_t *)handle;

	if(SCALER_INDEX_0 <= scalerIndex && scalerIndex < SCALER_INDEX_MAX)
	{
		fd = open(scalerDevName, O_RDWR | O_NDELAY);
		if(fd >= 0)
		{
			pContext->scalerFd[scalerIndex] = fd;
			printf("[INFO] [SCALER_API] scaler %s open() success:, fd:%d\n", scalerDevName, pContext->scalerFd[scalerIndex]);
			status = 0;
		}
		else
		{
			printf("[ERROR] [SCALER_API] scaler %s open() error\n", scalerDevName);
			status = -1;
		}
	}
    else
	{
		// Index Error
		status = -1;
	}

	return status;
}

int32_t ScalerCloseDevice(ScalerHandle handle, scaler_index_t scalerIndex)
{
	int32_t status = -1;

	scaler_context_t *pContext = (scaler_context_t *)handle;

	if(SCALER_INDEX_0 <= scalerIndex && scalerIndex < SCALER_INDEX_MAX)
	{
		close(pContext->scalerFd[scalerIndex]);
		pContext->scalerFd[scalerIndex] = -1;

		status = 0;
	}
	else
	{
		// Index Error
		status = -1;
	}

	return status;
}

int32_t ScalerGetStatus(ScalerHandle handle, scaler_index_t scalerIndex)
{
	int32_t status = -1;

	scaler_context_t *pContext = (scaler_context_t *)handle;

	if(pContext->scalerFd[scalerIndex] >= 0)
	{
		status = 0;
	}
	else
	{
		status = -1;
	}

	return status;
}

int32_t ScalerResize(ScalerHandle handle, scaler_index_t scalerIndex, scaler_size_info_t scalerSrc, scaler_size_info_t scalerDes)
{
	int32_t ret = -1;

	struct SCALER_TYPE scaler_info;
	scaler_context_t *pContext = (scaler_context_t *)handle;
	// printf("ScalerResize scalerIndex:%d\n", scalerIndex);

	if(SCALER_INDEX_0 <= scalerIndex && scalerIndex < SCALER_INDEX_MAX)
	{
		/* memset 0 scaler info structure */
		memset(&scaler_info, 0, sizeof(struct SCALER_TYPE));

		/* set scaler response type : interrupt */
		scaler_info.responsetype = SCALER_INTERRUPT;//SCALER_POLLING;

		/* source setting */
		scaler_info.src_Yaddr = scalerSrc.pmap;
		scaler_info.src_Uaddr = scalerSrc.pmap;
		scaler_info.src_Vaddr = scalerSrc.pmap;
		scaler_info.src_winLeft = 0;
		scaler_info.src_winTop = 0;
		scaler_info.src_winRight = scalerSrc.width;
		scaler_info.src_winBottom = scalerSrc.height;
		scaler_info.src_ImgWidth = scalerSrc.width;
		scaler_info.src_ImgHeight = scalerSrc.height;
		scaler_info.src_fmt = scalerSrc.format;

		/* destination setting */
		scaler_info.dest_Yaddr = scalerDes.pmap;
		scaler_info.dest_Uaddr = scalerDes.pmap;
		scaler_info.dest_Vaddr = scalerDes.pmap;
		scaler_info.dest_winLeft = 0;
		scaler_info.dest_winTop = 0;
		scaler_info.dest_winRight = scalerDes.width;
		scaler_info.dest_winBottom = scalerDes.height;
		scaler_info.dest_ImgWidth = scalerDes.width;
		scaler_info.dest_ImgHeight = scalerDes.height;
		scaler_info.dest_fmt = scalerDes.format;

		// printf("[INFO] [SCALER_API] Frame Resize : input[%8lx,%dx%d] -> output[%8lx,%dx%d]\n", scalerSrc.pmap, scalerSrc.width, scalerSrc.height, scalerDes.pmap, scalerDes.width, scalerDes.height);
		/* executing scaler */
		ret = ioctl(pContext->scalerFd[scalerIndex], TCC_SCALER_IOCTRL, (uint64_t)&scaler_info);
		if(ret < 0)
		{
			printf("[ERROR] [SCALER_API] scaler ioctl() error!!: fd:%d\n", pContext->scalerFd[scalerIndex]);
		}
	}
	return ret;
}

int32_t ScalerCropResize(ScalerHandle handle, scaler_index_t scalerIndex, scaler_size_info_t scalerSrc, scaler_size_info_t scalerDes, uint32_t cropXMin, uint32_t cropYMin, uint32_t cropXMax, uint32_t cropYMax)
{
	int ret = -1;
	struct SCALER_TYPE scaler_info;
	scaler_context_t *pContext = (scaler_context_t *)handle;

	if(SCALER_INDEX_0 <= scalerIndex && scalerIndex < SCALER_INDEX_MAX)
	{
		/* memset 0 scaler info structure */
		memset(&scaler_info, 0, sizeof(struct SCALER_TYPE));

		/* set scaler response type : interrupt */
		scaler_info.responsetype = SCALER_INTERRUPT;//SCALER_POLLING;

		/* source setting */
		scaler_info.src_Yaddr = scalerSrc.pmap;
		scaler_info.src_Uaddr = scalerSrc.pmap;
		scaler_info.src_Vaddr = scalerSrc.pmap;
		scaler_info.src_winLeft = cropXMin;
		scaler_info.src_winTop = cropYMin;
		scaler_info.src_winRight = cropXMax;
		scaler_info.src_winBottom = cropYMax;
		scaler_info.src_ImgWidth = scalerSrc.width;
		scaler_info.src_ImgHeight = scalerSrc.height;
		scaler_info.src_fmt = scalerSrc.format;

		/* destination setting */
		scaler_info.dest_Yaddr = scalerDes.pmap;
		scaler_info.dest_Uaddr = scalerDes.pmap;
		scaler_info.dest_Vaddr = scalerDes.pmap;
		scaler_info.dest_winLeft = 0;
		scaler_info.dest_winTop = 0;
		scaler_info.dest_winRight = scalerDes.width;
		scaler_info.dest_winBottom = scalerDes.height;
		scaler_info.dest_ImgWidth = scalerDes.width;
		scaler_info.dest_ImgHeight = scalerDes.height;
		scaler_info.dest_fmt = scalerDes.format;

		// printf("[INFO] [SCALER_API] input[%8lx,(%d, %d), (%d, %d)] -> output[%8lx,%dx%d]\n", scalerSrc.pmap, cropXMin, cropYMin, cropXMax, cropYMax, scalerDes.pmap, scalerDes.width, scalerDes.height);
		/* executing scaler */
		ret = ioctl(pContext->scalerFd[scalerIndex], TCC_SCALER_IOCTRL, (uint64_t)&scaler_info);
		if(ret < 0)
		{
			printf("[ERROR] [SCALER_API] scaler ioctl() error!!\n");
		}
	}
	else
	{
		printf("[ERROR] [SCALER_API] scaler index error!!\n");
	}
	return ret;
}

// fail: -1, sucess: 0
int32_t ScalerPoll(ScalerHandle handle, scaler_index_t scalerIndex)
{
	int32_t ret = 0;
	struct pollfd poll_event[1];
	scaler_context_t *pContext = (scaler_context_t *)handle;

	if(SCALER_INDEX_0 <= scalerIndex && scalerIndex < SCALER_INDEX_MAX)
	{
		memset(poll_event, 0, sizeof(poll_event));
		poll_event[0].fd = pContext->scalerFd[scalerIndex];
		poll_event[0].events = POLLIN;
		ret = poll((struct pollfd *)poll_event, 1, 400);

		if (ret < 0)
		{
			printf("[ERROR] [SCALER_API] poll() error %d\n", __LINE__);
			ret = -1;
		}
		else if (ret == 0)
		{
			printf("[ERROR] [SCALER_API] poll() Timeout %d\n", __LINE__);
			ret = -1;
		}
		else if (ret > 0 && (poll_event[0].revents & POLLERR))
		{
			printf("[ERROR] [SCALER_API] poll() error %d\n", __LINE__);
			ret = -1;
		}
	}
	else
	{
		printf("[ERROR] [SCALER_API] scaler index error!!\n");
	}
	return ret;
}
