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

#ifndef __NNDATATYPE_H__
#define __NNDATATYPE_H__

#include <npu_api.h>
/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define NPU_CORE_CLOCK	1000000000
#define NPU_CORE_NUM    2
#define CPU_CORE_NUM	4

#define OUTPUT_BUFFER_IDX_IDLE 1000
#define MILLISECONDS_PER_SECOND 1000.0
#define PERCENTAGE_CONVERSION_FACTOR 100

#define ADDR_NPU_CMD_CNT            		(0x038U)

#define ADDR_NPU_PERF_DMA           		(0x060U)
#define ADDR_NPU_PERF_COMP          		(0x064U)
#define ADDR_NPU_PERF_ALL           		(0x068U)
#define ADDR_NPU_PERF_AXI_CONF      		(0x070U)

#define ADDR_NPU_PERF_CNT_MM0        		(0x080U)
#define ADDR_NPU_PERF_CNT_DW0        		(0x084U)
#define ADDR_NPU_PERF_CNT_MISC0      		(0x088U)
#define ADDR_NPU_PERF_CNT_MLX0       		(0x08CU)

#define ADDR_NPU_PERF_CNT_MM1        		(0x090U)
#define ADDR_NPU_PERF_CNT_DW1        		(0x094U)
#define ADDR_NPU_PERF_CNT_MISC1      		(0x098U)
#define ADDR_NPU_PERF_CNT_MLX1       		(0x09CU)

#define CPU_CORE_NUM_MAX 5

typedef enum
{
	INPUT_MODE_CAMERA = 0,
	INPUT_MODE_FILE,
	INPUT_MODE_RTPM,
	INPUT_MODE_MAX
}input_data_type_t;

typedef enum
{
	OUTPUT_MODE_LCD = 0,
	OUTPUT_MODE_FILE,
	OUTPUT_MODE_RTPM,
	OUTPUT_MODE_NONE,
	OUTPUT_MODE_MAX
}output_data_type_t;

typedef enum
{
	ENCODING_MODE_DISABLE = 0,
	ENCODING_MODE_ENABLE
}encoding_mode_t;

typedef enum
{
	NETWORK_INDEX_0,
	NETWORK_INDEX_1,
	// NETWORK_INDEX_2,
	// NETWORK_INDEX_3,
	// NETWORK_INDEX_4,
	// ...............
	NETWORK_INDEX_MAX
}network_index_t;

typedef enum
{
	IMAGE_FMT_RGBA32 = 0, 	//RGBA
	IMAGE_FMT_RGB24,		//RGB

	IMAGE_FMT_MAX
}image_fmt_t;

typedef enum
{
	NPU_CLUSTER_INDEX_0 = 0,
	NPU_CLUSTER_INDEX_1,
	NPU_CLUSTER_INDEX_MAX
}npu_cluster_index_t;


typedef enum
{
	NPU_DEBUG_MODE_DISABLE = 0,
	NPU_DEBUG_MODE_LOG,
	NPU_DEBUG_MODE_FILE,
}npu_debug_mode_t;

typedef enum
{
	DEBUG_MODE_DISABLE = 0,
	DEBUG_MODE_LOG,
	DEBUG_MODE_FILE,
	DEBUG_MODE_MAX
}debug_mode_t;

typedef struct _perf_api
{
	uint32_t cpuUtil[CPU_CORE_NUM_MAX];
	uint32_t memUsage;
} perf_api_t;

typedef struct _perf_context
{
	perf_api_t pPerfInfo; // TODO:
	double fps;
} perf_context_t;

typedef struct npu_usage_info
{
	uint32_t cmdCnt;
	uint32_t perfDma;
	uint32_t perfComp;
	uint32_t perfDmaUsage; //precent of perfDma
	uint32_t perfCompUsage;//precent of perfComp
	uint32_t perfAll;
	uint32_t perfAxiConf;
	uint32_t perfCntMm0;
	uint32_t perfCntDw0;
	uint32_t perfCntMisc0;
	uint32_t perfCntMlx0;
	uint32_t perfCntMm1;
	uint32_t perfCntDw1;
	uint32_t perfCntMisc1;
	uint32_t perfCntMlx1;
	double   reqTime;
}npu_usage_info_t;

typedef struct _network_context
{
	//network
	char *networkPath;
	npu_net_t *networkHandle;
	long long convMacNum;
	//input tensor
	npu_buf_t *inputBuf;;
	int32_t inputBufferSize;
	uint32_t nnWidth;
	uint32_t nnHeight;
	image_fmt_t nnFormat;
	// scaler_index_t scalerIdx;
	uint8_t scalerIdx;
	//output tensor
	npu_buf_t *outputBuf;
	int32_t outputBufferSize;
	//inferenceResult
	float inferenceTime;
	float npuUtilization;
	//post-porcess
	enlight_postproc_t type;
	enlight_batch_cls_t resultCls;
	enlight_objs_t resultObj;
} network_context_t;

typedef struct _inference_context
{
	npu_t *npuFd[NPU_CLUSTER_INDEX_MAX];
	npu_cluster_index_t npuClusterIdx; //>??
	npu_run_mode_t npuRunMode;
	network_context_t neuralNetwork[NETWORK_INDEX_MAX];
	uint8_t neuralNetworkCnt;
	uint8_t neuralNetworkIndex;
	npu_usage_info_t prevNpuUsage[NPU_CLUSTER_INDEX_MAX];
	npu_usage_info_t currentNpuUsage[NPU_CLUSTER_INDEX_MAX];
	pthread_mutex_t inferenceMutex;
	pthread_mutex_t npuUsageLocker;
	npu_debug_mode_t npuDebugMode;
} inference_context_t;

#endif
