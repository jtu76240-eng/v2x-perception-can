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
#include <stdbool.h>

#include "NnNeuralNetwork.h"
#include "NnDebug.h"
#include "NnSignalHandler.h"
#include "time_api.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define SO_BUFFER_SIZE 256
#define CMD_BUFFER_SIZE 256
#define PARAM_BUFFER_SIZE 256
#define TIMEOUT_IN_MS 1000
#define NPU_ALPHA (32 * 32)
#define MILLISECOND_CONVERSION_FACTOR 1000.0
#define MAX_MODELS 10

/* ========================================================================== */
/*                       Internal Function Declarations                       */
/* ========================================================================== */
static void *NnRunInferenceFunc1(void *args);
static void *NnRunInferenceFunc2(void *args);
static NNAPP_ERRORTYPE calcNpuUsagePercent(npu_usage_info_t *pPrev, npu_usage_info_t *pCurr);

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
pthread_t g_npuMonitorThread = (pthread_t)NULL;
npu_usage_info_t g_npuCurrUsageInfo[NPU_CLUSTER_INDEX_MAX];

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */
static void *NnRunInferenceFunc1(void *args)
{
	inference_context_t *pContext = (inference_context_t *)args;
    NnRunInference(&(pContext->neuralNetwork[NETWORK_INDEX_0]), pContext->npuRunMode);
	return NULL;
}

static void *NnRunInferenceFunc2(void *args)
{
	inference_context_t *pContext = (inference_context_t *)args;
    NnRunInference(&(pContext->neuralNetwork[NETWORK_INDEX_1]), pContext->npuRunMode);
	return NULL;
}

NNAPP_ERRORTYPE NnNpuInit(inference_context_t *pContext, int32_t clusterIndex)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;
	int idx;

	printf("The npu cluster[%d] is openning...\n", clusterIndex);

	idx = clusterIndex;
	if (idx < NPU_CLUSTER_INDEX_MAX)
	{
		pContext->npuFd[idx] = npu_open(clusterIndex);
		// pContext->npuClusterIdx++;

		printf("The npu cluster[%d] is open done.\n", clusterIndex);
	}
	else
	{
		printf("The npu cluster can only use 0 and 1. Cluster[%d] cannot be used.\n", clusterIndex);
	}

	return err;
}

NNAPP_ERRORTYPE NnNpuDeinit(inference_context_t *pContext)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	for (int index = 0; index < NPU_CLUSTER_INDEX_MAX; index++)
	{
		if (pContext->npuFd[index] != NULL)
		{
			npu_close(pContext->npuFd[index]);
			printf("The npu cluster[%d] is close done.\n", index);
		}
		else
		{
			printf("The npu cluster[%d] is not open.\n", index);
		}
	}

	return err;
}

uint32_t alignInputDataWidth(uint32_t networkInputWidth, uint32_t multiple)
{
	if (networkInputWidth % multiple == 0) {
        return networkInputWidth;
    }
    uint32_t  quotient = networkInputWidth / multiple;
    uint32_t  nextMultiple = (quotient + 1) * multiple;
    return nextMultiple;
}

NNAPP_ERRORTYPE NnNeuralNetworkInit(inference_context_t *pContext, npu_cluster_index_t npuClusterIndex, network_index_t networkIndex, uint8_t scalerIndex, image_fmt_t fmt) //, npu_cluster_index_t npuIdx, uint8_t *networkPath)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;
	npu_t *npu;
	network_context_t *neuralNetwork;
	char sofile[SO_BUFFER_SIZE];
	char cmdfile[CMD_BUFFER_SIZE];
	char paramfile[PARAM_BUFFER_SIZE];
	// npu_perf_t npu_perf; // for profile mode

	npu = pContext->npuFd[npuClusterIndex];
	neuralNetwork = &pContext->neuralNetwork[networkIndex];

	sprintf(sofile, "%s/%s", neuralNetwork->networkPath, SO_FILE);
	sprintf(cmdfile, "%s/%s", neuralNetwork->networkPath, CMD_FILE);
	sprintf(paramfile, "%s/%s", neuralNetwork->networkPath, PARAM_FILE);

	// load network
	neuralNetwork->networkHandle = network_load_from_file(npu, sofile, cmdfile, paramfile);
	if (!neuralNetwork->networkHandle)
	{
		printf("npu_fd(%d) - network loading failed\n", npu->fd);
	}
	else
	{
		printf("npu_fd(%d) - network loading done\n", npu->fd);
	}

	neuralNetwork->nnWidth = network_get_input_width(neuralNetwork->networkHandle);
	neuralNetwork->nnHeight = network_get_input_height(neuralNetwork->networkHandle);
	neuralNetwork->scalerIdx = scalerIndex;
	neuralNetwork->nnFormat = fmt;

	// Print network info
	printf("npu_fd[%d] - %s loaded[%dx%d]\n", npu->fd, neuralNetwork->networkHandle->methods->get_network_name(), neuralNetwork->nnWidth, neuralNetwork->nnHeight);

	// prepare input buffer
	uint32_t multiple;
	uint32_t channel;
	if (fmt == IMAGE_FMT_RGBA32)
	{
		multiple = 4;
		channel = 4;
	}
	else if (fmt == IMAGE_FMT_RGB24)
	{
		multiple = 16;
		channel = 3;
	}
	else {
		/* unsupported by nn-app */
		printf("invalid image format\n");
		exit(0);
	}

	neuralNetwork->nnWidth = alignInputDataWidth(neuralNetwork->nnWidth, multiple);

	neuralNetwork->inputBufferSize = channel * neuralNetwork->nnWidth * neuralNetwork->nnHeight;
	neuralNetwork->inputBuf = buffer_alloc(npu, neuralNetwork->inputBufferSize);

	// prepare output buffer
	neuralNetwork->outputBufferSize = network_get_output_size(neuralNetwork->networkHandle);
	neuralNetwork->outputBuf = buffer_alloc(npu, neuralNetwork->outputBufferSize);

	// get network type
	neuralNetwork->type = network_get_type(neuralNetwork->networkHandle);

	// get conv mac number
	neuralNetwork->convMacNum = neuralNetwork->networkHandle->methods->conv_mac_num;

	return err;
}

NNAPP_ERRORTYPE NnNeuralNetworkDeinit(inference_context_t *pContext, network_index_t networkIndex)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;
	network_context_t *neuralNetwork;

	neuralNetwork = &pContext->neuralNetwork[networkIndex];

	// Deinitialize Inference
    if (neuralNetwork->networkHandle)
    {
        network_close(neuralNetwork->networkHandle);
    }
    if (neuralNetwork->inputBuf)
    {
        buffer_close(neuralNetwork->inputBuf);
    }
    if (neuralNetwork->outputBuf)
    {
        buffer_close(neuralNetwork->outputBuf);
    }

	return err;
}

void NnRunInference(network_context_t *pNeuralNetwork, npu_run_mode_t npuRunMode)
{
	npu_err_bits_t err_status;

	npu_perf_t npu_perf;
	npu_run_mode_t mode = npuRunMode;

	if (network_run(pNeuralNetwork->networkHandle, pNeuralNetwork->inputBuf, pNeuralNetwork->outputBuf,
					&err_status, mode, &npu_perf, TIMEOUT_IN_MS) < 0)
	{
		printf("Inference failed\n");
	}
	else
	{
		unsigned long long freq;
		float npuUtil;
		pNeuralNetwork->inferenceTime = (float)npu_perf.elapsed_in_us / MILLISECOND_CONVERSION_FACTOR;
		if (pNeuralNetwork->inferenceTime != 0u)
		{
			freq = pNeuralNetwork->inferenceTime * (NPU_CORE_CLOCK / 1000);
			freq = freq * NPU_ALPHA * NPU_CORE_NUM;
			npuUtil = (100.0 * (float)((float)(pNeuralNetwork->convMacNum) / (float)freq));
		}
		else
		{
			npuUtil = 0;
		}
		pNeuralNetwork->npuUtilization = npuUtil;
		// NN_LOG("[Info] [%s] Npu utilization : %d\n", __FUNCTION__, pNeuralNetwork->npuUtilization);
	}

#if ENABLE_DBG_ECC_DETECT
	if (err_status.as_field.ce_sram)
	{
		NN_LOG("[Error] NetType[%d] 0x%08x: MLX SRAM ECC CE error detected\n", pNeuralNetwork->type, err_status.as_word);
	}
	if (err_status.as_field.ue_sram)
	{
		NN_LOG("[Error] NetType[%d] 0x%08x: MLX SRAM ECC UE error detected\n", pNeuralNetwork->type, err_status.as_word);
	}
	if (err_status.as_field.ce_gbuf)
	{
		NN_LOG("[Error] NetType[%d] 0x%08x: GBUF ECC CE error detected\n", pNeuralNetwork->type, err_status.as_word);
	}
	if (err_status.as_field.ue_gbuf)
	{
		NN_LOG("[Error] NetType[%d] 0x%08x: GBUF ECC UE error detected\n", pNeuralNetwork->type, err_status.as_word);
	}
	if (err_status.as_field.ce_cbuf)
	{
		NN_LOG("[Error] NetType[%d] 0x%08x: Cmd BUF ECC CE error detected\n", pNeuralNetwork->type, err_status.as_word);
	}
	if (err_status.as_field.ue_cbuf)
	{
		NN_LOG("[Error] NetType[%d] 0x%08x: Cmd BUF ECC UE error detected\n", pNeuralNetwork->type, err_status.as_word);
	}
	if (err_status.as_field.wdt_to)
	{
		NN_LOG("[Error] NetType[%d] 0x%08x: WDT timeout detected\n", pNeuralNetwork->type, err_status.as_word);
	}
#endif

	// NN_LOG("[Info ]elapsed time : %.2f in ms\n", (float)npu_perf.elapsed_in_us / 1000);
	// NN_LOG("[Info ]dma count    : %d @pclk\n", npu_perf.dma);
	// NN_LOG("[Info ]comp count   : %d @pclk\n", npu_perf.comp);

	pNeuralNetwork->type = network_get_type(pNeuralNetwork->networkHandle);
	// printf("network type[%d]\n", pNeuralNetwork->type);

	if (pNeuralNetwork->type == TELECHIPS_NPU_POST_CLASSIFIER)
	{
		network_run_postprocess(pNeuralNetwork->networkHandle, pNeuralNetwork->outputBuf, &(pNeuralNetwork->resultCls));
		NN_LOG("[Post-Process] class[0] : %d\n", pNeuralNetwork->resultCls.class_ids[0]);
	}
	else if (pNeuralNetwork->type == TELECHIPS_NPU_POST_DETECTOR)
	{
		network_run_postprocess(pNeuralNetwork->networkHandle, pNeuralNetwork->outputBuf, &(pNeuralNetwork->resultObj));
		for (int i = 0; i < pNeuralNetwork->resultObj.cnt; i++)
		{
			NN_LOG("[Post-Process] min_xy(%4d,%4d) max_xy(%4d,%4d) class: %d score: %d\n",
				   (int)(pNeuralNetwork->resultObj.obj[i].x_min + 0.5),
				   (int)(pNeuralNetwork->resultObj.obj[i].y_min + 0.5),
				   (int)(pNeuralNetwork->resultObj.obj[i].x_max + 0.5),
				   (int)(pNeuralNetwork->resultObj.obj[i].y_max + 0.5),
				   pNeuralNetwork->resultObj.obj[i].cls,
				   (int)(pNeuralNetwork->resultObj.obj[i].score * 100. + 0.5));
		}
	}
	else if (pNeuralNetwork->type == TELECHIPS_NPU_POST_CUSTOM)
	{
		network_run_postprocess(pNeuralNetwork->networkHandle, pNeuralNetwork->outputBuf, &(pNeuralNetwork->resultCls));
	}
	else
	{
		// TBD
	}
}

NNAPP_ERRORTYPE NnCreateInferenceThread(inference_context_t *pContext)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	pthread_t infThreads_1;
	// pthread_t infThreads_2;

	pthread_create(&infThreads_1, NULL, NnRunInferenceFunc1, (void *)pContext);
	// pthread_create(&infThreads_2, NULL, NnRunInferenceFunc2, (void *)pContext);

	pthread_join(infThreads_1, NULL);
	// pthread_join(infThreads_2, NULL);

	return err;
}

NNAPP_ERRORTYPE NnDestroyInferenceThread()
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	// TBD

	return err;
}

static NNAPP_ERRORTYPE calcNpuUsagePercent(npu_usage_info_t *pPrev, npu_usage_info_t *pCurr)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	pCurr->reqTime = getCurrentTime();

	double unit_clk = ((pCurr->reqTime - pPrev->reqTime) * 200 * 1000 * 1000);

	pCurr->perfDmaUsage  =  (uint32_t)(((double)(pCurr->perfDma  - pPrev->perfDma)  / unit_clk) * 100);
	pCurr->perfCompUsage =  (uint32_t)(((double)(pCurr->perfComp - pPrev->perfComp) / unit_clk) * 100);

	return err;
}

/* NPU utilization is only available in async mode. */
NNAPP_ERRORTYPE getNPUVariation(npu_t *npuFd, uint8_t npuIndex, npu_usage_info_t *pPrev, npu_usage_info_t *pCurr)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	// npu_usage_info_t prev_npu_usage_data;
	memcpy(pPrev, pCurr, sizeof(npu_usage_info_t));

	npu_read_reg(npuFd, ADDR_NPU_CMD_CNT, &(pCurr->cmdCnt));
	npu_read_reg(npuFd, ADDR_NPU_PERF_DMA, &(pCurr->perfDma));
	npu_read_reg(npuFd, ADDR_NPU_PERF_COMP, &(pCurr->perfComp));
	npu_read_reg(npuFd, ADDR_NPU_PERF_ALL, &(pCurr->perfAll));
	npu_read_reg(npuFd, ADDR_NPU_PERF_AXI_CONF, &(pCurr->perfAxiConf));
	npu_read_reg(npuFd, ADDR_NPU_PERF_CNT_MM0, &(pCurr->perfCntMm0));
	npu_read_reg(npuFd, ADDR_NPU_PERF_CNT_DW0, &(pCurr->perfCntDw0));
	npu_read_reg(npuFd, ADDR_NPU_PERF_CNT_MISC0, &(pCurr->perfCntMisc0));
	npu_read_reg(npuFd, ADDR_NPU_PERF_CNT_MLX0, &(pCurr->perfCntMlx0));
	npu_read_reg(npuFd, ADDR_NPU_PERF_CNT_MM1, &(pCurr->perfCntMm1));
	npu_read_reg(npuFd, ADDR_NPU_PERF_CNT_DW1, &(pCurr->perfCntDw1));
	npu_read_reg(npuFd, ADDR_NPU_PERF_CNT_MISC1, &(pCurr->perfCntMisc1));
	npu_read_reg(npuFd, ADDR_NPU_PERF_CNT_MLX1, &(pCurr->perfCntMlx1));

	calcNpuUsagePercent(pPrev, pCurr);

	// printf("INDEX[%d], perfDmaUsage[%d], perfCompUsage[%d]\n",npuIndex, pCurr->perfDmaUsage, pCurr->perfCompUsage);

	NPU_LOG(
		npuIndex,
		pCurr->perfDma - pPrev->perfDma,
		pCurr->perfComp - pPrev->perfComp,
		pCurr->perfDmaUsage,
		pCurr->perfCompUsage,
		pCurr->cmdCnt - pPrev->cmdCnt,
		pCurr->perfAll - pPrev->perfAll,
		pCurr->perfAxiConf - pPrev->perfAxiConf,
		pCurr->perfCntMm0 - pPrev->perfCntMm0,
		pCurr->perfCntDw0 - pPrev->perfCntDw0,
		pCurr->perfCntMisc0 - pPrev->perfCntMisc0,
		pCurr->perfCntMlx0 - pPrev->perfCntMlx0,
		pCurr->perfCntMm1 - pPrev->perfCntMm1,
		pCurr->perfCntDw1 - pPrev->perfCntDw1,
		pCurr->perfCntMisc1 - pPrev->perfCntMisc1,
		pCurr->perfCntMlx1 - pPrev->perfCntMlx1
	);

	return err;
}

NNAPP_ERRORTYPE NPUUsageInfo(inference_context_t *pInfContext)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	getNPUVariation(pInfContext->npuFd[NPU_CLUSTER_INDEX_0], NPU_CLUSTER_INDEX_0, &(pInfContext->prevNpuUsage[NPU_CLUSTER_INDEX_0]), &(pInfContext->currentNpuUsage[NPU_CLUSTER_INDEX_0]));
	setNPUUsage(NPU_CLUSTER_INDEX_0, pInfContext->currentNpuUsage[NPU_CLUSTER_INDEX_0]);
	getNPUVariation(pInfContext->npuFd[NPU_CLUSTER_INDEX_1], NPU_CLUSTER_INDEX_1, &(pInfContext->prevNpuUsage[NPU_CLUSTER_INDEX_1]), &(pInfContext->currentNpuUsage[NPU_CLUSTER_INDEX_1]));
	setNPUUsage(NPU_CLUSTER_INDEX_1, pInfContext->currentNpuUsage[NPU_CLUSTER_INDEX_1]);

	return err;
}

void *runNpuResourcesMonitorThread(void *pData)
{
	inference_context_t *pContext = (inference_context_t *)pData;

	NN_LOG("[INFO] [%s] start!\n", __FUNCTION__);

	pthread_mutex_init(&pContext->npuUsageLocker, NULL); // TODO: Fix NnPerf.h

	while (NnCheckExitFlag() != true)
	{
		pthread_mutex_lock(&pContext->npuUsageLocker);
		NPUUsageInfo(pContext);
		pthread_mutex_unlock(&pContext->npuUsageLocker);

		usleep(DEFAULT_NPU_MONITOR_PERIOD_US);
	}
	if (NnCheckExitFlag())
	{
		printf("[INFO] [%s] end!\n", __FUNCTION__);
	}
	return NULL;
}

NNAPP_ERRORTYPE createNpuResourcesMonitorThread(inference_context_t *pContext)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	pthread_create(&g_npuMonitorThread, NULL, runNpuResourcesMonitorThread, (void*)pContext);

	NN_LOG("[INFO] [%s] create NpuResourcesMonitorThread Done!\n", __FUNCTION__);

	return err;
}

NNAPP_ERRORTYPE destroyNpuResourcesMonitorThread()
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	pthread_join(g_npuMonitorThread, NULL);

	NN_LOG("[INFO] [%s] destroy NpuResourcesMonitorThread Done!\n", __FUNCTION__);

	return err;
}

void setNPUUsage(uint8_t npuIndex, npu_usage_info_t currUsage)
{
	g_npuCurrUsageInfo[npuIndex].perfDmaUsage  = currUsage.perfDmaUsage;
	g_npuCurrUsageInfo[npuIndex].perfCompUsage = currUsage.perfCompUsage;
	// printf("#1 NPU[%d] perfDmaUsage[%d], perfCompUsage[%d]\n", npuIndex, g_npuCurrUsageInfo[npuIndex].perfDmaUsage, g_npuCurrUsageInfo[npuIndex].perfCompUsage);
}

void getNPUUsage(inference_context_t *pContext)
{
	pContext->currentNpuUsage[NPU_CLUSTER_INDEX_0].perfDmaUsage  = g_npuCurrUsageInfo[NPU_CLUSTER_INDEX_0].perfDmaUsage;
	pContext->currentNpuUsage[NPU_CLUSTER_INDEX_0].perfCompUsage = g_npuCurrUsageInfo[NPU_CLUSTER_INDEX_0].perfCompUsage;
	pContext->currentNpuUsage[NPU_CLUSTER_INDEX_1].perfDmaUsage  = g_npuCurrUsageInfo[NPU_CLUSTER_INDEX_1].perfDmaUsage;
	pContext->currentNpuUsage[NPU_CLUSTER_INDEX_1].perfCompUsage = g_npuCurrUsageInfo[NPU_CLUSTER_INDEX_1].perfCompUsage;
	// printf("NPU[0] perfDmaUsage[%d], perfCompUsage[%d]\n", pContext->currentNpuUsage[NPU_CLUSTER_INDEX_0].perfDmaUsage, pContext->currentNpuUsage[NPU_CLUSTER_INDEX_0].perfCompUsage);
	// printf("NPU[1] perfDmaUsage[%d], perfCompUsage[%d]\n", pContext->currentNpuUsage[NPU_CLUSTER_INDEX_1].perfDmaUsage, pContext->currentNpuUsage[NPU_CLUSTER_INDEX_1].perfCompUsage);
}

NNAPP_ERRORTYPE NnStartInferenceAsync(inference_context_t *pContext, int netIdx, pthread_t *pThread)
{
	void *(*func)(void *) = (netIdx == 0) ? NnRunInferenceFunc1 : NnRunInferenceFunc2;
	pthread_create(pThread, NULL, func, (void *)pContext);
	return NNAPP_NO_ERROR;
}

NNAPP_ERRORTYPE NnWaitInference(pthread_t thread)
{
	pthread_join(thread, NULL);
	return NNAPP_NO_ERROR;
}