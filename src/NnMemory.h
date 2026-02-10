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

#ifndef __NNRESERVEDMEMORY_H__
#define __NNRESERVEDMEMORY_H__
/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "NnDebug.h"
#include "NnType.h"
#include "NnError.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
#define PMAP_DEVICE_PATH "/proc/reserved_mem"

#define PMAP_NAME_0 "pmap_visionprotocol"
#define PMAP_NAME_1 "pmap_visionprotocol1"
#define PMAP_NAME_2 "pmap_visionprotocol2"
#define PMAP_NAME_3 "pmap_visionprotocol3"

#define PMAP_MAX_NUMBER (4)
#define PMAP_SPLIT_NUMBER (4)
#define PMAP_SIZE (1920*1080*4)

#define ALIGN_BUFFER_SIZE(outputWidth, outputHeight, alignParam)	do{		\
	uint64_t quotient = (outputWidth * outputHeight * 3) / 4096;			\
	if((outputWidth * outputHeight * 3) % 4096 == 0) {						\
		alignParam = quotient * 4096;										\
	} 																		\
	else{																	\
		alignParam = (quotient + 1) * 4096;									\
	}																		\
}	while(0)

typedef struct _memory_context
{
	int32_t displayMemoryFd;
	int32_t fileMemoryFd;
	uint64_t phy_base_output[2];
	uint8_t *map_base_output[2];
	/* # reservedMemory: Memory reserved for specific scenarios in tcnnapp.
	*   recvPhyAddrAry   |   recvPhyAddrAry   |   recvPhyAddrAry   |   recvPhyAddrAry   |
	*   sendPhyAddrAry   |   sendPhyAddrAry   |   sendPhyAddrAry   |   sendPhyAddrAry   |
	*                    |                    |                    |                    |
	*   phy_base_output  |   phy_base_output  |   phy_base_input   |                    |
	*/
	uint64_t reservedMemory[4][4];
} memory_context_t;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
NNAPP_ERRORTYPE ReservedMemoryInit(memory_context_t *pContext);
NNAPP_ERRORTYPE ApplicationMemoryInit(memory_context_t *pContext, char* inputCameraPath, uint32_t outputWidth, uint32_t outputHeight);
uint64_t ReservedMemoryGetFilePhyBase(memory_context_t *pContext);
NNAPP_ERRORTYPE NnMemoryInit(memory_context_t *pContext, char* inputPath, uint32_t outputWidth, uint32_t outputHeight);
NNAPP_ERRORTYPE NnMemoryDeinit(memory_context_t *pContext, uint32_t outputWidth, uint32_t outputHeight);

#endif // __NNRESERVEDMEMORY_H__
