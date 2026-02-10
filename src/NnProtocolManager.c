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

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <json-c/json.h>
#include "NnDeWarp.h"
#include <vision_api.h>
#include <NnProtocolManager.h>
#include <math.h>
#include <NnDebug.h>

#define VISION_PROTOCOL_STREAM_QUEUE_COUNT 4
#define VISION_PROTOCOL_CTRL_QUEUE_COUNT 6
#define VISION_PROTOCOL_CTRL_DATA_SIZE (1024 * 320)

typedef struct protocolmanager_buffer
{
	uint32_t length;
	uint64_t phyAddr;
	uint8_t *virAddr;
	uint64_t syncStamp;
} protocolmanager_buffer_t;

typedef struct protocolmanager_context
{
	protocolmanager_status_t status;
	pthread_t threadId;
	bool threadStatus;
	/* vision protocol*/
	void *messageHandle;
	void *streamHandle;
	protocolmanager_buffer_t *pmRecvBuffer;
	protocolmanager_buffer_t *pmSendBuffer;
	int32_t recvBufferCount;
	int32_t sendBufferCount;
	queue_info_t recvStreamInfo;
	queue_info_t controlQue;
	queue_info_t streamQue;
	vision_stream_info_t *popInfo[VISION_PROTOCOL_STREAM_QUEUE_COUNT];

} protocolmanager_context_t;

void *MsgRecvThreadFunction(void *data);
static protocolmanager_context_t protocolmanagerContext = {
	0,
};

static uint32_t sentSyncStamp = 0;

protocolmanager_status_t ProtocolmanagerGetStatus(void)
{
	return protocolmanagerContext.status;
}

int32_t ProtocolmanagerOpen(protocolmanager_stream_status_t streamMode, uint64_t *baseOffset, int32_t bufferCount, uint32_t bufferSize)
{
	int32_t ret = -1;

	if (protocolmanagerContext.status == PROTOCOLMANAGER_STATUS_IDLE)
	{
		int32_t bufIndex;

		vision_user_config_t messageConfig;
		memset(&messageConfig, 0, sizeof(vision_user_config_t));
		messageConfig.netInterface 			= INTERFACE_ETHERNET;
		messageConfig.netRole 				= CLIENT;
		messageConfig.netBuf.sendBufSize	= SYS_DEFAULT_BUFSIZE;
		messageConfig.netBuf.recvBufSize	= SYS_DEFAULT_BUFSIZE;
		strcpy(messageConfig.ip, "192.168.0.8");
		messageConfig.port 					= 9999;
		messageConfig.reconnection 			= OFF;
		messageConfig.opMode 				= MESSAGE_MODE;

		messageConfig.sendQ.maxDataSize 	= VISION_PROTOCOL_CTRL_DATA_SIZE;
		messageConfig.sendQ.numQ 			= VISION_PROTOCOL_CTRL_QUEUE_COUNT;
		messageConfig.recvQ.maxDataSize 	= VISION_PROTOCOL_CTRL_DATA_SIZE;
		messageConfig.recvQ.numQ 			= VISION_PROTOCOL_CTRL_QUEUE_COUNT;

		ret = Vision_API_Initialization(&messageConfig, &protocolmanagerContext.messageHandle);
		if(ret == 0)
		{
			ret = Vision_API_Connection(protocolmanagerContext.messageHandle);
			if(ret == 0)
			{
				protocolmanagerContext.status = PROTOCOLMANAGER_STATUS_OPENED;
			}
			else
			{
				printf("[%s][Error] Vision_API_Connection(MESSAGE_HANDLE)\n", __FUNCTION__);
			}
		}
		else
		{
			printf("[%s][Error] Vision_API_Initialization(MESSAGE_HANDLE)\n", __FUNCTION__);
		}

		vision_user_config_t streamConfig;
		memset(&streamConfig, 0, sizeof(vision_user_config_t));
		streamConfig.netInterface 			= INTERFACE_ETHERNET;
		streamConfig.netRole 				= CLIENT;
		streamConfig.netBuf.sendBufSize		= SYS_DEFAULT_BUFSIZE;
		streamConfig.netBuf.recvBufSize		= SYS_DEFAULT_BUFSIZE;
		strcpy(streamConfig.ip, "192.168.0.8");
		streamConfig.port 					= 9998;
		streamConfig.reconnection 			= OFF;
		streamConfig.streamZeroCopy			= OFF;
		streamConfig.opMode 				= STREAM_MODE;

		if(streamMode == PROTOCOLMANAGER_STREAM_OFF) // stream mode off
		{
			streamConfig.sendQ.maxDataSize 		= 0;
			streamConfig.sendQ.numQ 			= 0;
			streamConfig.recvQ.maxDataSize 		= 0;
			streamConfig.recvQ.numQ 			= 0;
		}
		else if(streamMode == PROTOCOLMANAGER_STREAM_SEND) // projection mode
		{
			streamConfig.sendQ.maxDataSize 		= bufferSize;
			streamConfig.sendQ.numQ 			= bufferCount;
			streamConfig.recvQ.maxDataSize 		= 0;
			streamConfig.recvQ.numQ 			= 0;
		}
		else // injection mode (PROTOCOLMANAGER_STREAM_RECV)
		{
			streamConfig.sendQ.maxDataSize 		= 0;
			streamConfig.sendQ.numQ 			= 0;
			streamConfig.recvQ.maxDataSize 		= bufferSize;
			streamConfig.recvQ.numQ 			= bufferCount;
		}

		if((protocolmanagerContext.status == PROTOCOLMANAGER_STATUS_OPENED) && (streamMode > 0) && (bufferCount > 0) && (bufferSize > 0))
		{
			ret = Vision_API_Initialization(&streamConfig, &protocolmanagerContext.streamHandle);
			if(ret == 0)
			{
				if(streamMode == PROTOCOLMANAGER_STREAM_SEND) // projection
				{
					protocolmanagerContext.sendBufferCount = streamConfig.sendQ.numQ;
					protocolmanagerContext.pmSendBuffer = (protocolmanager_buffer_t *)malloc(sizeof(protocolmanager_buffer_t) * streamConfig.sendQ.numQ);
					for (bufIndex = 0; bufIndex < protocolmanagerContext.sendBufferCount; bufIndex++)
					{
						int32_t memFd = 0;
						int32_t retIndex = -1;
						protocolmanagerContext.pmSendBuffer[bufIndex].length = bufferSize;
						protocolmanagerContext.pmSendBuffer[bufIndex].phyAddr = baseOffset[bufIndex];

						memFd = open("/dev/mem", O_RDWR | O_SYNC);
						if (memFd < 0)
						{
							printf("[%s] mem open error\n", __FUNCTION__);
							ret = -1;
							break;
						}
						else
						{
							protocolmanagerContext.pmSendBuffer[bufIndex].virAddr = (uint8_t *)mmap(NULL, protocolmanagerContext.pmSendBuffer[bufIndex].length, PROT_READ | PROT_WRITE, MAP_SHARED, memFd, (off_t)protocolmanagerContext.pmSendBuffer[bufIndex].phyAddr);
							if (protocolmanagerContext.pmSendBuffer[bufIndex].virAddr == NULL)
							{
								printf("[%s] mmap failed!!\n", __FUNCTION__);
								exit(0);
							}
							
							ret = Vision_API_RegisterStreamSendBuffer(protocolmanagerContext.streamHandle, protocolmanagerContext.pmSendBuffer[bufIndex].virAddr, protocolmanagerContext.pmSendBuffer[bufIndex].length);
							if(ret == 0)
							{
								printf("[%s, SendBuf] [vir:%p] [phy:%p] [size:%d] [buf idx:%d]\n", __FUNCTION__, protocolmanagerContext.pmSendBuffer[bufIndex].virAddr, protocolmanagerContext.pmSendBuffer[bufIndex].phyAddr, protocolmanagerContext.pmSendBuffer[bufIndex].length, bufIndex);
							}
							else
							{
								printf("[%s, SendBuf] Error [buf idx:%d]\n", __FUNCTION__, bufIndex);
							}
							close(memFd);
						}
					}

					if (bufIndex == protocolmanagerContext.sendBufferCount)
					{
						ret = 0;
					}
					else
					{
						ret = -1;
					}
				}
				else if(streamMode == PROTOCOLMANAGER_STREAM_RECV) // injection
				{
					protocolmanagerContext.recvBufferCount = streamConfig.recvQ.numQ;
					protocolmanagerContext.pmRecvBuffer = (protocolmanager_buffer_t *)malloc(sizeof(protocolmanager_buffer_t) * streamConfig.recvQ.numQ);
					for (bufIndex = 0; bufIndex < protocolmanagerContext.recvBufferCount; bufIndex++)
					{
						int32_t memFd = 0;
						int32_t retIndex = -1;
						protocolmanagerContext.pmRecvBuffer[bufIndex].length = bufferSize;
						protocolmanagerContext.pmRecvBuffer[bufIndex].phyAddr = baseOffset[bufIndex];

						memFd = open("/dev/mem", O_RDWR | O_SYNC);
						if (memFd < 0)
						{
							printf("[%s] mem open error\n", __FUNCTION__);
							ret = -1;
							break;
						}
						else
						{
							protocolmanagerContext.pmRecvBuffer[bufIndex].virAddr = (uint8_t *)mmap(NULL, protocolmanagerContext.pmRecvBuffer[bufIndex].length, PROT_READ | PROT_WRITE, MAP_SHARED, memFd, (off_t)protocolmanagerContext.pmRecvBuffer[bufIndex].phyAddr);
							if (protocolmanagerContext.pmRecvBuffer[bufIndex].virAddr == NULL)
							{
								printf("[%s] mmap failed!!\n", __FUNCTION__);
								exit(0);
							}

							ret = Vision_API_RegisterStreamRecvBuffer(protocolmanagerContext.streamHandle, protocolmanagerContext.pmRecvBuffer[bufIndex].virAddr, protocolmanagerContext.pmRecvBuffer[bufIndex].length);
							if(ret == 0)
							{
								printf("[%s, RecvBuf] [vir:%p] [phy:%p] [size:%d] [buf idx:%d]\n", __FUNCTION__, protocolmanagerContext.pmRecvBuffer[bufIndex].virAddr, protocolmanagerContext.pmRecvBuffer[bufIndex].phyAddr, protocolmanagerContext.pmRecvBuffer[bufIndex].length, bufIndex);
							}
							else
							{
								printf("[%s, RecvBuf] Error [buf idx:%d]\n", __FUNCTION__, bufIndex);
							}
							close(memFd);
						}
					}

					if (bufIndex == protocolmanagerContext.recvBufferCount)
					{
						ret = 0;
					}
					else
					{
						ret = -1;
					}
				}

				if(ret == 0)
				{
					printf("[%s] wait for target signal ...\n", __FUNCTION__);
					ret = Vision_API_Connection(protocolmanagerContext.streamHandle);
					if(ret == 0)
					{
						protocolmanagerContext.status = PROTOCOLMANAGER_STATUS_STREAMING;
					}
				}

				// Message Recv Thread Create
				protocolmanagerContext.threadStatus = true;
				pthread_create(&(protocolmanagerContext.threadId), NULL, MsgRecvThreadFunction, (void *)&protocolmanagerContext);
			}
		}
		else
		{
			printf("[%s] Not used streaming\n", __FUNCTION__);
			protocolmanagerContext.status = PROTOCOLMANAGER_STATUS_OPENED;
			ret = 0;
		}
	}
	return ret;
}

int32_t ProtocolmanagerClose(void)
{
	int32_t ret = -1;

	if ((protocolmanagerContext.status > PROTOCOLMANAGER_STATUS_IDLE) && (protocolmanagerContext.status <= PROTOCOLMANAGER_STATUS_STREAMING))
	{
		// message recv thread exit
		protocolmanagerContext.threadStatus = false;
		pthread_join(protocolmanagerContext.threadId, NULL);

		Vision_API_Disconnection(protocolmanagerContext.messageHandle); 
		Vision_API_Deinitialization(&protocolmanagerContext.messageHandle);
		if (protocolmanagerContext.status == PROTOCOLMANAGER_STATUS_STREAMING)
		{
			Vision_API_Disconnection(protocolmanagerContext.streamHandle);
			Vision_API_Deinitialization(&protocolmanagerContext.streamHandle);

			if (protocolmanagerContext.pmRecvBuffer != NULL)
			{
				int32_t bufIndex, res;
				for (bufIndex = 0; bufIndex < protocolmanagerContext.recvBufferCount; bufIndex++)
				{
					res = munmap(protocolmanagerContext.pmRecvBuffer[bufIndex].virAddr, protocolmanagerContext.pmRecvBuffer[bufIndex].length);
					printf("[%s] Recv Buffer [vir:%p] [phy:%p]\n", __FUNCTION__, protocolmanagerContext.pmRecvBuffer[bufIndex].virAddr, protocolmanagerContext.pmRecvBuffer[bufIndex].phyAddr);
					protocolmanagerContext.pmRecvBuffer[bufIndex].virAddr = NULL;
					if (res < 0)
					{
						printf("[%s] munmap error [index : %d]\n", __FUNCTION__, bufIndex);
					}
				}
				free(protocolmanagerContext.pmRecvBuffer);
				protocolmanagerContext.pmRecvBuffer = NULL;
			}

			if (protocolmanagerContext.pmSendBuffer != NULL)
			{
				int32_t bufIndex, res;
				for (bufIndex = 0; bufIndex < protocolmanagerContext.sendBufferCount; bufIndex++)
				{
					res = munmap(protocolmanagerContext.pmSendBuffer[bufIndex].virAddr, protocolmanagerContext.pmSendBuffer[bufIndex].length);
					printf("[%s] Send Buffer [vir:%p] [phy:%p]\n", __FUNCTION__, protocolmanagerContext.pmSendBuffer[bufIndex].virAddr, protocolmanagerContext.pmSendBuffer[bufIndex].phyAddr);
					protocolmanagerContext.pmSendBuffer[bufIndex].virAddr = NULL;
					if (res < 0)
					{
						printf("[%s] munmap error [index : %d]\n", __FUNCTION__, bufIndex);
					}
				}
				free(protocolmanagerContext.pmSendBuffer);
				protocolmanagerContext.pmSendBuffer = NULL;
			}
		}
		protocolmanagerContext.messageHandle = NULL;
		protocolmanagerContext.streamHandle = NULL;
		protocolmanagerContext.status = PROTOCOLMANAGER_STATUS_IDLE;
		ret = 0;
	}
	else
	{
		printf("[%s] Vision Protocol close error\n", __FUNCTION__);
	}

	return ret;
}

int32_t ProtocolmanagerPopReceiveBuffer(uint8_t **virtualAddr, uint64_t *baseOffset, uint64_t *syncStamp)
{
	int32_t ret = -1;

	*virtualAddr = NULL;
	*baseOffset = 0ull;
	*syncStamp = 0ull;

	if (protocolmanagerContext.status > PROTOCOLMANAGER_STATUS_OPENED)
	{
		uint32_t bufIndex = protocolmanagerContext.recvBufferCount;
		uint8_t *buffer;
		vision_stream_info_t peekInfo;
		vision_stream_info_t *popInfo;

		while(1)
		{
			ret = Vision_API_RecvStream(protocolmanagerContext.streamHandle, &popInfo, &bufIndex, BLOCKING);
			if ((ret == 0) && (bufIndex < protocolmanagerContext.recvBufferCount))
			{
				if (popInfo->pBuffer == protocolmanagerContext.pmRecvBuffer[bufIndex].virAddr)
				{
					// Need to parsing header
					*virtualAddr = protocolmanagerContext.pmRecvBuffer[bufIndex].virAddr;
					*baseOffset = protocolmanagerContext.pmRecvBuffer[bufIndex].phyAddr;
					*syncStamp = popInfo->seqNum;
					// size = protocolmanagerContext.pmRecvBuffer[bufIndex].length;		  // popInfo->bufferSize
					protocolmanagerContext.popInfo[bufIndex] = popInfo;
					break;
				}
				else
				{
					printf("[%s] Return buffer doesn't match with buffer at index\n", __FUNCTION__);
				}
			}
			else
			{
				printf("[%s] Invalid buffer index\n", __FUNCTION__);
			}
		}
	}
	else
	{
		printf("[%s] Device is not opened\n", __FUNCTION__);
	}
	return ret;
}

int32_t ProtocolmanagerPushReceiveBuffer(uint8_t *virtualAddr, uint64_t baseOffset)
{
	int32_t ret = -1;

	if (protocolmanagerContext.status > PROTOCOLMANAGER_STATUS_OPENED)
	{
		uint32_t bufIndex;
		for (bufIndex = 0; bufIndex < protocolmanagerContext.recvBufferCount; bufIndex++)
		{
			if (protocolmanagerContext.pmRecvBuffer[bufIndex].phyAddr == baseOffset)
			{
				ret = Vision_API_ReleaseStreamRecvBuffer(protocolmanagerContext.streamHandle, protocolmanagerContext.popInfo[bufIndex], bufIndex);
				if (ret < 0)
				{
					printf("[%s] Push buffer error [index : %d]\n", __FUNCTION__, bufIndex);
				}
				break;
			}
		}
	}
	else
	{
		printf("[%s] Device is not opened\n", __FUNCTION__);
	}

	return ret;
}

int32_t ProtocolmanagerPopSendBuffer(uint8_t **virtualAddr, uint64_t *baseOffset, uint32_t *bufferIndex)
{
	int32_t ret = -1;
	vision_stream_info_t *popInfo;
	uint8_t idx;
	int cnt = 0;
	*virtualAddr = NULL;
	*baseOffset = 0ull;

	if (protocolmanagerContext.status > PROTOCOLMANAGER_STATUS_OPENED)
	{
		ret = Vision_API_GetStreamSendBuffer(protocolmanagerContext.streamHandle, &popInfo, &idx, BLOCKING);
		if(ret == 0)
		{
			popInfo->id = 0x11;
			popInfo->seqNum = sentSyncStamp++;
			popInfo->timestamp = 0;

			// printf("[%s] Pop Buffer : %p / %d\n", __FUNCTION__,  popInfo->pBuffer, idx);
			if (idx < protocolmanagerContext.sendBufferCount)
			{
				if (popInfo->pBuffer == protocolmanagerContext.pmSendBuffer[idx].virAddr)
				{
					// Need to parsing header
					*virtualAddr = protocolmanagerContext.pmSendBuffer[idx].virAddr;
					*baseOffset = protocolmanagerContext.pmSendBuffer[idx].phyAddr;
					// size = protocolmanagerContext.pmSendBuffer[*bufferIndex].length;
					protocolmanagerContext.popInfo[idx] = popInfo;
					*bufferIndex = idx;
				}
				else
				{
					printf("[%s] Return buffer doesn't match with buffer at index\n", __FUNCTION__);
				}
			}
			else
			{
				printf("[%s] Invalid buffer index\n", __FUNCTION__);
			}
		}
	}
	else
	{
		printf("[%s] Device is not opened\n", __FUNCTION__);
	}

	return ret;
}

int32_t ProtocolmanagerPushSendBuffer(uint8_t *virtualAddr, uint32_t bufferIndex)
{
	int32_t ret = -1;
	if (protocolmanagerContext.status > PROTOCOLMANAGER_STATUS_OPENED)
	{
		if (bufferIndex < protocolmanagerContext.sendBufferCount)
		{
			if (virtualAddr == protocolmanagerContext.pmSendBuffer[bufferIndex].virAddr)
			{
				ret = Vision_API_SendStream(protocolmanagerContext.streamHandle, protocolmanagerContext.popInfo[bufferIndex], bufferIndex);
				if (ret < 0)
				{
					printf("[%s] Push buffer error [addr : %p][index : %d]\n", __FUNCTION__, virtualAddr, bufferIndex);
				}
			}
			else
			{
				printf("[%s] mismatch buffer index and address\n", __FUNCTION__);
			}
		}
		else
		{
			printf("[%s] Invalid buffer index\n", __FUNCTION__);
		}
	}
	else
	{
		printf("[%s] Device is not opened\n", __FUNCTION__);
	}

	return ret;
}

int32_t ProtocolmanagerSendResultDataAsJson(uint64_t syncStamp, protocolmanager_result_type_t cl1Type, uint16_t cl1Count, box_t *cl1Result, protocolmanager_result_type_t cl2Type, uint16_t cl2Count, box_t *cl2Result)
{
	int32_t ret = -1;
	uint16_t i, pi;
	char *stream = NULL;
	// Create json object
	json_object *mainObj = json_object_new_object();
	json_object *cluster1Obj = json_object_new_object();
	json_object *cluster2Obj = json_object_new_object();
	json_object *od1Ary = json_object_new_array();
	json_object *od2Ary = json_object_new_array();
	json_object *dataAry = json_object_new_array();

	// Add info
	json_object_array_add(dataAry, json_object_new_int(syncStamp));
	// json_object_array_add(dataAry, json_object_new_int(odCount));
	json_object_object_add(mainObj, "info", dataAry);

	// Add cluster1 result
	if(cl1Type == PROTOCOLMANAGER_TYPE_CL)
	{
		json_object_object_add(cluster1Obj, "type", json_object_new_int(cl1Type));
		json_object_object_add(cluster1Obj, "cl", json_object_new_int(cl1Count));
	}
	else // PROTOCOLMANAGER_TYPE_OD
	{
		json_object_object_add(cluster1Obj, "type", json_object_new_int(cl1Type));
		if ((cl1Count > 0) && (cl1Result != NULL))
		{
			for (i = 0; i < cl1Count; i++)
			{
				json_object *odDataObj = json_object_new_object();
				json_object *boxAry = json_object_new_array();
				json_object_object_add(odDataObj, "id", json_object_new_int(i));
				json_object_object_add(odDataObj, "category_id", json_object_new_int(cl1Result[i].cls - 1));
				json_object_object_add(odDataObj, "score", json_object_new_double(cl1Result[i].score));
				json_object_array_add(boxAry, json_object_new_int(round(cl1Result[i].xMin)));
				json_object_array_add(boxAry, json_object_new_int(round(cl1Result[i].yMin)));
				json_object_array_add(boxAry, json_object_new_int(round(cl1Result[i].xMax - cl1Result[i].xMin)));
				json_object_array_add(boxAry, json_object_new_int(round(cl1Result[i].yMax - cl1Result[i].yMin)));
				json_object_object_add(odDataObj, "bbox", boxAry);
				json_object_object_add(odDataObj, "desc", json_object_new_string(cl1Result[i].label));
				json_object_object_add(odDataObj, "distance", json_object_new_double(cl1Result[i].distance));

				json_object_array_add(od1Ary, odDataObj);
			}
		}
		json_object_object_add(cluster1Obj, "od", od1Ary);
	}
	json_object_object_add(mainObj, "cluster1", cluster1Obj);

	// Add cluster2 result
	if(cl2Type == PROTOCOLMANAGER_TYPE_CL)
	{
		json_object_object_add(cluster2Obj, "type", json_object_new_int(cl2Type));
		json_object_object_add(cluster2Obj, "cl", json_object_new_int(cl2Count));
	}
	else // PROTOCOLMANAGER_TYPE_OD
	{
		json_object_object_add(cluster2Obj, "type", json_object_new_int(cl2Type));
		if ((cl2Count > 0) && (cl2Result != NULL))
		{
			for (i = 0; i < cl2Count; i++)
			{
				json_object *odDataObj = json_object_new_object();
				json_object *boxAry = json_object_new_array();
				json_object_object_add(odDataObj, "id", json_object_new_int(i));
				json_object_object_add(odDataObj, "category_id", json_object_new_int(cl2Result[i].cls - 1));
				json_object_object_add(odDataObj, "score", json_object_new_double(cl2Result[i].score));
				json_object_array_add(boxAry, json_object_new_int(round(cl2Result[i].xMin)));
				json_object_array_add(boxAry, json_object_new_int(round(cl2Result[i].yMin)));
				json_object_array_add(boxAry, json_object_new_int(round(cl2Result[i].xMax - cl2Result[i].xMin)));
				json_object_array_add(boxAry, json_object_new_int(round(cl2Result[i].yMax - cl2Result[i].yMin)));
				json_object_object_add(odDataObj, "bbox", boxAry);
				json_object_object_add(odDataObj, "desc", json_object_new_string(cl2Result[i].label));
				json_object_object_add(odDataObj, "distance", json_object_new_double(cl2Result[i].distance));

				json_object_array_add(od2Ary, odDataObj);
			}
		}
		json_object_object_add(cluster2Obj, "od", od2Ary);
	}
	json_object_object_add(mainObj, "cluster2", cluster2Obj);

	// Get String of json type
	stream = json_object_get_string(mainObj);
	if (stream != NULL)
	{
		vision_message_header_t msg;
		memset(&msg, 0, sizeof(vision_message_header_t));
		msg.id = VISION_MSG_EVENT_RESULT_DATA_JSON;
		msg.length = strnlen(stream, VISION_PROTOCOL_CTRL_DATA_SIZE);

		size_t totalSize = sizeof(vision_message_header_t) + msg.length;
		void *combinedMsg = malloc(totalSize);
		if (combinedMsg != NULL)
        {
            memcpy(combinedMsg, &msg, sizeof(vision_message_header_t));
            memcpy(combinedMsg + sizeof(vision_message_header_t), stream, msg.length);

			ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, combinedMsg, totalSize, BLOCKING);
            free(combinedMsg);
        }
		else
        {
            printf("[%s] is failed, memory allocation error\n", __FUNCTION__);
        }
		
		// printf(" json size : %d / send size : %d / OD : %d / LD : %d\n", strnlen(stream, 1024*1024), size, odCount, ldCount);
		// printf("%s\n", stream);
	}
	else
	{
		printf("[%s] make json error : %p\n", __FUNCTION__, stream);
	}

	// free json object
	json_object_put(mainObj);

	return ret;
}

int32_t ProtocolmanagerSendResultPerfAsJson(uint16_t totalCount, uint32_t *utilization, uint32_t *inferenceTime, double fps)
{
	int32_t ret = -1;
	uint16_t i;
	char *stream = NULL;
	
	// Create json object
	json_object *mainObj = json_object_new_object();
	json_object *utilAry = json_object_new_array();
	json_object *infTAry = json_object_new_array();

	// Add fps
	json_object_object_add(mainObj, "fps", json_object_new_double(fps));

	// Add inferenceTime and utilization
	if ((totalCount > 0) && (utilization != NULL))
	{
		for (i = 0; i < totalCount; i++)
		{
			json_object_array_add(infTAry, json_object_new_int(inferenceTime[i]));
			json_object_array_add(utilAry, json_object_new_int(utilization[i]));
		}
	}
	json_object_object_add(mainObj, "inferenceTime", infTAry);
	json_object_object_add(mainObj, "utilization", utilAry);

	// Get String of json type
	stream = json_object_get_string(mainObj);
	if (stream != NULL)
	{
		vision_message_header_t msg;
		memset(&msg, 0, sizeof(vision_message_header_t));
		msg.id = VISION_MSG_EVENT_RESULT_PERF_JSON;
		msg.length = strnlen(stream, VISION_PROTOCOL_CTRL_DATA_SIZE);

		size_t totalSize = sizeof(vision_message_header_t) + msg.length;
		void *combinedMsg = malloc(totalSize);
		if (combinedMsg != NULL)
        {
			memcpy(combinedMsg, &msg, sizeof(vision_message_header_t));
            memcpy(combinedMsg + sizeof(vision_message_header_t), stream, msg.length);

			ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, combinedMsg, totalSize, BLOCKING);

            free(combinedMsg);
		}
		else
        {
            printf("[%s] is failed, memory allocation error\n", __FUNCTION__);
        }

		// printf(" json size : %d / send size : %d\n", msg.length, size);
		// printf("%s\n", stream);
	}
	else
	{
		printf("[%s] make json error : %p\n", __FUNCTION__, stream);
	}

	// free json object
	json_object_put(mainObj);

	return ret;
}

int32_t ProtocolmanagerSendODResult(uint64_t syncStamp, uint16_t totalCount, box_t *odResult)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_NpuInferenceODBoundingBox)];

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		uint16_t i;
		if (totalCount > 0)
		{
			for (i = 0; i < totalCount; i++)
			{
				memset(ctrl_msg, 0, sizeof(ctrl_msg));

				msg->id = VISION_MSG_EVENT_NPU_INFERENCE_OD_BOUNDINGBOX;
				msg->length = sizeof(Vision_MSG_NpuInferenceODBoundingBox);

				Vision_MSG_NpuInferenceODBoundingBox *odResultMsg = (Vision_MSG_NpuInferenceODBoundingBox *)(ctrl_msg + sizeof(vision_message_header_t));

				odResultMsg->Total = totalCount;
				odResultMsg->current = i + 1;
				odResultMsg->xmin = odResult[i].xMin;
				odResultMsg->ymin = odResult[i].yMin;
				odResultMsg->xmax = odResult[i].xMax;
				odResultMsg->ymax = odResult[i].yMax;
				odResultMsg->score = odResult[i].score;
				odResultMsg->classId = odResult[i].cls;
				odResultMsg->distance = odResult[i].distance;
				odResultMsg->speed = odResult[i].speed;
				memcpy(odResultMsg->label, odResult[i].label, sizeof(odResultMsg->label));

				ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
				// printf("[%s] %d %d %d %d %d %d / %d\n", __FUNCTION__, odResultMsg.xmin, odResultMsg.ymin, odResultMsg.xmax, odResultMsg.ymax, odResultMsg.score, odResultMsg.classId, size);
			}
		}
		else
		{
			memset(ctrl_msg, 0, sizeof(ctrl_msg));

			msg->id = VISION_MSG_EVENT_NPU_INFERENCE_OD_BOUNDINGBOX;
			msg->length = sizeof(Vision_MSG_NpuInferenceODBoundingBox);

			Vision_MSG_NpuInferenceODBoundingBox *odResultMsg = (Vision_MSG_NpuInferenceODBoundingBox *)(ctrl_msg + sizeof(vision_message_header_t));
			ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
			printf("[%s] No result box / %d\n", __FUNCTION__, ret);
		}
	}
	return ret;
}

int32_t ProtocolmanagerSendHBAResult(uint64_t syncStamp, uint32_t hbaResult)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_NpuInferenceHBAResult)];
	memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_NPU_INFERENCE_HBA_RESULT;
		msg->length = sizeof(Vision_MSG_NpuInferenceHBAResult);

		Vision_MSG_NpuInferenceHBAResult *hbaResultMsg = (Vision_MSG_NpuInferenceHBAResult *)(ctrl_msg + sizeof(vision_message_header_t));
		hbaResultMsg->Total = 1;
		hbaResultMsg->current = 1;
		hbaResultMsg->EnDisable = hbaResult;

		ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}
	return ret;
}

int32_t ProtocolmanagerSendLDResult(uint64_t syncStamp, uint16_t laneCount, lane_t *ldResult)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_NpuInferenceLDResult)];

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		uint16_t i;
		uint16_t pi;
		if (laneCount > 0)
		{
			// printf("Lane num : %d \n", laneCount);
			for (i = 0; i < laneCount; i++)
			{
				uint16_t pointNum = ldResult[i].pointNum;
				// printf("Point num : %d \n", pointNum);
				for (pi = 0; pi < pointNum; pi++)
				{
					memset(ctrl_msg, 0, sizeof(ctrl_msg));

					msg->id = VISION_MSG_EVENT_NPU_INFERENCE_LD_RESULT;
					msg->length = sizeof(Vision_MSG_NpuInferenceLDResult);

					Vision_MSG_NpuInferenceLDResult *ldResultMsg = (Vision_MSG_NpuInferenceLDResult *)(ctrl_msg + sizeof(vision_message_header_t));
					ldResultMsg->TotalLane = laneCount;
					ldResultMsg->currentLane = i + 1;
					ldResultMsg->TotalPoint = pointNum;
					ldResultMsg->currentPoint = pi + 1;
					ldResultMsg->x = ldResult[i].points[pi].x;
					ldResultMsg->y = ldResult[i].points[pi].y;

					ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
					// printf("[%s] %d %d %d %d / %d\n", __FUNCTION__, i, pi, ldResultMsg.x, ldResultMsg.y, size);
				}
			}
		}
		else
		{
			memset(ctrl_msg, 0, sizeof(ctrl_msg));
			
			msg->id = VISION_MSG_EVENT_NPU_INFERENCE_LD_RESULT;
			msg->length = sizeof(Vision_MSG_NpuInferenceLDResult);

			Vision_MSG_NpuInferenceLDResult *ldResultMsg = (Vision_MSG_NpuInferenceLDResult *)(ctrl_msg + sizeof(vision_message_header_t));
			ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
			printf("[%s] No result lane / %d\n", __FUNCTION__, ret);
		}
	}
	return ret;
}

int32_t ProtocolmanagerSendInferenceTime(uint64_t syncStamp, uint16_t totalCount, uint32_t *inferenceTime)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_NpuInferenceTime)];

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		uint16_t i;
		for (i = 0; i < totalCount; i++)
		{
			memset(ctrl_msg, 0, sizeof(ctrl_msg));

			msg->id = VISION_MSG_EVENT_NPU_INFERENCE_TIME;
			msg->length = sizeof(Vision_MSG_NpuInferenceTime);
			
			Vision_MSG_NpuInferenceTime *npuInferenceTime = (Vision_MSG_NpuInferenceTime *)(ctrl_msg + sizeof(vision_message_header_t));
			npuInferenceTime->Total = totalCount;
			npuInferenceTime->current = i + 1;
			npuInferenceTime->time = inferenceTime[i];

			ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
			// printf("[%s] %d : %d %d / %d\n", __FUNCTION__, i, npuInferenceTime.time, size);
		}
	}
	return ret;
}

int32_t ProtocolmanagerSendNpuUtilization(uint16_t totalCount, uint32_t *utilization)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_NpuUtilization)];

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		uint16_t i;
		for (i = 0; i < totalCount; i++)
		{
			memset(ctrl_msg, 0, sizeof(ctrl_msg));

			msg->id = VISION_MSG_EVENT_NPU_UTILIZATION;
			msg->length = sizeof(Vision_MSG_NpuUtilization);

			Vision_MSG_NpuUtilization *npuUtilization = (Vision_MSG_NpuUtilization *)(ctrl_msg + sizeof(vision_message_header_t));
			npuUtilization->Total = totalCount;
			npuUtilization->current = i + 1;
			npuUtilization->util = utilization[i];

			ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
			// printf("[%s] %d : %d %d / %d\n", __FUNCTION__, i, npuUtilization.util, size);
		}
	}
	return ret;
}

int32_t ProtocolmanagerSendCpuUtilization(uint32_t utilization)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_CpuUtilization)];
    memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_CPU_UTILIZATION;
		msg->length = sizeof(Vision_MSG_CpuUtilization);

		Vision_MSG_CpuUtilization *cpuUtilization = (Vision_MSG_CpuUtilization *)(ctrl_msg + sizeof(vision_message_header_t));
    	cpuUtilization->utilization = utilization;

		ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}

	return ret;
}

int32_t ProtocolmanagerSendSdkMemUsage(uint32_t usage)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_SdkMemUsage)];
    memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_SDK_MEM_USAGE;
		msg->length = sizeof(Vision_MSG_SdkMemUsage);

		Vision_MSG_SdkMemUsage *memUsage = (Vision_MSG_SdkMemUsage *)(ctrl_msg + sizeof(vision_message_header_t));
		memUsage->usage = usage;

		ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}
	return ret;
}

int32_t ProtocolmanagerSendSdkVideoFps(float fps)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_SdkVideoFPS)];
    memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_SDK_VIDEO_FPS;
		msg->length = sizeof(Vision_MSG_SdkVideoFPS);

		Vision_MSG_SdkVideoFPS *videoFps = (Vision_MSG_SdkVideoFPS *)(ctrl_msg + sizeof(vision_message_header_t));
		videoFps->fps = fps;
		
		ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}
	return ret;
}

int32_t ProtocolmanagerSendResultDataEvent(int32_t status)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_ResultEventStartStop)];
    memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_RESULT_STARTSTOP;
		msg->length = sizeof(Vision_MSG_ResultEventStartStop);

		Vision_MSG_ResultEventStartStop *eventStatus = (Vision_MSG_ResultEventStartStop *)(ctrl_msg + sizeof(vision_message_header_t));
		eventStatus->status = status;

		ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}
	return ret;
}

int32_t ProtocolmanagerSendDewarpStatus(uint32_t status)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_DewarpStatus)];
    memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED && IsInitDewarp())
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_DEWARP_STATUS;
		msg->length = sizeof(Vision_MSG_DewarpStatus);

		Vision_MSG_DewarpStatus *eventStatus = (Vision_MSG_DewarpStatus *)(ctrl_msg + sizeof(vision_message_header_t));
		eventStatus->status = status;

		ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}

	return ret;
}

int32_t ProtocolmanagerSendDewarpMsgAsJson(const char *send_stream)
{
	int32_t ret = -1;

	if (send_stream != NULL)
	{
		vision_message_header_t msg;
		memset(&msg, 0, sizeof(vision_message_header_t));
		msg.id = VISION_MSG_EVENT_DEWARP_DATA_MSG;
		msg.length = strnlen(send_stream, VISION_PROTOCOL_CTRL_DATA_SIZE);

		size_t totalSize = sizeof(vision_message_header_t) + msg.length;
		void *combinedMsg = malloc(totalSize);
		if (combinedMsg != NULL)
        {
            memcpy(combinedMsg, &msg, sizeof(vision_message_header_t));
            memcpy(combinedMsg + sizeof(vision_message_header_t), send_stream, msg.length);

            ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, combinedMsg, totalSize, BLOCKING);

            free(combinedMsg);
        }
		else
        {
            printf("[%s] is failed, memory allocation error\n", __FUNCTION__);
        }
	}
	else
	{
		printf("[%s] is failed, send_stream is NULL\n", __FUNCTION__);
	}

	return ret;
}


int32_t ProtocolmanagerSendNpuUsage(uint32_t npu0Dma, uint32_t npu0Comp, uint32_t npu1Dma, uint32_t npu1Comp)
{
	int32_t ret = -1;
	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_NpuUsage)];
	memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (protocolmanagerContext.status >= PROTOCOLMANAGER_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_NPU_DRIVER_USAGE;
		msg->length = sizeof(Vision_MSG_NpuUsage);

		Vision_MSG_NpuUsage *memUsage = (Vision_MSG_NpuUsage *)(ctrl_msg + sizeof(vision_message_header_t));
		memUsage->npu0Dma  = npu0Dma;
		memUsage->npu0Comp = npu0Comp;
		memUsage->npu1Dma  = npu1Dma;
		memUsage->npu1Comp = npu1Comp;

		ret = Vision_API_SendMessage(protocolmanagerContext.messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}
	return ret;
}


void *MsgRecvThreadFunction(void *data)
{
	protocolmanager_context_t *context = (protocolmanager_context_t *)data;
	vision_message_header_t peekHeader;
	vision_message_header_t header;
	char *recv_stream[5000];
	int ret = 0;

	printf("[%s][id:%d] start!\n", __FUNCTION__, gettid());

	while (context->threadStatus)
	{
		// peeking message header info
		ret = Vision_API_PeekMessage(context->messageHandle, &peekHeader, sizeof(vision_message_header_t));
		if (ret == 0) 
		{
			if (peekHeader.id == VISION_MSG_EVENT_DEWARP_DATA_MSG)
			{
				// read message header
				ret = Vision_API_RecvMessage(context->messageHandle, &header, sizeof(vision_message_header_t), BLOCKING);
				if (header.id == VISION_MSG_EVENT_DEWARP_DATA_MSG)
				{
					memset(recv_stream, 0U, 5000);
					ret = Vision_API_RecvMessage(context->messageHandle, recv_stream, header.length, BLOCKING);
					if (ret == 0)
					{
						ReceiveDewapJsonStream(recv_stream);
					}
					else
					{
						printf("[%s ] Failed to receive dewarp parameter, invalid message size: %s\n", recv_stream);
					}
				}		
			}
		}

		usleep(5000);
	}

	printf("[%s][id:%d] end!\n", __FUNCTION__, gettid());
}

