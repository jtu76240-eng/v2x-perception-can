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
#include "display_api.h"


/* ========================================================================== */
/*                         Structures and Enums                               */
/* ========================================================================== */

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */
int32_t DisplayCreate(DisplayHandle *pHandle)
{
	int32_t status = 0;
	display_context_t *pContext;

	*pHandle = NULL;

	pContext = malloc(sizeof(*pContext));
	if (pContext == NULL)
	{
		return -1;
	}

	memset(pContext, 0, sizeof(*pContext));

	*pHandle = (DisplayHandle)pContext;

	return status;
}

int32_t DisplayDestroy(DisplayHandle handle)
{
	int32_t status = 0;

	if (handle)
	{
		free(handle);
	}

	return status;
}

int32_t DisplayOpenDevice(DisplayHandle handle, char* outputLcdPath)
{
	int32_t status = 0;

	display_context_t *pContext = (display_context_t *)handle;

	pContext->display_fd = open(outputLcdPath, O_RDWR);

	return status;
}

int32_t DisplayCloseDevice(DisplayHandle handle)
{
	int32_t status = 0;

	display_context_t *pContext = (display_context_t *)handle;

	close(pContext->display_fd);

	return status;
}

int32_t DisplayShow(DisplayHandle handle, uint64_t baseAddr, uint32_t desX, uint32_t desY, uint32_t desW, uint32_t desH)
{
	int32_t status = 0;

	display_context_t *pContext = (display_context_t *)handle;
	overlay_video_buffer_t overBuffCfg;

	overBuffCfg.cfg.sx = desX;
	overBuffCfg.cfg.sy = desY;
	overBuffCfg.cfg.width = desW;
	overBuffCfg.cfg.height = desH;
	overBuffCfg.cfg.format = DISPLAY_FORMAT_RGB888;
	overBuffCfg.cfg.transform = 0; // N/A, set 0

	overBuffCfg.addr = baseAddr;
	overBuffCfg.addr1 = 0;
	overBuffCfg.addr2 = 0;
	overBuffCfg.afbc_dec_num = 0;
	overBuffCfg.afbc_dec_need = 0;

	if (ioctl(pContext->display_fd, OVERLAY_PUSH_VIDEO_BUFFER, (unsigned long)&overBuffCfg) != 0)
	{
		fprintf(stderr, "Error: ioctl failed for OVERLAY_PUSH_VIDEO_BUFFER. Error code: %d, Message: %s\n", errno, strerror(errno));

		status = -1;
	}
	else
	{
		// printf("Video buffer push succeeded\n");
	}
	// printf("display:[0x%x]\n",baseAddr);

	return status;
}