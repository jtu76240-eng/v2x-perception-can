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
#include "NnPerf.h"
#include "NnDebug.h"
#include "NnSignalHandler.h"

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
pthread_t g_systemMonitorThread = (pthread_t)NULL;

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */
NNAPP_ERRORTYPE createSystemMonitorThread(perf_context_t *pContext)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	err = pthread_create(&g_systemMonitorThread, NULL, runSystemResourcesMonitorThread, (void*)pContext);

	printf("[INFO] [%s] create SystemMonitorThread Done!\n", __FUNCTION__);

	return err;
}

NNAPP_ERRORTYPE destroySystemMonitorThread()
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	err = pthread_join(g_systemMonitorThread, NULL);

	printf("[INFO] [%s] destroy SystemMonitorThread Done!\n", __FUNCTION__);

	return err;
}

void *runSystemResourcesMonitorThread(void *pData)
{
	perf_context_t *pContext = (perf_context_t *)pData;

	NN_LOG("[INFO] [%s] start!\n", __FUNCTION__);

	while (NnCheckExitFlag() != true)
	{
		getCpuUsage(&pContext->pPerfInfo);
		getMemoryUsage(&pContext->pPerfInfo);

		usleep(500 * 1000);
	}
	if (NnCheckExitFlag())
	{
		printf("[INFO] [%s] end!\n", __FUNCTION__);
	}
	return NULL;
}
