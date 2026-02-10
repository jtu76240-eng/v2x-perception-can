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

#ifndef __NNRTPM_H__
#define __NNRTPM_H__

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "NnDebug.h"
#include "NnType.h"
#include "NnError.h"

#include "message_api.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define RTPM_STEAM_PORT 9998
#define RTPM_MESSAGE_PORT 9999

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
int32_t RtpmSendCpuUtilization(MessageHandle handle, uint32_t utilization);
int32_t RtpmSendSdkMemUsage(MessageHandle handle, uint32_t usage);
int32_t RtpmSendNpuUsage(MessageHandle handle, uint32_t npu0Dma, uint32_t npu0Comp, uint32_t npu1Dma, uint32_t npu1Comp);
int32_t RtpmSendInferencePerformanceToRTPM(MessageHandle handle, inference_context_t *pContext);
int32_t RtpmSendResultPerfAsJson(MessageHandle handle, uint16_t totalCount, uint32_t *utilization, uint32_t *inferenceTime, double fps);
int32_t RtpmPostProcessClassifierResults(network_context_t *pNeural_network, message_result_type_t *pResultType, int32_t *pObjCount, int32_t networkIdx);
int32_t RtpmPostProcessDetectionResults(boxes_t *pObjs, network_context_t *pNeural_network, int32_t *pObjCount, message_result_type_t *pResultType, int32_t networkIdx, uint32_t outputRtpmFrameWidth, uint32_t outputRtpmFrameHeight);
int32_t RtpmSendResultDataAsJson(MessageHandle handle, uint64_t syncStamp, message_result_type_t cl1Type, uint16_t cl1Count, box_t *cl1Result, message_result_type_t cl2Type, uint16_t cl2Count, box_t *cl2Result);

NNAPP_ERRORTYPE RtpmCreatePerfmanceDataSendThread(MessageHandle msgHandle);
NNAPP_ERRORTYPE RtpmDestroyPerfmanceDataSendThread();

#endif //__NNRTPM_H__