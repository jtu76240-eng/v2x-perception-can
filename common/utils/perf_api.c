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
#include <stdio.h>

#include "NnType.h"
#include "NnError.h"
#include "NnDebug.h"
#include "perf_api.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define LINE_SIZE 256
#define BUFFER_SIZE 32
#define PERCENTAGE_MULTIPLIER 100.0
#define CPU_UTILIZATION_FACTOR 1.0
#define MEMORY_UTILIZATION_FACTOR 1.0

typedef enum
{
	CPU_STAT_USER = 0,
	CPU_STAT_USER_NICE,
	CPU_STAT_SYSTEM,
	CPU_STAT_IDLE,
	CPU_STAT_MAX
} cpu_stat_index_t;

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */
void getCpuUsage(perf_api_t *pPerfInfo)
{
	int32_t ret = 0;

	static int32_t prevJiffies[CPU_CORE_NUM + 1][CPU_STAT_MAX] = {0, };
	int32_t curJiffies[CPU_CORE_NUM + 1][CPU_STAT_MAX] = {0, };
	int8_t cpuId[CPU_CORE_NUM_MAX];
	int32_t diffJiffies[CPU_CORE_NUM + 1][CPU_STAT_MAX];
	int32_t statIdx, coreIdx, tmp;
	int32_t totalJiffies[CPU_CORE_NUM + 1] = {0, };

	FILE* fp;

	fp = fopen("/proc/stat", "r");
	if (fp != NULL)
	{
		for (coreIdx = 0; coreIdx < CPU_CORE_NUM + 1; coreIdx++)
		{
			totalJiffies[coreIdx] = 0;
			ret = fscanf(fp, "%s %d %d %d %d %d %d %d %d %d %d",
						 cpuId, &curJiffies[coreIdx][CPU_STAT_USER], &curJiffies[coreIdx][CPU_STAT_USER_NICE], &curJiffies[coreIdx][CPU_STAT_SYSTEM], &curJiffies[coreIdx][CPU_STAT_IDLE],
						 &tmp, &tmp, &tmp, &tmp, &tmp, &tmp);
			if(ret != 11)
			{
				fprintf(stderr, "Error reading from /proc/stat\n");
			}
			else
			{
				// printf("Successfully read CPU stats for core %d\n", coreIdx);
			}

			for (statIdx = 0; statIdx < CPU_STAT_MAX; statIdx++)
			{
				diffJiffies[coreIdx][statIdx] = curJiffies[coreIdx][statIdx] - prevJiffies[coreIdx][statIdx];
				totalJiffies[coreIdx] = totalJiffies[coreIdx] + diffJiffies[coreIdx][statIdx];
			}
			if (coreIdx != 0)
			{
				NN_LOG("[INFO] [%s] Cpu usage %d : %f%%\n", __FUNCTION__, coreIdx, PERCENTAGE_MULTIPLIER * (CPU_UTILIZATION_FACTOR - (diffJiffies[coreIdx][CPU_STAT_IDLE] / (double)totalJiffies[coreIdx])));
			}
			pPerfInfo->cpuUtil[coreIdx] = (uint32_t)round(PERCENTAGE_MULTIPLIER * (CPU_UTILIZATION_FACTOR - (diffJiffies[coreIdx][CPU_STAT_IDLE] / (double)totalJiffies[coreIdx])));
			memcpy(prevJiffies[coreIdx], curJiffies[coreIdx], sizeof(int32_t) * CPU_STAT_MAX);
		}
		fclose(fp);
		NN_LOG("[INFO] [%s] cpu usage : %d%% / size : %d\n", __FUNCTION__, pPerfInfo->cpuUtil[0], ret);
	}
}

void getMemoryUsage(perf_api_t *pPerfInfo)
{
	int32_t memTotal = 0;
	int32_t memFree = 0;
	char line[LINE_SIZE];

	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (fp != NULL)
	{
		while (fgets(line, LINE_SIZE, fp) != NULL)
		{
			if (strstr(line, "MemTotal"))
			{
				char tmp[BUFFER_SIZE];
				char size[BUFFER_SIZE];
				sscanf(line, "%s%s", tmp, size);
				memTotal = atoi(size);
				NN_LOG("[INFO] [%s] MemTotal : %d\n", __FUNCTION__, memTotal);
			}
			else if (strstr(line, "MemFree"))
			{
				char tmp[BUFFER_SIZE];
				char size[BUFFER_SIZE];
				sscanf(line, "%s%s", tmp, size);
				memFree = atoi(size);
				NN_LOG("[INFO] [%s] MemFree : %d\n", __FUNCTION__, memFree);
				break;
			}
			else
			{
				// TBD
			}
		}
		fclose(fp);
		pPerfInfo->memUsage = (uint32_t)round(PERCENTAGE_MULTIPLIER * (MEMORY_UTILIZATION_FACTOR - (memFree / (double)memTotal)));
		NN_LOG("[INFO] [%s] mem usage : %d%%\n", __FUNCTION__, pPerfInfo->memUsage);
	}
}