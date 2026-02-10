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

#ifndef __NNNEURALNETWORK_H__
#define __NNNEURALNETWORK_H__

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <pthread.h>

#include "NnType.h"
#include "NnError.h"
#include "NnDebug.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define SO_FILE    "net.so"
#define CMD_FILE   "npu_cmd.bin"
#define PARAM_FILE "quantized_network.bin"

#define DEFAULT_NPU_MONITOR_PERIOD_US (1000*1000)

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
NNAPP_ERRORTYPE NnNpuInit(inference_context_t *pContext, int32_t clusterIndex);
NNAPP_ERRORTYPE NnNpuDeinit(inference_context_t *pContext);
NNAPP_ERRORTYPE NnNeuralNetworkInit(inference_context_t *pContext, npu_cluster_index_t npuClusterIndex, network_index_t networkIndex, uint8_t scalerIndex, image_fmt_t fmt);//, npu_cluster_index_t npuIdx, uint8_t *networkPath)
NNAPP_ERRORTYPE NnNeuralNetworkDeinit(inference_context_t *pContext, network_index_t networkIndex);
NNAPP_ERRORTYPE NnCreateInferenceThread(inference_context_t *pContext);
NNAPP_ERRORTYPE NnDestroyInferenceThread();
void NnRunInference(network_context_t *pNeuralNetwork, npu_run_mode_t npuRunMode);

NNAPP_ERRORTYPE getNPUVariation(npu_t *npuFd, uint8_t npuIndex, npu_usage_info_t *pPrev, npu_usage_info_t *pCurr);
NNAPP_ERRORTYPE NPUUsageInfo(inference_context_t *pContext);
void setNPUUsage(uint8_t npuIndex, npu_usage_info_t currUsage);
void getNPUUsage(inference_context_t *pContext);

NNAPP_ERRORTYPE createNpuResourcesMonitorThread(inference_context_t *pContext);
NNAPP_ERRORTYPE destroyNpuResourcesMonitorThread();
void *runNpuResourcesMonitorThread(void *pData);

NNAPP_ERRORTYPE NnStartInferenceAsync(inference_context_t *pContext, int netIdx, pthread_t *pThread);
NNAPP_ERRORTYPE NnWaitInference(pthread_t thread);

#endif //__NNNEURALNETWORK_H__
