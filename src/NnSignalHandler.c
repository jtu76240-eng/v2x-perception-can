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
#include <stdio.h>
#include <execinfo.h>
#include <stdlib.h>
#include <stdbool.h>

#include "NnSignalHandler.h"

volatile sig_atomic_t g_isSigintFlag;

void NnCrashHandler(int32_t signal)
{
	int32_t j;
	int32_t n;
	void *buffer[256];
	char **strings;

	n = backtrace(buffer, 256);
	printf("[ERROR] [%s]======== [signal : %d] ========\n", __FUNCTION__, signal);

	strings = backtrace_symbols(buffer, n);

	if(strings != NULL)
	{
		for(j = 0; j < n; j++)
		{
			printf("[ERROR] [%s]\r\n", strings[j]);
		}
		free(strings);
	}

	printf("[ERROR] [%s]======= End backtrace()=======\r\n", __FUNCTION__);

	exit(EXIT_FAILURE);
}

void NnSigintChecker(int32_t sig)
{
	printf("[ERROR] [%s] SIGINT occurred : %d\n", __FUNCTION__, sig);

	g_isSigintFlag = true;

	usleep(2 * 1000 * 1000);

	exit(EXIT_FAILURE);
}

void NnSignalChecker(void)
{
	struct sigaction crash;

	crash.sa_handler = NnCrashHandler;
	crash.sa_flags = 0;
	sigemptyset(&crash.sa_mask);

	sigaction(SIGBUS, &crash, NULL);
	sigaction(SIGABRT, &crash, NULL);
	sigaction(SIGSEGV, &crash, NULL);
	sigaction(SIGILL, &crash, NULL);

	signal(SIGINT, NnSigintChecker);
}

void NnSetSignalFlag(bool flag)
{
	g_isSigintFlag = flag;
}


int32_t NnCheckExitFlag()
{
	return g_isSigintFlag;
}