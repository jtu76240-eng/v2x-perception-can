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
#include <stdint.h>
#include <stdarg.h>
#include <time.h>

#include "NnDebug.h"
#include "time_api.h"

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
static char debugLogfile[50];
static char npuDebugLogfile[50];

static int g_NnDebugLevel;
static int g_NpuDebugLevel;

static const char *npuInfoList[] = {
	"TIME", "NPU_NUM", "PERF_DMA", "PERF_COMP","PERF_DMA_PERCENT", "PERF_COMP_PERCENT",
	"CMD_CNT", "PERF_ALL", "PERF_AXI_COMP",
	"PERF_CNT_MM0", "PERF_CNT_DW0", "PERF_CNT_MLX0", "PERF_CNT_MISC0",
	"PERF_CNT_MM1", "PERF_CNT_DW1", "PERF_CNT_MLX1", "PERF_CNT_MISC1",
};
/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
void MakeNPUFile();
void MakeFile();

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */
void NnSetDebugMode(debug_mode_t debugMode, npu_debug_mode_t npuDebugMode)
{
	switch (debugMode)
	{
		case DEBUG_MODE_LOG:
			g_NnDebugLevel = 1;
			break;

		case DEBUG_MODE_FILE:
			g_NnDebugLevel = 2;
			MakeFile();
			break;

		default:
			g_NnDebugLevel = 0;
			break;
	}

	switch (npuDebugMode)
	{
		case NPU_DEBUG_MODE_LOG:
			g_NpuDebugLevel = 1;
			break;

		case NPU_DEBUG_MODE_FILE:
			g_NpuDebugLevel = 2;
			MakeNPUFile();
			break;

		default:
			g_NpuDebugLevel = 0;
			break;
	}
}

void MakeNPUFile()
{
	time_t rawtime;
	struct tm *timeinfo;
	char timestamp[20];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(timestamp, 20, "%Y%m%d%H%M%S", timeinfo);

	FILE *file;
	sprintf(npuDebugLogfile, "tcnnapp_npu_debug_log_%s.csv", timestamp);
	file = fopen(npuDebugLogfile, "a");
	for (size_t i = 0; i < sizeof(npuInfoList) / sizeof(npuInfoList[0]); i++)
	{
		fprintf(file, "%s,", npuInfoList[i]);
	}
	fprintf(file, "\n");
	fclose(file);
}

void MakeFile()
{
	time_t rawtime;
	struct tm *timeinfo;
	char timestamp[20];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(timestamp, 20, "%Y%m%d%H%M%S", timeinfo);

	FILE *file;
	sprintf(debugLogfile, "tcnnapp_debug_log_%s.txt", timestamp);
	file = fopen(debugLogfile, "a");
	fclose(file);
}

void NN_LOG(const char *format,...)
{
	static char LogBuf[512];
	va_list args;
	int32_t length= 0;
#ifdef NN_LOG_TIMESTAMP
	double currentTime;
	currentTime = getCurrentTime();
#endif

	if(g_NnDebugLevel == 1)
	{
		va_start(args, format);
		length = (int32_t)vsnprintf(LogBuf, 512 - 3, format, args);
		va_end (args);
		if(0 < length)
		{
#ifdef NN_LOG_TIMESTAMP
			fprintf(stderr, "[%.3lf] %s", currentTime, &LogBuf);
#else
			fprintf(stderr, "%s", (char*)LogBuf);
#endif
			fflush(stderr);
		}
	}
	else if(g_NnDebugLevel == 2)
	{
		FILE *file;
		file = fopen(debugLogfile, "a");
		va_start(args, format);
		length = (int32_t)vsnprintf(LogBuf, 512 - 3, format, args);
		va_end (args);
		if(0 < length)
		{
#ifdef NN_LOG_TIMESTAMP
			fprintf(file, "[%.3lf] %s", currentTime, &LogBuf);
#else
			fprintf(file, "%s", (char*)LogBuf);
#endif
		}
		fclose(file);
	}
	else
	{
		;
	}
}

void NPU_LOG(uint32_t npu_index, ...)
{
	static char LogBuf_npu[512];
	va_list args;
	int32_t length= 0;

	if(g_NpuDebugLevel == 1)
	{
		fprintf(stderr, "NPU %d Usage Variation\n", npu_index);
		va_start(args, npu_index);
		for(size_t i = 2; i < sizeof(npuInfoList) / sizeof(npuInfoList[0]); i++)
		{
			uint32_t val = va_arg(args, uint32_t);
			if(i ==4 || i == 5)
			{
				fprintf(stderr, "%s \t: %u%%\n", npuInfoList[i], val);
			}
			else
			{
				fprintf(stderr, "%s \t\t: %u\n", npuInfoList[i], val);
			}
		}
		fprintf(stderr, "\n");
		va_end (args);
	}
	else if(g_NpuDebugLevel == 2)
	{
		FILE *file;
		file = fopen(npuDebugLogfile, "a");
		double currentTime = getCurrentTime();

		va_start(args, npu_index);
		uint32_t perf_dma = va_arg(args, uint32_t);
		uint32_t perf_comf = va_arg(args, uint32_t);
		uint32_t perf_dma_percent = va_arg(args, uint32_t);
		uint32_t perf_comf_percent = va_arg(args, uint32_t);

		length = (int32_t)vsnprintf(LogBuf_npu, 512 - 3, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u", args);
		va_end (args);
		if(0 < length)
		{
			fprintf(file, "%.4lf,%d,%u,%u,%u%%,%u%%,%s\n", currentTime, npu_index, perf_dma, perf_comf,
													perf_dma_percent, perf_comf_percent, (char*)LogBuf_npu);
		}
		fclose(file);
	}
	else
	{
		;
	}
}