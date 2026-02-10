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
#include "NnMemory.h"

/* ========================================================================== */
/*                       Internal Function Declarations                       */
/* ========================================================================== */
static int getReservedMemory(const char *pmapName, unsigned long *base, unsigned long *size);

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

static int getReservedMemory(const char *pmapName, unsigned long *base, unsigned long *size)
{
    FILE *file = fopen(PMAP_DEVICE_PATH, "r");
    char line[100];
    char readname[30];
    long long unsigned int start, end;
    int ret = -1;

    if (file == NULL)
    {
        perror("Failed open memory device");
        return ret;
    }

    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "%llx-%llx %*s %s", &start, &end, readname) == 3)
        {
            if (strcmp(readname, pmapName) == 0)
            {
                *size = end - start + 1;
                *base = start;
                ret = 0;
                break;
            }
        }
    }

    return ret;
}

//Design how to set reserved memory in the app.
//Direct access, as used in ApplicationMemoryInit() and NnOutputModeInit(), is not recommended.
//Design how to get reserved memory in the app.
//refer to wiki - https://wiki.telechips.com/pages/viewpage.action?pageId=521251557
uint64_t ReservedMemoryGetFilePhyBase(memory_context_t *pContext)
{
	uint64_t ret;

	ret = pContext->reservedMemory[3][2];

	return ret;
}


NNAPP_ERRORTYPE ReservedMemoryInit(memory_context_t *pContext)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

    unsigned long base, size;
    int ret;
    char *pmapName[PMAP_MAX_NUMBER] = {PMAP_NAME_0, PMAP_NAME_1, PMAP_NAME_2, PMAP_NAME_3};

    for (int i = 0; i < PMAP_MAX_NUMBER; i++)
    {
        ret = getReservedMemory(pmapName[i], &base, &size);
        if (ret == 0)
        {
            for (int j = 0; j < PMAP_SPLIT_NUMBER; j++)
            {
                pContext->reservedMemory[i][j] = base + (PMAP_SIZE * j);
            }
        }
        else
        {
			err = NNAPP_RESERVED_MEM_ERROR;
            NN_LOG("Fail to find reserved memory %s\n", pmapName[i]);
            break;
        }
    }

    return err;
}

NNAPP_ERRORTYPE ApplicationMemoryInit(memory_context_t *pContext, char* inputPath, uint32_t outputWidth, uint32_t outputHeight) //TODO need to change Param variable to context
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	// Initialize Display Memory Driver
	pContext->displayMemoryFd = open("/dev/mem", O_RDWR | O_NDELAY);
	if ((pContext->fileMemoryFd < 0) || (pContext->displayMemoryFd < 0))
	{
		printf("[ERROR] open() error display: /dev/mem\n");
		exit(1);
	}
	else
	{
		printf("[INFO] open() success display: /dev/mem\n");
	}

	// Initialize Memory
	uint64_t alignParam = 0;
	ALIGN_BUFFER_SIZE(outputWidth, outputHeight, alignParam);
	if(strcmp((char *)inputPath, "/dev/video0") == 0)
	{
		pContext->phy_base_output[0] = pContext->reservedMemory[2][0];
		pContext->phy_base_output[1] = pContext->reservedMemory[2][0] + alignParam;
	}
	else if(strcmp((char *)inputPath, "/dev/video1") == 0)
	{
		pContext->phy_base_output[0] = pContext->reservedMemory[2][1];
		pContext->phy_base_output[1] = pContext->reservedMemory[2][1]  + alignParam;
	}
	else if(strcmp((char *)inputPath, "/dev/video2") == 0)
	{
		pContext->phy_base_output[0] = pContext->reservedMemory[2][2];
		pContext->phy_base_output[1] = pContext->reservedMemory[2][2] + alignParam;
	}
	else if(strcmp((char *)inputPath, "/dev/video3") == 0)
	{
		pContext->phy_base_output[0] = pContext->reservedMemory[2][3];
		pContext->phy_base_output[1] = pContext->reservedMemory[2][3] + alignParam;
	}
	else
	{
		pContext->phy_base_output[0] = pContext->reservedMemory[2][0];
		pContext->phy_base_output[1] = pContext->reservedMemory[2][1] + alignParam;
	}

	pContext->map_base_output[0] = (uint8_t *)mmap(NULL, outputWidth * outputHeight * 3, PROT_READ | PROT_WRITE, MAP_SHARED, pContext->displayMemoryFd, pContext->phy_base_output[0]);
	if(pContext->map_base_output[0] == MAP_FAILED)
	{
		printf("[ERROR] [%s] mmap fail map_base_output[0]\n", __FUNCTION__);
		exit(1);
	}
	else
	{
		printf("[INFO] [%s] mmap success map_base_output[0]\n", __FUNCTION__);
	}

	pContext->map_base_output[1] = (uint8_t *)mmap(NULL, outputWidth * outputHeight * 3, PROT_READ | PROT_WRITE, MAP_SHARED, pContext->displayMemoryFd, pContext->phy_base_output[1]);
	if(pContext->map_base_output[1] == MAP_FAILED)
	{
		printf("[ERROR] [%s] mmap fail map_base_output[1]\n", __FUNCTION__);
		exit(1);
	}
	else
	{
		printf("[INFO] [%s] mmap success map_base_output[1]\n", __FUNCTION__);
	}

	return err;
}

NNAPP_ERRORTYPE NnMemoryInit(memory_context_t *pContext, char* inputPath, uint32_t outputWidth, uint32_t outputHeight)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	ReservedMemoryInit(pContext);
	printf("ReservedMemoryInit Done\n");
	ApplicationMemoryInit(pContext, inputPath, outputWidth, outputHeight);
	printf("ApplicationMemoryInit Done\n");

	return err;
}

NNAPP_ERRORTYPE NnMemoryDeinit(memory_context_t *pContext, uint32_t outputWidth, uint32_t outputHeight)
{
	NNAPP_ERRORTYPE err = NNAPP_NO_ERROR;

	if (munmap(pContext->map_base_output[0], outputWidth * outputHeight * 3) == -1)
	{
		perror("Error unmapping map_base_output[0]");
		err = -1; // NNAPP_MEMORY_DEINIT_ERROR
	}

	if (munmap(pContext->map_base_output[1], outputWidth * outputHeight * 3) == -1)
	{
		perror("Error unmapping map_base_output[1]");
		err = -1; // NNAPP_MEMORY_DEINIT_ERROR
	}

	return err;
}
