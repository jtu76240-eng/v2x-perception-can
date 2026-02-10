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
#include <pthread.h>

#include "NnRtpm.h"
#include "perf_api.h"
#include "NnNeuralNetwork.h"

#include "NnSignalHandler.h"

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
pthread_t g_performanceSendThread = (pthread_t)NULL;

/* ========================================================================== */
/*                       Internal Function Declarations                       */
/* ========================================================================== */
void SendCpuUtilization(MessageHandle handle);
void SendMemoryUsage(MessageHandle handle);
void SendNpuUsage(MessageHandle handle);

void *sendPerfmanceDataThread(void *args);

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */
void SendCpuUtilization(MessageHandle handle)
{
	int32_t sizeRet;

	perf_api_t context;

	getCpuUsage(&context);

	sizeRet = RtpmSendCpuUtilization(handle, context.cpuUtil[0]);
	if(sizeRet < 0)
	{
		NN_LOG("[INFO] [%s] RtpmSendCpuUtilization Fail!\n", __FUNCTION__);
	}
}

int32_t RtpmSendCpuUtilization(MessageHandle handle, uint32_t utilization)
{
	int32_t ret = -1;

	message_context_t *pContext = (message_context_t *)handle;

	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_CpuUtilization)];

	memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (pContext->status >= MESSAGE_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_CPU_UTILIZATION;
		msg->length = sizeof(Vision_MSG_CpuUtilization);

		Vision_MSG_CpuUtilization *cpuUtilization = (Vision_MSG_CpuUtilization *)(ctrl_msg + sizeof(vision_message_header_t));
		cpuUtilization->utilization = utilization;

		ret = Vision_API_SendMessage(pContext->messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}

	return ret;
}

void SendMemoryUsage(MessageHandle handle)
{
	int32_t sizeRet;

	perf_api_t context;

	getMemoryUsage(&context);

	sizeRet = RtpmSendSdkMemUsage(handle, context.memUsage);
	if(sizeRet < 0)
	{
		NN_LOG("[INFO] [%s] RtpmSendSdkMemUsage Fail!\n", __FUNCTION__);
	}
}

int32_t RtpmSendSdkMemUsage(MessageHandle handle, uint32_t usage)
{
	int32_t ret = -1;

	message_context_t *pContext = (message_context_t *)handle;

	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_SdkMemUsage)];

	memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (pContext->status >= MESSAGE_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_SDK_MEM_USAGE;
		msg->length = sizeof(Vision_MSG_SdkMemUsage);

		Vision_MSG_SdkMemUsage *memUsage = (Vision_MSG_SdkMemUsage *)(ctrl_msg + sizeof(vision_message_header_t));
		memUsage->usage = usage;

		ret = Vision_API_SendMessage(pContext->messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}
	return ret;
}

void SendNpuUsage(MessageHandle handle)
{
	inference_context_t context;

	getNPUUsage(&context);

	// pthread_mutex_lock(&pContext->npuUsageLocker);

	(void)RtpmSendNpuUsage(handle,
							context.currentNpuUsage[NPU_CLUSTER_INDEX_0].perfDmaUsage,
							context.currentNpuUsage[NPU_CLUSTER_INDEX_0].perfCompUsage,
							context.currentNpuUsage[NPU_CLUSTER_INDEX_1].perfDmaUsage,
							context.currentNpuUsage[NPU_CLUSTER_INDEX_1].perfCompUsage);

	// pthread_mutex_unlock(&pContext->npuUsageLocker);
}

int32_t RtpmSendNpuUsage(MessageHandle handle, uint32_t npu0Dma, uint32_t npu0Comp, uint32_t npu1Dma, uint32_t npu1Comp)
{
	int32_t ret = -1;

	message_context_t *pContext = (message_context_t *)handle;

	uint8_t ctrl_msg[sizeof(vision_message_header_t) + sizeof(Vision_MSG_NpuUsage)];

	memset(ctrl_msg, 0, sizeof(ctrl_msg));

	if (pContext->status >= MESSAGE_STATUS_OPENED)
	{
		vision_message_header_t *msg = (vision_message_header_t *)ctrl_msg;
		msg->id = VISION_MSG_EVENT_NPU_DRIVER_USAGE;
		msg->length = sizeof(Vision_MSG_NpuUsage);

		Vision_MSG_NpuUsage *memUsage = (Vision_MSG_NpuUsage *)(ctrl_msg + sizeof(vision_message_header_t));
		memUsage->npu0Dma  = npu0Dma;
		memUsage->npu0Comp = npu0Comp;
		memUsage->npu1Dma  = npu1Dma;
		memUsage->npu1Comp = npu1Comp;

		ret = Vision_API_SendMessage(pContext->messageHandle, ctrl_msg, sizeof(ctrl_msg), BLOCKING);
	}
	return ret;
}

int32_t RtpmSendInferencePerformanceToRTPM(MessageHandle handle, inference_context_t *pContext)
{
	int32_t sizeRet = -1;

	/* Single network: use NETWORK_INDEX_0 only */
	float maxInferenceTime = (float)pContext->neuralNetwork[NETWORK_INDEX_0].inferenceTime;
	// float maxInferenceTime = pContext->neuralNetwork[NETWORK_INDEX_0].inferenceTime > pContext->neuralNetwork[NETWORK_INDEX_1].inferenceTime
	// 							 ? pContext->neuralNetwork[NETWORK_INDEX_0].inferenceTime
	// 							 : pContext->neuralNetwork[NETWORK_INDEX_1].inferenceTime;

	double fps = 1.0 / (maxInferenceTime / MILLISECONDS_PER_SECOND);

	uint32_t npuUtilization[] = {pContext->neuralNetwork[NETWORK_INDEX_0].npuUtilization};
	uint32_t inferenceTime[] = {pContext->neuralNetwork[NETWORK_INDEX_0].inferenceTime};
	// uint32_t npuUtilization[] = {pContext->neuralNetwork[NETWORK_INDEX_0].npuUtilization, pContext->neuralNetwork[NETWORK_INDEX_1].npuUtilization};
	// uint32_t inferenceTime[] = {pContext->neuralNetwork[NETWORK_INDEX_0].inferenceTime, pContext->neuralNetwork[NETWORK_INDEX_1].inferenceTime};

	sizeRet = RtpmSendResultPerfAsJson(handle, 1, npuUtilization, inferenceTime, fps);

	return sizeRet;
}

int32_t RtpmSendResultPerfAsJson(MessageHandle handle, uint16_t totalCount, uint32_t *utilization, uint32_t *inferenceTime, double fps)
{
	int32_t ret = -1;
	uint16_t i;
	const char *stream = NULL;

	message_context_t *pContext = (message_context_t *)handle;

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

			ret = Vision_API_SendMessage(pContext->messageHandle, combinedMsg, totalSize, BLOCKING);

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

int32_t RtpmPostProcessClassifierResults(network_context_t *pNeural_network, message_result_type_t *pResultType, int32_t *pObjCount, int32_t networkIdx)
{
	int32_t class_ids = pNeural_network->resultCls.class_ids[0];
	pResultType[networkIdx] = MESSAGE_TYPE_CL;
	pObjCount[networkIdx] = class_ids;

	return 0;
}

int32_t RtpmPostProcessDetectionResults(boxes_t *pObjs, network_context_t *pNeural_network, int32_t *pObjCount, message_result_type_t *pResultType, int32_t networkIdx, uint32_t outputRtpmFrameWidth, uint32_t outputRtpmFrameHeight)
{
	float wScale = 1;
	float hScale = 1;

	pResultType[networkIdx] = MESSAGE_TYPE_OD;
	pObjCount[networkIdx] = pNeural_network->resultObj.cnt;

	if (pObjCount[networkIdx] > 0)
	{
		wScale = (float)outputRtpmFrameWidth / (float)pNeural_network->resultObj.obj[0].img_w;
		hScale = (float)outputRtpmFrameHeight / (float)pNeural_network->resultObj.obj[0].img_h;
	}
	else
	{
		// TBD
	}

	for (int boxCnt = 0; boxCnt < pObjCount[networkIdx]; boxCnt++)
	{
		pObjs[networkIdx].boxes[boxCnt].xMin = pNeural_network->resultObj.obj[boxCnt].x_min * wScale;
		pObjs[networkIdx].boxes[boxCnt].yMin = pNeural_network->resultObj.obj[boxCnt].y_min * hScale;
		pObjs[networkIdx].boxes[boxCnt].xMax = pNeural_network->resultObj.obj[boxCnt].x_max * wScale;
		pObjs[networkIdx].boxes[boxCnt].yMax = pNeural_network->resultObj.obj[boxCnt].y_max * hScale;
		pObjs[networkIdx].boxes[boxCnt].cls = pNeural_network->resultObj.obj[boxCnt].cls;
		pObjs[networkIdx].boxes[boxCnt].score = pNeural_network->resultObj.obj[boxCnt].score * PERCENTAGE_CONVERSION_FACTOR;
	}

	return 0;
}

int32_t RtpmSendResultDataAsJson(MessageHandle handle, uint64_t syncStamp, message_result_type_t cl1Type, uint16_t cl1Count, box_t *cl1Result, message_result_type_t cl2Type, uint16_t cl2Count, box_t *cl2Result)
{
	int32_t ret = -1;
	uint16_t i;
	const char *stream = NULL;

	message_context_t *pContext = (message_context_t *)handle;

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
	if(cl1Type == MESSAGE_TYPE_CL)
	{
		json_object_object_add(cluster1Obj, "type", json_object_new_int(cl1Type));
		json_object_object_add(cluster1Obj, "cl", json_object_new_int(cl1Count));
	}
	else // MESSAGE_TYPE_OD
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
	if(cl2Type == MESSAGE_TYPE_CL)
	{
		json_object_object_add(cluster2Obj, "type", json_object_new_int(cl2Type));
		json_object_object_add(cluster2Obj, "cl", json_object_new_int(cl2Count));
	}
	else // MESSAGE_TYPE_OD
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

			ret = Vision_API_SendMessage(pContext->messageHandle, combinedMsg, totalSize, BLOCKING);
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

NNAPP_ERRORTYPE RtpmCreatePerfmanceDataSendThread(MessageHandle msgHandle)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	err = pthread_create(&g_performanceSendThread, NULL, sendPerfmanceDataThread, msgHandle);

	if(err == NNAPP_NO_ERROR)
	{
		NN_LOG("[INFO] [%s] create PerfmanceDataSendThread Done!\n", __FUNCTION__);
	}
	else
	{
		NN_LOG("[INFO] [%s] create PerfmanceDataSendThread Fail!\n", __FUNCTION__);
	}

	return err;
}

NNAPP_ERRORTYPE RtpmDestroyPerfmanceDataSendThread()
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	if (g_performanceSendThread != 0)
	{

		if (pthread_join(g_performanceSendThread, NULL) != 0)
		{
			NN_LOG("[ERROR] [%s] Failed to join performance send thread!\n", __FUNCTION__);
			err = -1;
		}
		else
		{
			g_performanceSendThread = 0;
			NN_LOG("[INFO] [%s] Performance send thread joined successfully.\n", __FUNCTION__);
		}
	}
	else
	{
		NN_LOG("[WARNING] [%s] Performance send thread is not running or already joined.\n", __FUNCTION__);
	}

	NN_LOG("[INFO] [%s] destroy PerfmanceDataSendThread Done!\n", __FUNCTION__);

	return err;
}

void *sendPerfmanceDataThread(void *args)
{
	MessageHandle msgHandle = (MessageHandle)args;
	NN_LOG("[INFO] [%s] start!\n", __FUNCTION__);

	while (NnCheckExitFlag() != true)
	{
		// if rtpm out, send to data using v.p

		SendCpuUtilization(msgHandle);
		SendMemoryUsage(msgHandle);
		SendNpuUsage(msgHandle);

		// else if standalone, print log data using uart

		usleep(500 * 1000);
	}
	if(NnCheckExitFlag())
	{
		printf("[INFO] [%s] end!\n", __FUNCTION__);
	}
	return NULL;
}
