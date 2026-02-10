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
#include "camera_api.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define BUFFER_COUNT (4 /* buffer */ * 3 /* planes */)
#define USED_TRUE 1
#define USED_FALSE 0

typedef struct cam_buffer
{
	struct v4l2_buffer v4l2Buffer;
	uint8_t used;
	uint64_t length;
	uint64_t phyAddr;
	uint8_t *virAddr;
} cam_buffer_t;

typedef struct cam_context
{
	cam_status_t status;
	uint32_t width;
	uint32_t height;

	/* camera */
	int32_t fd;
	struct v4l2_format format;
	struct v4l2_requestbuffers reqBuffer;

	struct v4l2_buffer bufferInfo;
	struct v4l2_plane planes[3];

	cam_buffer_t camBuffer[BUFFER_COUNT];
} cam_context_t;

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */
int32_t CameraCreate(CameraHandle *pHandle)
{
	int32_t status = 0;
	cam_context_t *pContext;

	*pHandle = NULL;

	pContext = malloc(sizeof(*pContext));
	if (pContext == NULL)
	{
		return -1;
	}

	memset(pContext, 0, sizeof(*pContext));
	*pHandle = (CameraHandle)pContext;

	return status;
}

int32_t CameraDestroy(CameraHandle handle)
{
	int32_t status = 0;

	if (handle)
	{
		free(handle);
	}

	return status;
}

static void ReleaseVirtAddr(cam_buffer_t *cambuffer)
{
	int32_t i;

	for (i = 0; i < BUFFER_COUNT; i++)
	{
		if (cambuffer[i].used == USED_TRUE)
		{
			if (-1 == munmap(cambuffer[i].virAddr, cambuffer[i].length))
			{
				app_debug_printf("[ERROR] [CAMERA_API] [%s] Failed to munmap (index : %d)\n", __FUNCTION__, i);
			}
			else
			{
				app_debug_printf("[ERROR] [CAMERA_API] [%s] munmap (index : %d)\n", __FUNCTION__, i);
			}
		}
	}
}

cam_status_t CameraGetStatus(CameraHandle handle)
{
	int32_t status = 0;

	cam_context_t *pContext = (cam_context_t *)handle;

	status = pContext->status;

	return status;
}

int32_t CameraOpenDevice(CameraHandle handle, char *inputDevName)
{
	int32_t ret = -1;

	cam_context_t *pContext = (cam_context_t *)handle;

	if (pContext->status == CAMERA_STATUS_IDLE)
	{
		pContext->fd = open(inputDevName, O_RDWR);
		if (pContext->fd > 0)
		{
			struct v4l2_capability capa;
			ret = ioctl(pContext->fd, VIDIOC_QUERYCAP, &capa);
			if (ret == 0)
			{
				if (!(capa.capabilities & (uint32_t)V4L2_CAP_VIDEO_CAPTURE_MPLANE))
				{
					app_debug_printf("[ERROR] [CAMERA_API] [%s] Device is Not support V4L2_CAP_VIDEO_CAPTURE_MPLANE\n", __FUNCTION__);
					ret = -2;
				}
				else
				{
					pContext->status = CAMERA_STATUS_OPENED;
				}
			}
		}
		else
		{
			app_debug_printf("[ERROR] [CAMERA_API] [%s] Device open error\n", __FUNCTION__);
			ret = -1;
		}
	}
	else
	{
		app_debug_printf("[ERROR] [CAMERA_API] [%s] Device is already open\n", __FUNCTION__);
	}

	return ret;
}

int32_t CameraCloseDevice(CameraHandle handle)
{
	int32_t ret = -1;

	cam_context_t *pContext = (cam_context_t *)handle;

	if ((pContext->status > CAMERA_STATUS_IDLE) && (pContext->status <= CAMERA_STATUS_STREAMING))
	{
		if (pContext->status > CAMERA_STATUS_OPENED)
		{
			ReleaseVirtAddr(pContext->camBuffer);
		}

		ret = close(pContext->fd);
		if (ret == 0)
		{
			pContext->status = CAMERA_STATUS_IDLE;
		}
		else
		{
			app_debug_printf("[ERROR] [CAMERA_API] [%s] Device close error\n", __FUNCTION__);
		}
	}
	else
	{
		app_debug_printf("[ERROR] [CAMERA_API] [%s] Device is not prepared\n", __FUNCTION__);
	}

	return ret;
}

int32_t CameraSetConfig(CameraHandle handle, uint32_t width, uint32_t height)
{
	int32_t ret = -1;

	cam_context_t *pContext = (cam_context_t *)handle;

	if (pContext->status == CAMERA_STATUS_OPENED)
	{
		pContext->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		pContext->format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_RGB32;
		pContext->format.fmt.pix_mp.width = width;
		pContext->format.fmt.pix_mp.height = height;
		pContext->format.fmt.pix_mp.field = 0;
		pContext->format.fmt.pix_mp.num_planes = 1;
		pContext->format.fmt.pix_mp.plane_fmt[0].sizeimage = width * height * 4;
		ret = ioctl(pContext->fd, VIDIOC_S_FMT, &pContext->format);
		if (ret == 0)
		{
			pContext->reqBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			pContext->reqBuffer.memory = V4L2_MEMORY_MMAP;
			pContext->reqBuffer.count = 4;
			ret = ioctl(pContext->fd, VIDIOC_REQBUFS, &pContext->reqBuffer);
			if (ret == 0)
			{
				struct v4l2_plane planes[3];
				uint32_t bufIndex;
				for (bufIndex = 0; bufIndex < pContext->reqBuffer.count; bufIndex++)
				{
					// struct v4l2_buffer *bufferInfo = &pContext->camBuffer[bufIndex]->v4l2Buffer;
					struct v4l2_buffer bufferInfo;
					memset(&planes, 0, sizeof(planes));
					memset(&bufferInfo, 0, sizeof(bufferInfo));
					bufferInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
					bufferInfo.memory = V4L2_MEMORY_MMAP;
					bufferInfo.index = bufIndex;
					bufferInfo.m.planes = planes;
					bufferInfo.length = 1;

					ret = ioctl(pContext->fd, VIDIOC_QUERYBUF, &bufferInfo);
					if (ret == 0)
					{
						pContext->camBuffer[bufIndex].length = (uint64_t)planes[0].length;
						pContext->camBuffer[bufIndex].phyAddr = (uint64_t)planes[0].reserved[0];
						app_debug_printf("[INFO] [CAMERA_API] [%s] mmap idx : %d, len : %d, off : %8x, reserved : %8x\n", __FUNCTION__, bufIndex, planes[0].length, planes[0].m.mem_offset, planes[0].reserved[0]);
						pContext->camBuffer[bufIndex].virAddr = (uint8_t *)mmap(NULL, pContext->camBuffer[bufIndex].length, PROT_READ | PROT_WRITE, MAP_SHARED, pContext->fd, (off_t)planes[0].m.mem_offset);
						if (pContext->camBuffer[bufIndex].virAddr == MAP_FAILED)
						{
							app_debug_printf("[ERROR] [CAMERA_API] [%s] mmap fail [index : %d]\n", __FUNCTION__, bufIndex);
							break;
						}
						ret = ioctl(pContext->fd, VIDIOC_QBUF, &bufferInfo);
						if (ret < 0)
						{
							app_debug_printf("[ERROR] [CAMERA_API] [%s] Queue buffer error [index : %d]\n", __FUNCTION__, bufIndex);
							break;
						}
						memset(pContext->camBuffer[bufIndex].virAddr, 0, planes[0].length);
					}
					else
					{
						app_debug_printf("[ERROR] [CAMERA_API] [%s] Query buffer error [index : %d]\n", __FUNCTION__, bufIndex);
						break;
					}
				}

				int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
				if (ioctl(pContext->fd, VIDIOC_STREAMON, &type) < 0)
				{
					printf("VIDIOC_STREAMON\n");
					exit(1);
				}
				else
				{
					pContext->status = CAMERA_STATUS_STREAMING;
				}
			}
			else
			{
				app_debug_printf("[ERROR] [CAMERA_API] [%s] Request buffer error\n", __FUNCTION__);
			}
		}
		else
		{
			app_debug_printf("[ERROR] [CAMERA_API] [%s] Set format error\n", __FUNCTION__);
		}
	}
	else
	{
		app_debug_printf("[ERROR] [CAMERA_API] [%s] Device is not prepared\n", __FUNCTION__);
	}

	return ret;
}

uint32_t CameraGetBuffer(CameraHandle handle, uint8_t **virtualAddr, uint64_t *baseOffset)
{
	uint32_t size = 0;

	cam_context_t *pContext = (cam_context_t *)handle;

	if (pContext->status > CAMERA_STATUS_OPENED)
	{
		memset(&(pContext->bufferInfo), 0, sizeof(struct v4l2_buffer));
		memset(&(pContext->planes), 0, sizeof(struct v4l2_plane)*3);
		pContext->bufferInfo.type = (uint32_t)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		pContext->bufferInfo.memory = (uint32_t)V4L2_MEMORY_MMAP;
		pContext->bufferInfo.m.planes = pContext->planes;
		pContext->bufferInfo.length = 1;
		size = ioctl(pContext->fd, VIDIOC_DQBUF, &(pContext->bufferInfo));
		if (size == 0)
		{
			*virtualAddr = pContext->camBuffer[pContext->bufferInfo.index].virAddr;
			*baseOffset = pContext->camBuffer[pContext->bufferInfo.index].phyAddr;
			size = pContext->camBuffer[pContext->bufferInfo.index].length;
		}
		else
		{
			*virtualAddr = NULL;
			*baseOffset = 0ull;
			app_debug_printf("[ERROR] [CAMERA_API] [%s] Dequeue error\n", __FUNCTION__);
		}
	}
	else
	{
		app_debug_printf("[ERROR] [CAMERA_API] [%s] Device is not prepared\n", __FUNCTION__);
	}

	return size;
}

uint32_t CameraReleaseBuffer(CameraHandle handle)
{
	uint32_t ret;
	cam_context_t *pContext = (cam_context_t *)handle;

	if (ioctl(pContext->fd, VIDIOC_QBUF, &(pContext->bufferInfo)) < 0)
	{
		printf("VIDIOC_QBUF error\n");
		ret = 0;
	}
	else
	{
		ret = 1;
	}
	return ret;
}