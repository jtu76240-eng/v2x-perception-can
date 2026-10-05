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
#include "NnAppMain.h"
#include "time_api.h"
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "NnCan.h"
/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
/* 공유 메모리로 raw 프레임 전달 (OpenCV 프로세스용). 헤더 + 더블버퍼. */
#define NN_EXPORT_SHM_NAME "/nn_frame_export"
#define NN_EXPORT_SHM_MAGIC 0x4E4E4652U  /* "NNFR" */
#define NN_EXPORT_SHM_VERSION 1
typedef struct __attribute__((packed)) _nn_export_shm_header {
	uint32_t magic;
	uint32_t version;
	uint32_t width;
	uint32_t height;
	uint32_t stride;      /* bytes per line, width*3 */
	uint32_t write_slot;  /* 0 or 1, which buffer NN last wrote */
	uint64_t frame_index;
} nn_export_shm_header_t;
#define NN_EXPORT_SHM_HEADER_SIZE 32
#define DEFAULT_NETWORK_PATH_1 "/usr/share/yolov5s_quantized/"
#define DEFAULT_NETWORK_PATH_2 "/usr/share/mobilenetv2_10_quantized/"
#define DEFAULT_INPUT_PATH "/dev/video2"
#define DEFAULT_OUTPUT_PATH "/dev/overlay"
#define DEFAULT_INPUT_WIDTH 1280
#define DEFAULT_INPUT_HEIGHT 720
#define DEFAULT_OUTPUT_WIDTH 800
#define DEFAULT_OUTPUT_HEIGHT 480
#define DEFAULT_INPUT_MODE INPUT_MODE_CAMERA
#define DEFAULT_OUTPUT_MODE OUTPUT_MODE_LCD
#ifndef DEFAULT_DEBUG_MODE
#define DEFAULT_DEBUG_MODE DEBUG_MODE_LOG
#endif
#define DEFAULT_NPU_DEBUG_MODE NPU_DEBUG_MODE_DISABLE
#define DEFAULT_NPU_RUN_MODE NPU_RUN_ASYNC
#define DEFAULT_NETWORK_INDEX 2
#define DEFAULT_OUTPUT_SX 0
#define DEFAULT_OUTPUT_SY 0
#define INPUT_RTPM_WIDTH 1280
#define INPUT_RTPM_HEIGHT 720
#define OUTPUT_RTPM_WIDTH 1280
#define OUTPUT_RTPM_HEIGHT 720
#define DEFAULT_TARGET_IP_ADDRESS "192.168.0.8"

typedef struct
{
	CameraHandle cam_handle;
	ScalerHandle scaler_handle;
	DisplayHandle display_handle;
	MessageHandle msg_handle;
} app_obj_t;

/* ========================================================================== */
/*                       Internal Function Declarations                       */
/* ========================================================================== */
static void NnparseArgs(param_info_t *param, int argc, char** argv);
static void NnprintUsage(void);
static void NnModeChecker(param_info_t *param);
static void NnInitAppContext(app_context_t *pContext, param_info_t *pParam);

static float NnGetFPS();
static int32_t NnCreateAPI(app_context_t *pContext, app_obj_t *pObj);
static int32_t NnDestroyAPI(app_context_t *pContext, app_obj_t *pObj);

static void NnInputModeInit(app_context_t *pContext, CameraHandle CameraHandle, MessageHandle msgHandle);
static void NnInputModeDeinit(app_context_t *pContext, CameraHandle CameraHandle, MessageHandle msgHandle);
static void NnGetFrame(app_context_t *pContext, CameraHandle CameraHandle, MessageHandle msgHandle);

static void NnOutputModeInit(app_context_t *pContext, DisplayHandle dispHandle, MessageHandle msgHandle);
static void NnOutputModeDeinit(app_context_t *pContext, DisplayHandle dispHandle, MessageHandle msgHandle);
static int32_t NnOutputResultFrame(app_context_t *pContext, DisplayHandle dispHandle, MessageHandle msgHandle);

static int32_t NnScalerInit(ScalerHandle handle);
static void NnScalerDenit(ScalerHandle handle);
static void NnResizeInputFrame(app_context_t *pContext, ScalerHandle handle, int netIdx);
static void NnResizeOutputFrame(app_context_t *pContext, ScalerHandle scalerHandle, MessageHandle msgHandle);

static int32_t NnOutputResultData(app_context_t *pContext, MessageHandle msgHandle, int netIdx);
static int32_t NnReleaseFrame(app_context_t *pContext, CameraHandle cameraHandle, MessageHandle msgHandle);
static void NnDrawResult(app_context_t *pContext, int netIdx);
static void NnPrintDetectionResults(app_context_t *pContext, int netIdx);

static int NnExportShmInit(app_context_t *pContext);
static void NnExportShmDeinit(void);
static void NnExportFrameToShm(app_context_t *pContext);

static int NnVideoRecorderInit(app_context_t *pContext);
static void NnVideoRecorderDeinit(void);
static void NnVideoRecorderEnqueue(app_context_t *pContext);

static void NnPerfMonitorInit(app_context_t *pContext, MessageHandle msgHandle);
static void NnPerfMonitorDeinit(app_context_t *pContext);

static void NnShowUsage(int32_t argc, char *argv[]);
static int32_t adjustRes(uint16_t *punXres, uint16_t *punYres);

#ifdef INTERACTIVE_MODE
static void *NnInteractive(void *arg);
#endif // _INTERACTIVE_MODE
/* ========================================================================== */
/*                         Structures and Enums                               */
/* ========================================================================== */
static const struct {
	char *inputModeStr[3];
	char *outputModeStr[4];
	char *networkModeStr[2];
	char *debugModeStr[3];
	char *npuDebugModeStr[3];
	char *npuRunModeStr[2];
	char *imgFmtStr[2];
} opt_str = {
	.inputModeStr = {"camera", "file", "rtpm"},
	.outputModeStr = {"display", "file", "rtpm", "none"},
	.networkModeStr = {"on", "off"},
	.debugModeStr = {"off", "log", "file"},
	.npuDebugModeStr = {"off", "log", "file"},
	.npuRunModeStr = {"SyncMode", "AsyncMode"},
	.imgFmtStr = {"ARGB8888", "RGB888"},
};

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
uint64_t syncStamp = 0; //global
app_obj_t g_AppObj;
pthread_t g_InteractiveThread = (pthread_t)NULL;
static volatile uint16_t g_latest_capture_ms_u16[NETWORK_INDEX_MAX] = {0};

#ifdef INTERACTIVE_MODE
static char menu[] = {
	"\n"
	"\n ========================="
	"\n Demo : tc-nn-app Demo"
	"\n ========================="
	"\n"
	"\n c: TBD"
	"\n"
	"\n p: TBD"
	"\n"
	"\n x: Exit"
	"\n"
	"\n Enter Choice: "
};
#endif // _INTERACTIVE_MODE

static char logo[] = {
    "\n"
    " ===================================================================================================\n"
    " ==   _________  _______   ___       _______   ________  ___  ___  ___  ________  ________        ==\n"
    " ==  |\\___   ___\\\\  ___ \\ |\\  \\     |\\  ___ \\ |\\   ____\\|\\  \\|\\  \\|\\  \\|\\   __  \\|\\   ____\\       ==\n"
    " ==  \\|___ \\  \\_\\ \\   __/|\\ \\  \\    \\ \\   __/|\\ \\  \\___|\\ \\  \\\\\\  \\ \\  \\ \\  \\|\\  \\ \\  \\___|_      ==\n"
    " ==       \\ \\  \\ \\ \\  \\_|/_\\ \\  \\    \\ \\  \\_|/_\\ \\  \\    \\ \\   __  \\ \\  \\ \\   ____\\ \\_____  \\     ==\n"
    " ==        \\ \\  \\ \\ \\  \\_|\\ \\ \\  \\____\\ \\  \\_|\\ \\ \\  \\____\\ \\  \\ \\  \\ \\  \\ \\  \\___|\\|____|\\  \\    ==\n"
    " ==         \\ \\__\\ \\ \\_______\\ \\_______\\ \\_______\\ \\_______\\ \\__\\ \\__\\ \\__\\ \\__\\     ____\\_\\  \\   ==\n"
    " ==          \\|__|  \\|_______|\\|_______|\\|_______|\\|_______|\\|__|\\|__|\\|__|\\|__|    |\\_________\\  ==\n"
    " ==                                                                                 \\|_________|  ==\n"
	" ===================================================================================================\n"
	"\n"
};

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */
static void NnShowUsage(int32_t argc, char *argv[])
{
	printf("\n");
	printf(" tc-nn-app Demo - (c) telechips\n");
	printf(" ========================================================\n");
	printf("\n");
	printf(" Usage,\n");
	printf(" ex) [%d]  %s -?\n", argc, argv[0]);
	printf("\n");
}

#ifdef INTERACTIVE_MODE
static void *NnInteractive(void *arg)
{
	char ch;
	(void)arg;

	while (NnCheckExitFlag() != true)
	{
		printf("%s", menu);
		ch = getchar();
		printf("\n");

		switch (ch)
		{
		case 'c':
			// TBD
			break;
		case 'p':
			// TBD
			break;
		case 'e':
			// TBD
			break;
		case 'x':
			NnSetSignalFlag(true);
			break;
		default:
			// printf("Invalid option! Please try again.\n");
			break;
		}
	}
	if (NnCheckExitFlag())
	{
		printf("[INFO] [%s] end!\n", __FUNCTION__);
	}
	return NULL;
}
#endif // _INTERACTIVE_MODE

static void NnparseArgs(param_info_t *param, int argc, char** argv)
{
	int32_t opt;

	// Setting Default Parameters
	param->networkCnt = DEFAULT_NETWORK_INDEX;

	// 두 NPU에서 동일한 모델을 사용하도록 기본 모델 경로 설정
	param->networkPath[NETWORK_INDEX_0] = DEFAULT_NETWORK_PATH_1;
	param->networkPath[NETWORK_INDEX_1] = DEFAULT_NETWORK_PATH_1;;

	param->inputPath = DEFAULT_INPUT_PATH;
	param->outputPath = DEFAULT_OUTPUT_PATH;
	param->inputWidth = DEFAULT_INPUT_WIDTH;
	param->inputHeight = DEFAULT_INPUT_HEIGHT;
	param->inputFormat = IMAGE_FMT_RGBA32; //IMAGE_FMT_RGBA32
	param->outputWidth = DEFAULT_OUTPUT_WIDTH;
	param->outputHeight = DEFAULT_OUTPUT_HEIGHT;
	param->npuDebugMode = NPU_DEBUG_MODE_DISABLE;
	param->outputFormat = IMAGE_FMT_RGB24; //IMAGE_FMT_RGB24
	param->inputMode = DEFAULT_INPUT_MODE;
	param->outputMode = DEFAULT_OUTPUT_MODE;
	param->outputSx = DEFAULT_OUTPUT_SX;
	param->outputSy = DEFAULT_OUTPUT_SY;
	param->recordVideo = 0;
	param->debugMode = DEFAULT_DEBUG_MODE;
	param->npuRunMode = DEFAULT_NPU_RUN_MODE;
	param->pRtpmIpAdress = DEFAULT_TARGET_IP_ADDRESS;

	if (argc == 1)
	{
		printf("%s", logo);
		NnShowUsage(argc, argv);
		// exit(0);
	}
	else
	{
		printf("%s", logo);
	}

	while(-1 != (opt = getopt(argc, argv, "i:o:w:W:h:H:n:N:p:P:X:Y:g:u:a:R?")))
	{
		switch (opt) {
			// Common Parameter
			case 'i':
				if(strcmp(optarg, "camera") == 0)
				{
					param->inputMode = INPUT_MODE_CAMERA;
					param->inputFormat = IMAGE_FMT_RGBA32;
				}
				else if(strcmp(optarg, "rtpm") == 0)
				{
					param->inputMode = INPUT_MODE_RTPM;
					param->inputFormat = IMAGE_FMT_RGB24;
					param->inputWidth = INPUT_RTPM_WIDTH;
					param->inputHeight = INPUT_RTPM_HEIGHT;
				}
				else if(strcmp(optarg, "file") == 0)
				{
					param->inputMode = INPUT_MODE_FILE;
					param->inputFormat = IMAGE_FMT_RGBA32;
				}
				else
				{
					printf("invalid parameter %s", optarg);
					NnprintUsage();
					exit(0);
				}
				break;
			case 'o':
				if(strcmp(optarg, "display") == 0)
				{
					param->outputMode = OUTPUT_MODE_LCD;
					param->outputFormat = IMAGE_FMT_RGB24;
				}
				else if(strcmp(optarg, "rtpm") == 0)
				{
					param->outputMode = OUTPUT_MODE_RTPM;
					param->outputFormat = IMAGE_FMT_RGB24;
					param->outputWidth = OUTPUT_RTPM_WIDTH;
					param->outputHeight = OUTPUT_RTPM_HEIGHT;
				}
				else if(strcmp(optarg, "file") == 0)
				{
					param->outputMode = OUTPUT_MODE_FILE;
					param->outputFormat = IMAGE_FMT_RGB24;
				}
				else if(strcmp(optarg, "n") == 0)
				{
					param->outputMode = OUTPUT_MODE_NONE;
					param->outputFormat = IMAGE_FMT_RGB24;
				}
				else
				{
					printf("invalid parameter %s", optarg);
					NnprintUsage();
					exit(0);
				}
				break;
			case 'w':
				param->inputWidth = atoi(optarg);
				break;
			case 'W':
				param->outputWidth = atoi(optarg);
				break;
			case 'h':
				param->inputHeight = atoi(optarg);
				break;
			case 'H':
				param->outputHeight = atoi(optarg);
				break;
			case 'u':
				if(strcmp(optarg, "off") == 0)
				{
					param->npuDebugMode = NPU_DEBUG_MODE_DISABLE;
				}
				else if(strcmp(optarg, "log") == 0)
				{
					param->npuDebugMode = NPU_DEBUG_MODE_LOG;
				}
				else if(strcmp(optarg, "file") == 0)
				{
					param->npuDebugMode = NPU_DEBUG_MODE_FILE;
				}
				else
				{
					printf("invalid parameter %s", optarg);
					NnprintUsage();
					exit(0);
				}
				break;
			case 'n':
				param->networkPath[0] = optarg;
				break;
			case 'N':
				param->networkPath[1] = optarg;
				break;
			case 'p':
				param->inputPath = optarg;
				break;
			case 'P':
				param->outputPath = optarg;
				break;
			case 'X':
				param->outputSx = atoi(optarg);
				break;
			case 'Y':
				param->outputSy = atoi(optarg);
				break;
			case 'g':
				if(strcmp(optarg, "off") == 0)
				{
					param->debugMode = DEBUG_MODE_DISABLE;
				}
				else if(strcmp(optarg, "log") == 0)
				{
					param->debugMode = DEBUG_MODE_LOG;
				}
				else if(strcmp(optarg, "file") == 0)
				{
					param->debugMode = DEBUG_MODE_FILE;
				}
				else
				{
					printf("invalid parameter %s", optarg);
					NnprintUsage();
					exit(0);
				}
				break;
			case 'a':
				if(strcmp(optarg, "sync") == 0)
				{
					param->npuRunMode = NPU_RUN_SYNC;
				}
				else if(strcmp(optarg, "async") == 0)
				{
					param->npuRunMode = NPU_RUN_ASYNC;
				}
				else
				{
					printf("[WARN] invalid parameter %s, Using a Default MODE [%s]\n",
							optarg , opt_str.npuRunModeStr[DEFAULT_NPU_RUN_MODE]);
				}
				break;
			case 'R':
				param->recordVideo = 1;
				break;
			case 't':
				param->pRtpmIpAdress = optarg;
				break;
			case '?':
				NnprintUsage();
				exit(0);
				break;
			default:
				NnprintUsage();
				exit(0);
				break;
		}
	}

   	if (param->outputMode == OUTPUT_MODE_LCD) {
    	int32_t bAdjust = adjustRes(&param->outputWidth, &param->outputHeight);
    	if (bAdjust == -1) {
        	printf("---!!!! adusting resolution failed\n");
    	}
	}

	printf("----------------Parameter Info----------------\n");
	printf("\n");
	printf("[Network1]Network Path         : %s\n", param->networkPath[0]);
	// printf("[Network2]Network Path         : %s\n", param->networkPath[1]);
	printf("\n");
	printf("[Common]Input Mode             : %s\n", opt_str.inputModeStr[param->inputMode]);
	printf("[Common]Output Mode            : %s\n", opt_str.outputModeStr[param->outputMode]);
	printf("[Common]Input Size             : %d x %d\n", param->inputWidth, param->inputHeight);
	printf("[Common]Output Size            : %d x %d\n", param->outputWidth, param->outputHeight);
	printf("[Common]Input Path             : %s\n", param->inputPath);
	printf("[Common]Output Path            : %s\n", param->outputPath);
	printf("[Common]Output Position X      : %d\n", param->outputSx);
	printf("[Common]Output Position Y      : %d\n", param->outputSy);
	printf("\n");
	printf("[Debug]Debug Mode              : %s\n", opt_str.debugModeStr[param->debugMode]);
	printf("[Debug]NPU Debug Mode          : %s\n", opt_str.debugModeStr[param->npuDebugMode]);
	printf("[Debug]NPU Running Mode        : %s\n", opt_str.npuRunModeStr[param->npuRunMode]);
	printf("----------------Parameter End----------------\n\n");
}

static void NnprintUsage(void)
{
	printf("---------------- Usage ----------------\n");
    printf("[Network1]Network Path         : -n default[%s]\n", DEFAULT_NETWORK_PATH_1);
    printf("[Network2]Network Path         : -N default[%s]\n", DEFAULT_NETWORK_PATH_2);
	printf("\n");
	printf("[Common]Input Mode             : -i default[%s] option: camera, rtpm, file\n", opt_str.inputModeStr[DEFAULT_INPUT_MODE]);
	printf("[Common]Output Mode            : -o default[%s] option: display, rtpm, file, n(none)\n", opt_str.outputModeStr[DEFAULT_OUTPUT_MODE]);
	printf("[Common]Input Width            : -w default[%d] max: 1920 \n", DEFAULT_INPUT_WIDTH);
	printf("[Common]Input Height           : -h default[%d] max 1080\n", DEFAULT_INPUT_HEIGHT);
	printf("[Common]Output Width           : -W default[%d] max: 1920\n", DEFAULT_OUTPUT_WIDTH);
	printf("[Common]Output Height          : -H default[%d] max 720\n", DEFAULT_OUTPUT_HEIGHT);
	printf("[Common]Input Path             : -p default[%s]\n", DEFAULT_INPUT_PATH);
	printf("[Common]Output Path            : -P default[%s]\n", DEFAULT_OUTPUT_PATH);
	printf("[Common]Output Position X      : -X default[%d]\n", DEFAULT_OUTPUT_SX);
	printf("[Common]Output Position Y      : -Y default[%d]\n", DEFAULT_OUTPUT_SY);
	printf("\n");
	printf("[Debug]Debug Mode              : -g default[%s] option: off, log, file\n", opt_str.debugModeStr[DEFAULT_DEBUG_MODE]);
	printf("[Debug]NPU Debug Mode          : -u default[%s] option: off, log, file\n", opt_str.npuDebugModeStr[DEFAULT_NPU_DEBUG_MODE]);
	printf("[Debug]NPU Running Mode        : -a default[%s] option: sync, async\n", opt_str.npuRunModeStr[DEFAULT_NPU_RUN_MODE]);
	printf("---------------- Usage ----------------\n\n");
}


static int32_t adjustRes(uint16_t *punXres, uint16_t *punYres) {
	int resfd = -1;

    printf("[Info] reading framebuffer resolution from fb@0\n");
	resfd = open("/proc/device-tree/fb@0/xres", O_RDONLY);
	if(resfd > 0)
	{
		char buf[4];
		uint32_t xRes = 0;
		int bytes_read = read(resfd, buf, sizeof(buf));
		if(bytes_read > 0)
		{
			memcpy(&xRes, buf, 4);
			xRes = __builtin_bswap32(xRes);
		}

		*punXres = (xRes >= *punXres)? *punXres : xRes;
		close(resfd);
	} else {
		return -1;
	}	

	resfd = open("/proc/device-tree/fb@0/yres", O_RDONLY);
	if(resfd > 0)
	{
		char buf[4];
		uint32_t yRes = 0;
		int bytes_read = read(resfd, buf, sizeof(buf));
		if(bytes_read > 0)
		{
			memcpy(&yRes, buf, 4);
			yRes = __builtin_bswap32(yRes);
		}
		*punYres = (yRes >= *punYres)? *punYres : yRes;
		close(resfd);
	} else {
		return -1;
	}	

	printf("####### finall Witdh = %d and Height=%d\n", *punXres, *punYres);
	return 0;
}

static void NnModeChecker(param_info_t *param)
{

	if(param->inputMode == INPUT_MODE_RTPM && param->outputMode != OUTPUT_MODE_RTPM)
	{
		printf("When the Input Mode is set to RTPM, the Output Mode cannot be set to anything other than RTPM.\n");

		exit(0);
	}
	else if(param->inputMode == INPUT_MODE_FILE && param->outputMode == OUTPUT_MODE_RTPM)
	{
		printf("When the Input Mode is set to File, it is not possible to set the Output Mode to RTPM.\n");

		exit(0);
	}

	if(param->inputMode == INPUT_MODE_FILE)
	{
		const char *suffix = ".png";
		size_t str_len = strlen(param->inputPath);
		size_t suffix_len = strlen(suffix);
		const char *start = param->inputPath + str_len - suffix_len;
		FILE *file = fopen(param->inputPath, "r");

		if(strcmp(start, suffix) != 0)
		{
			printf("The extension of the input file must be png. If you have not set the input path, use the [-p ./filePath.png] option.\n");

			exit(0);
		}
		else if(file == NULL)
		{
			printf("Input file does not exist.\n");

			exit(0);
		}

		fclose(file);
	}

	if(param->outputMode == OUTPUT_MODE_FILE)
	{
		const char *suffix = ".png";
		size_t str_len = strlen(param->outputPath);
		size_t suffix_len = strlen(suffix);
		const char *start = param->outputPath + str_len - suffix_len;

		if(strcmp(start, suffix) != 0)
		{
			printf("The extension of the output file must be png. If you have not set the output path, use the [-P ./filePath.png] option.\n");

			exit(0);
		}
	}
}

/* 실행 파일과 같은 폴더 아래 outputs/ 생성, 이미 있으면 내부 파일만 삭제 */
static void NnEnsureOutputsDirReady(app_context_t *pContext)
{
	char exePath[PATH_MAX];
	char dirBuf[PATH_MAX];
	char outputsDir[PATH_MAX + 16];
	struct dirent *ent;
	DIR *d;
	ssize_t len;

	len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
	if (len <= 0)
		return;
	exePath[len] = '\0';
	strncpy(dirBuf, exePath, sizeof(dirBuf) - 1);
	dirBuf[sizeof(dirBuf) - 1] = '\0';
	dirname(dirBuf);
	snprintf(pContext->outputsBaseDir, sizeof(pContext->outputsBaseDir), "%s", dirBuf);
	snprintf(outputsDir, sizeof(outputsDir), "%s/outputs", dirBuf);

	mkdir(outputsDir, 0755);
	if (errno != 0 && errno != EEXIST)
		return;

	d = opendir(outputsDir);
	if (!d)
		return;
	while ((ent = readdir(d)) != NULL)
	{
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
			continue;
		{
			/* outputsDir + "/" + d_name(최대 255) 수용 */
			char full[PATH_MAX + 16 + 256];
			(void)snprintf(full, sizeof(full), "%s/%s", outputsDir, ent->d_name);
			unlink(full);
		}
	}
	closedir(d);
}

static void NnInitAppContext(app_context_t *pContext, param_info_t *pParam)
{
	pContext->inferenceContext.neuralNetworkCnt = pParam->networkCnt;
	pContext->outputFrameCounter = 0;
	pContext->outputsBaseDir[0] = '\0';
	pContext->recordVideo = pParam->recordVideo;

	memset(&pContext->inferenceContext.neuralNetwork[NETWORK_INDEX_0], 0x0, sizeof(network_context_t));
	memset(&pContext->inferenceContext.neuralNetwork[NETWORK_INDEX_1], 0x0, sizeof(network_context_t));
	pContext->inferenceContext.neuralNetwork[NETWORK_INDEX_0].networkPath = pParam->networkPath[NETWORK_INDEX_0];
	pContext->inferenceContext.neuralNetwork[NETWORK_INDEX_1].networkPath = pParam->networkPath[NETWORK_INDEX_1];

	//TODO: npuFD init
	pContext->debugMode = pParam->debugMode;
	pContext->inferenceContext.npuDebugMode = pParam->npuDebugMode;
	pContext->inferenceContext.npuRunMode = pParam->npuRunMode;

	pContext->memory_context.displayMemoryFd = 0;
	pContext->memory_context.fileMemoryFd = 0;

	pContext->phy_base_input = 0;
	pContext->map_base_input = NULL;
	pContext->memory_context.phy_base_output[0] = 0;
	pContext->memory_context.phy_base_output[1] = 0;
	pContext->memory_context.map_base_output[0] = NULL;
	pContext->memory_context.map_base_output[1] = NULL;

	pContext->phy_base_output_idx = 0;

	//TODO //reserved mem variable init

	pContext->inputDataType = pParam->inputMode;
	pContext->inputImageFormat = pParam->inputFormat;
	pContext->inputWidth = pParam->inputWidth;
	pContext->inputHeight = pParam->inputHeight;
	pContext->inputPath = pParam->inputPath;

	if(pContext->inputDataType == INPUT_MODE_CAMERA)
	{
		pContext->camCaptureRetryCnt = CAM_RETRY_CNT;
	}
	else if(pContext->inputDataType == INPUT_MODE_FILE)
	{
		// pContext->inputPath = pParam->inputPath;

	}
	else if(pContext->inputDataType == INPUT_MODE_RTPM)
	{
		pContext->inputRtpmBufferCount = 4;
	}
	else
	{
		exit(1);
	}

	pContext->outputDataType = pParam->outputMode;
	pContext->outputImageFormat = pParam->outputFormat;
	pContext->outputPath = pParam->outputPath;

	if(pContext->outputDataType == OUTPUT_MODE_LCD)
	{
		pContext->outputLcdSx = pParam->outputSx;
		pContext->outputLcdSy = pParam->outputSy;
	}
	else if(pContext->outputDataType == OUTPUT_MODE_FILE)
	{
		// pContext->outputPath = pParam->outputPath;

	}
	else if(pContext->outputDataType == OUTPUT_MODE_RTPM)
	{
		pContext->outputRtpmBufferCount = 4;
		pContext->pRtpmIpAdress = pParam->pRtpmIpAdress;
		pContext->streamPort = RTPM_STEAM_PORT;
		pContext->messagePort = RTPM_MESSAGE_PORT;
	}
	else if(pContext->outputDataType == OUTPUT_MODE_NONE)
	{
		/* no output device */
	}
	else
	{
		exit(1);
	}

	pContext->outputWidth = pParam->outputWidth;
	pContext->outputHeight = pParam->outputHeight;

#ifdef USE_FISHEYE_UNDISTORT
	{
		int ch = (pContext->inputImageFormat == IMAGE_FMT_RGBA32) ? 4 : 3;
		pContext->fisheyeWorkBufSize = (size_t)pContext->inputWidth * (size_t)pContext->inputHeight * (size_t)ch;
		pContext->fisheyeWorkBuf = (uint8_t *)malloc(pContext->fisheyeWorkBufSize);
		pContext->fisheyeMapHandle = NULL;
		if (pContext->fisheyeWorkBuf != NULL) {
#ifndef FISHEYE_UNDISTORT_STRENGTH
#define FISHEYE_UNDISTORT_STRENGTH 1.3f
#endif
			/* D_base: (k1, k2, p1, p2, k3). k1>0 이면 통통렌즈(어안) 보정. 0이면 strength를 올려도 변화 없음. */
			static const float D_base[5] = { 0.4f, 0.f, 0.f, 0.f, 0.f };
			float K[9] = {
				(float)pContext->inputWidth, 0.f, (float)(pContext->inputWidth / 2),
				0.f, (float)pContext->inputWidth, (float)(pContext->inputHeight / 2),
				0.f, 0.f, 1.f
			};
			float D[5] = {
				D_base[0] * FISHEYE_UNDISTORT_STRENGTH,
				D_base[1] * FISHEYE_UNDISTORT_STRENGTH,
				D_base[2] * FISHEYE_UNDISTORT_STRENGTH,
				D_base[3] * FISHEYE_UNDISTORT_STRENGTH,
				D_base[4] * FISHEYE_UNDISTORT_STRENGTH
			};
			pContext->fisheyeMapHandle = cvFisheyeUndistortMapCreate(K, D,
				(uint32_t)pContext->inputWidth, (uint32_t)pContext->inputHeight);
			if (pContext->fisheyeMapHandle != NULL)
				NN_LOG("[PERF] fisheye map precomputed (remap-only mode)\n");
			else
				NN_LOG("[PERF] fisheye fallback (recompute map every frame)\n");
		} else
			pContext->fisheyeWorkBufSize = 0;
	}
#else
	pContext->fisheyeWorkBuf = NULL;
	pContext->fisheyeWorkBufSize = 0;
	pContext->fisheyeMapHandle = NULL;
#endif
}

static float NnGetFPS()
{
	static struct timeval bgn, end;
	float fps;

	gettimeofday(&end, NULL);
	fps = 1000.0 / (((end.tv_sec - bgn.tv_sec) * 1000.0) + ((end.tv_usec - bgn.tv_usec) / 1000.0));
	gettimeofday(&bgn, NULL);

	return fps;
}

static void NnInputModeInit(app_context_t *pContext, CameraHandle CameraHandle, MessageHandle msgHandle)
{
	int32_t ret;
	input_data_type_t inputMode = pContext->inputDataType;

	// Setting Input Mode
	if(inputMode == INPUT_MODE_CAMERA)
	{
		// Initialize Camera Driver
		ret = CameraOpenDevice(CameraHandle, pContext->inputPath);
		if (ret < 0)
		{
			NN_LOG("[ERROR] [CameraOpenDevice] %s error : %d\n", pContext->inputPath, ret);
			exit(1);
		}
		else
		{
			NN_LOG("[INFO] [CameraOpenDevice] %s successs : %d\n", pContext->inputPath, ret);
		}

		ret = CameraSetConfig(CameraHandle, pContext->inputWidth, pContext->inputHeight);
		if (ret < 0)
		{
			NN_LOG("[ERROR] [CameraSetConfig] error : %d\n", ret);
			exit(1);
		}
		else
		{
			NN_LOG("[INFO] [CameraSetConfig] success : %d\n", ret);
		}
	}
	else if(inputMode == INPUT_MODE_FILE)
	{
		pContext->phy_base_input = pContext->memory_context.reservedMemory[3][2];
		pContext->map_base_input = (uint8_t *)mmap(NULL, PMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pContext->memory_context.displayMemoryFd, pContext->phy_base_input);
		if(pContext->map_base_input == MAP_FAILED)
		{
			NN_LOG("[ERROR] [%s] mmap fail file input buffer\n", __FUNCTION__);
			exit(1);
		}
		else
		{
			NN_LOG("[INFO] [%s] mmap success file input buffer\n", __FUNCTION__);
		}
	}
	else if(inputMode == INPUT_MODE_RTPM)
	{
		uint8_t colorFormatBytes;
		uint32_t rtpmFrameSize;
		uint8_t rtpmBufferCount;
		uint64_t recvPhyAddrAry[4] = {pContext->memory_context.reservedMemory[0][0], pContext->memory_context.reservedMemory[0][1], pContext->memory_context.reservedMemory[0][2], pContext->memory_context.reservedMemory[0][3]};
		uint16_t rtpmTargetStreamPort;
		uint16_t rtpmTargetMessagePort;
		char * pRtpmTargetipAddress;

		if(pContext->inputImageFormat == IMAGE_FMT_RGB24)
		{
			colorFormatBytes = 3;
		}
		else if(pContext->inputImageFormat == IMAGE_FMT_RGBA32)
		{
			colorFormatBytes = 4;
		}
		else
		{
			NN_LOG("[INFO] [%s] invalid color format\n", __FUNCTION__);
			exit(EXIT_FAILURE);
		}

		rtpmFrameSize = pContext->inputWidth * pContext->inputHeight * colorFormatBytes;
		rtpmBufferCount = pContext->inputRtpmBufferCount;
		pRtpmTargetipAddress = pContext->pRtpmIpAdress;
		rtpmTargetStreamPort = pContext->streamPort;
		rtpmTargetMessagePort = pContext->messagePort;

		ret = MessageOpen(msgHandle, MESSAGE_STREAM_RECV, recvPhyAddrAry, rtpmBufferCount, rtpmFrameSize, pRtpmTargetipAddress, rtpmTargetStreamPort, rtpmTargetMessagePort);
		if (ret < 0)
		{
			printf("[Message Open] error : %d\n", ret);
			exit(EXIT_FAILURE);
		}
		else
		{
			NN_LOG("[Message Open] is success\n", __FUNCTION__);
		}
	}
	else
	{
		NN_LOG("[Error] Invaild input mode\n", __FUNCTION__);
	}
	printf("Input mode init Done\n");
}

static void NnInputModeDeinit(app_context_t *pContext, CameraHandle CameraHandle, MessageHandle msgHandle)
{
#ifdef USE_FISHEYE_UNDISTORT
	if (pContext->fisheyeWorkBuf != NULL)
	{
		free(pContext->fisheyeWorkBuf);
		pContext->fisheyeWorkBuf = NULL;
		pContext->fisheyeWorkBufSize = 0;
	}
#endif
	if(pContext->inputDataType == INPUT_MODE_CAMERA)
	{
		CameraCloseDevice(CameraHandle);
	}
	else if(pContext->inputDataType == INPUT_MODE_FILE)
	{
		// TODO:
	}
	else if(pContext->inputDataType == INPUT_MODE_RTPM)
	{
		MessageClose(msgHandle);
	}
}

static void NnOutputModeInit(app_context_t *pContext, DisplayHandle dispHandle, MessageHandle msgHandle)
{
	int32_t ret = -1;
	input_data_type_t input_mode = pContext->inputDataType;
	output_data_type_t output_mode = pContext->outputDataType;

	/* Setup initialize for output mode */
	switch (output_mode)
	{
		case OUTPUT_MODE_NONE:
			break;

		case OUTPUT_MODE_RTPM:
			uint8_t colorFormatBytes;
			uint32_t rtpmFrameSize;
			uint8_t rtpmBufferCount;
			uint64_t sendPhyAddrAry[4] = {pContext->memory_context.reservedMemory[1][0], pContext->memory_context.reservedMemory[1][1], pContext->memory_context.reservedMemory[1][2], pContext->memory_context.reservedMemory[1][3]};
			uint16_t rtpmTargetStreamPort;
			uint16_t rtpmTargetMessagePort;
			char * pRtpmTargetipAddress;

			if(pContext->outputImageFormat == IMAGE_FMT_RGB24)
			{
				colorFormatBytes = 3;
			}
			else if(pContext->outputImageFormat == IMAGE_FMT_RGBA32)
			{
				colorFormatBytes = 4;
			}
			else
			{
				printf("[error] [%s] invalid color format\n", __FUNCTION__);
				exit(EXIT_FAILURE);
			}

			rtpmFrameSize = pContext->outputWidth * pContext->outputHeight * colorFormatBytes;
			rtpmBufferCount = pContext->outputRtpmBufferCount;
			pRtpmTargetipAddress = pContext->pRtpmIpAdress;
			rtpmTargetStreamPort = pContext->streamPort;
			rtpmTargetMessagePort = pContext->messagePort;
			if(input_mode == INPUT_MODE_CAMERA)
			{
				ret = MessageOpen(msgHandle, MESSAGE_STREAM_SEND, sendPhyAddrAry, rtpmBufferCount, rtpmFrameSize, pRtpmTargetipAddress, rtpmTargetStreamPort, rtpmTargetMessagePort);
				if (ret < 0)
				{
					printf("[error] Message Open error : %d\n", ret);
					exit(EXIT_FAILURE);
				}
				else
				{
					printf("[INFO] Message Open succcess : %d\n", ret);
				}
			}
			else
			{
				// printf("Injection Mode\n");
			}
			break;

		case OUTPUT_MODE_LCD:
			// Initialize Display Driver
			ret = DisplayOpenDevice(dispHandle, pContext->outputPath);
			if (ret < 0)
			{
				printf("[ERROR] DisplayOpenDevice() error : %s \n", (char *)pContext->outputPath);
				exit(EXIT_FAILURE);
			}
			else
			{
				NN_LOG("[INFO] DisplayOpenDevice() success : %s \n", (char *)pContext->outputPath);
			}
			break;

		case OUTPUT_MODE_FILE:
			// TBD
			break;

		default:
			// Handle unknown output mode
			printf("[ERROR] Unknown output mode: %d\n", output_mode);
			exit(EXIT_FAILURE);
	}
	printf("Output mode init Done\n");
}


static void NnOutputModeDeinit(app_context_t *pContext, DisplayHandle dispHandle, MessageHandle msgHandle)
{
	// Deinitialize Output
	if(pContext->outputDataType == OUTPUT_MODE_LCD)
	{
		DisplayCloseDevice(dispHandle);
		printf("The Display Device is close done.\n");
	}
	else
	{
		printf("The output is not in LCD mode\n");
	}

	if(pContext->outputDataType == OUTPUT_MODE_RTPM)
	{
		if(pContext->inputDataType != INPUT_MODE_RTPM)
		{
			MessageClose(msgHandle);
		}
		else
		{
			// TBD
		}
	}
	else
	{
		// TBD
	}
}

static int32_t NnScalerInit(ScalerHandle handle)
{
	int32_t ret = -1;

	// Initialize Scaler Driver
	ret = ScalerOpenDevice(handle, SCALER_DEV_NAME_0, SCALER_INDEX_0);
	if (ret < 0)
	{
		NN_LOG("[ERROR] ScalerOpenDevice() error : /dev/scaler1 \n");
		exit(EXIT_FAILURE);
	}
	else
	{
		NN_LOG("[INFO] ScalerOpenDevice() success : /dev/scaler1 \n");
	}

	ret = ScalerOpenDevice(handle, SCALER_DEV_NAME_1, SCALER_INDEX_1);
	if (ret < 0)
	{
		NN_LOG("[ERROR] ScalerOpenDevice() error : /dev/scaler3 \n");
		exit(EXIT_FAILURE);
	}
	else
	{
		NN_LOG("[INFO] ScalerOpenDevice() success : /dev/scaler3 \n");
	}

	return ret;
}

static void NnScalerDenit(ScalerHandle handle)
{
	// Deinitialize Scaler Driver
	ScalerCloseDevice(handle, SCALER_INDEX_0);
	ScalerCloseDevice(handle, SCALER_INDEX_1);
}

static void NnGetFrame(app_context_t *pContext, CameraHandle CameraHandle, MessageHandle msgHandle)
{
	int32_t sizeRet = -1;
	input_data_type_t inputMode = pContext->inputDataType;
	int32_t retryCnt;
	double t0, t1, t2;

	t0 = getCurrentTime() * 1000.0;

	if(inputMode == INPUT_MODE_CAMERA)
	{
		retryCnt = pContext->camCaptureRetryCnt;

		while((sizeRet <= 0) && (retryCnt > 0))
		{
			sizeRet = CameraGetBuffer(CameraHandle, &pContext->map_base_input, &pContext->phy_base_input);
			if(sizeRet <= 0)
			{
				usleep(1000);
				retryCnt--;
			}
			else
			{
				retryCnt = CAM_RETRY_CNT;
				break;
			}
		}
	}
	else if(inputMode == INPUT_MODE_FILE)
	{
		sizeRet = cvLoadImage(pContext->map_base_input, pContext->inputPath, &(pContext->inputWidth), &(pContext->inputHeight));
	}
	else if(inputMode == INPUT_MODE_RTPM)
	{
		sizeRet = MessagePopReceiveBuffer(msgHandle, &pContext->map_base_input, &pContext->phy_base_input, &syncStamp);
	}
	else
	{
		NN_LOG("[ERROR] Invalid inputMode=%d \n", inputMode);
		exit(0);
	}

	t1 = getCurrentTime() * 1000.0;
	NN_LOG("[PERF] GetFrame acquire (Camera/File/RTPM) ms=%.2f\n", t1 - t0);

#ifdef USE_FISHEYE_UNDISTORT
	/* FishEye 왜곡 제거: 사전 계산 맵이 있으면 Remap만, 없으면 기존 방식(매 프레임 맵 계산) */
	if (sizeRet > 0 && pContext->map_base_input != NULL && pContext->fisheyeWorkBuf != NULL &&
	    pContext->fisheyeWorkBufSize >= (size_t)pContext->inputWidth * (size_t)pContext->inputHeight * (pContext->inputImageFormat == IMAGE_FMT_RGBA32 ? 4u : 3u))
	{
		static int fisheye_path_logged;
		if (!fisheye_path_logged) {
			fisheye_path_logged = 1;
			NN_LOG("[PERF] fisheye runtime: %s\n", pContext->fisheyeMapHandle != NULL ? "remap-only" : "fallback (recompute every frame)");
		}
		int ch = (pContext->inputImageFormat == IMAGE_FMT_RGBA32) ? 4 : 3;
		if (pContext->fisheyeMapHandle != NULL)
			cvFisheyeUndistortRemap(pContext->map_base_input, pContext->fisheyeWorkBuf,
				(uint32_t)pContext->inputWidth, (uint32_t)pContext->inputHeight, ch, pContext->fisheyeMapHandle);
		else
		{
#ifndef FISHEYE_UNDISTORT_STRENGTH
#define FISHEYE_UNDISTORT_STRENGTH 0.75f
#endif
			static const float D_base[5] = { 2.0f, 0.f, 0.f, 0.f, 0.f };
			float K[9] = {
				(float)pContext->inputWidth, 0.f, (float)(pContext->inputWidth / 2),
				0.f, (float)pContext->inputWidth, (float)(pContext->inputHeight / 2),
				0.f, 0.f, 1.f
			};
			float D[5] = {
				D_base[0] * FISHEYE_UNDISTORT_STRENGTH,
				D_base[1] * FISHEYE_UNDISTORT_STRENGTH,
				D_base[2] * FISHEYE_UNDISTORT_STRENGTH,
				D_base[3] * FISHEYE_UNDISTORT_STRENGTH,
				D_base[4] * FISHEYE_UNDISTORT_STRENGTH
			};
			cvFisheyeUndistort(pContext->map_base_input, pContext->fisheyeWorkBuf,
				pContext->inputWidth, pContext->inputHeight, ch, K, D);
		}
		memcpy(pContext->map_base_input, pContext->fisheyeWorkBuf, pContext->fisheyeWorkBufSize);
	}
#endif
	t2 = getCurrentTime() * 1000.0;
	NN_LOG("[PERF] GetFrame fisheye ms=%.2f\n", t2 - t1);

	// 카메라 상하 반전 보정: 추론 전에 180도 회전하여 YOLO가 정방향 영상으로 동작
	{
		int ch = (pContext->inputImageFormat == IMAGE_FMT_RGBA32) ? 4 : 3;
		cvRotate180(pContext->map_base_input, pContext->inputWidth, pContext->inputHeight, ch);
	}

	t1 = getCurrentTime() * 1000.0;
	NN_LOG("[PERF] GetFrame rotate180 ms=%.2f\n", t1 - t2);
	NN_LOG("[PERF] GetFrame total ms=%.2f\n", t1 - t0);
}

static int32_t NnReleaseFrame(app_context_t *pContext, CameraHandle cameraHandle, MessageHandle msgHandle)
{
	int32_t ret = -1;
	input_data_type_t input_mode = pContext->inputDataType;
	output_data_type_t output_mode = pContext->outputDataType;

	if(input_mode == INPUT_MODE_CAMERA)
	{
		ret = CameraReleaseBuffer(cameraHandle);
	}
	else if(input_mode == INPUT_MODE_FILE)
	{
		//TBD
		ret = true;
	}
	else if(input_mode == INPUT_MODE_RTPM)
	{
		ret = MessagePushReceiveBuffer(msgHandle, pContext->phy_base_input);
	}
	else{
		//TBD
		ret = true;
	}

	if(output_mode == OUTPUT_MODE_LCD)
	{
		//TBD
		ret = true;
	}
	else if(output_mode == OUTPUT_MODE_FILE)
	{
		NnSetSignalFlag(true);
		ret = true;
	}
	else if(output_mode == OUTPUT_MODE_RTPM)
	{
		//TBD
		ret = true;
	}
	else{
		//TBD
		ret = true;
	}

	return ret;
}


static void NnResizeInputFrame(app_context_t *pContext, ScalerHandle handle, int netIdx)
{
	scaler_size_info_t scalerSrc;
	scaler_size_info_t scalerDes;
	scaler_index_t scalerIdx;

	scalerSrc.pmap = pContext->phy_base_input;
	scalerSrc.width = pContext->inputWidth;
	scalerSrc.height = pContext->inputHeight;
	if(pContext->inputImageFormat == IMAGE_FMT_RGBA32)
	{
		scalerSrc.format = SCALER_FORMAT_ARGB8888;
	}
	else
	{
		scalerSrc.format = SCALER_FORMAT_RGB888;
	}

	scalerIdx = pContext->inferenceContext.neuralNetwork[netIdx].scalerIdx;

	scalerDes.pmap = pContext->inferenceContext.neuralNetwork[netIdx].inputBuf->paddr;
	scalerDes.width = pContext->inferenceContext.neuralNetwork[netIdx].nnWidth;
	scalerDes.height = pContext->inferenceContext.neuralNetwork[netIdx].nnHeight;
	if(pContext->outputImageFormat == IMAGE_FMT_RGBA32)
	{
		scalerDes.format = SCALER_FORMAT_ARGB8888;
	}
	else
	{
		scalerDes.format = SCALER_FORMAT_RGB888;
	}

	ScalerResize(handle, scalerIdx, scalerSrc, scalerDes);
	ScalerPoll(handle, scalerIdx);
}

static void NnResizeOutputFrame(app_context_t *pContext, ScalerHandle scalerHandle, MessageHandle msgHandle)
{
	scaler_size_info_t scalerSrc;
	scaler_size_info_t scalerDes;
	uint32_t outputIdx;
	input_data_type_t input_mode = pContext->inputDataType;
	output_data_type_t output_mode = pContext->outputDataType;

	scalerSrc.pmap = pContext->phy_base_input;
	scalerSrc.width = pContext->inputWidth;
	scalerSrc.height = pContext->inputHeight;
	if(pContext->inputImageFormat == IMAGE_FMT_RGBA32)
	{
		scalerSrc.format = SCALER_FORMAT_ARGB8888;
	}
	else
	{
		scalerSrc.format = SCALER_FORMAT_RGB888;
	}

	outputIdx = pContext->phy_base_output_idx;
	if(output_mode == OUTPUT_MODE_RTPM)
	{
		if(input_mode == INPUT_MODE_CAMERA)
		{
			MessagePopSendBuffer(msgHandle, &pContext->memory_context.map_base_output[outputIdx], &pContext->memory_context.phy_base_output[outputIdx], &pContext->inputRtpmBufferIndex);
		}
	}

	scalerDes.pmap = pContext->memory_context.phy_base_output[outputIdx];
	scalerDes.width = pContext->outputWidth;
	scalerDes.height = pContext->outputHeight;
	if(pContext->outputImageFormat == IMAGE_FMT_RGBA32)
	{
		scalerDes.format = SCALER_FORMAT_ARGB8888;
	}
	else
	{
		scalerDes.format = SCALER_FORMAT_RGB888;
	}

	ScalerResize(scalerHandle, pContext->outputScalerIdx, scalerSrc, scalerDes);
	ScalerPoll(scalerHandle, pContext->outputScalerIdx);
}

/* 객체 인식 시 터미널(stderr)에 Type(cls)과 확률(score) 출력. 소켓/파이프 연동 시 파싱용 한 줄 형식 */
static void NnPrintDetectionResults(app_context_t *pContext, int netIdx)
{
	fprintf(stderr, "[%.1f FPS] ", NnGetFPS());

	network_context_t *pNet = &pContext->inferenceContext.neuralNetwork[netIdx];
	
	if (pNet->type == TELECHIPS_NPU_POST_CLASSIFIER)
	{
		int32_t classId = pNet->resultCls.class_ids[0];
		fprintf(stderr, "[DETECT] net=%d type=cls class_id=%d\n", netIdx, classId);
		fflush(stderr);
	}
	else if (pNet->type == TELECHIPS_NPU_POST_DETECTOR)
	{
		int32_t i;
		int32_t cnt = pNet->resultObj.cnt;
		int32_t best_cls = -1;
		float best_score = -1.0f;
		for (i = 0; i < cnt; i++)
		{
			int32_t cls = pNet->resultObj.obj[i].cls;
			float score = pNet->resultObj.obj[i].score;
			fprintf(stderr, "[DETECT] net=%d cls=%d score=%.4f\n", netIdx, cls, (double)score);
			fflush(stderr);
			if (score > best_score)
			{
				best_score = score;
				best_cls = cls;
			}
		}
		if (best_cls >= 0)
			(void)NnCanSendObjectNow((uint8_t)best_cls, g_latest_capture_ms_u16[netIdx]);
		else
			(void)NnCanSendObjectNow(0xFF, g_latest_capture_ms_u16[netIdx]);
	}
}

static void NnDrawResult(app_context_t *pContext, int netIdx)
{
	unsigned char *outputMapBase = NULL;
	uint16_t outputWidth = 0;
	uint16_t outputHeight = 0;
	network_context_t *pNeuralNetwork = NULL;
	uint32_t outputIdx = 0;
	double fontSize = 0.8;
	int32_t textOffset = 5;
	Color_t redColor = RGB(255, 0, 0);
	Color_t greenColor = RGB(0, 255, 0);
	Color_t whiteColor = RGB(255, 255, 255);
	Color_t customColor = RGB(0, 0, 0);
	int32_t baseXPos = 1600;
	int32_t baseYPos = 100;
	int32_t spacing = 30;
	int32_t currentYPos = baseYPos;

	/* OUTPUT_MODE_NONE이어도 outputs/frames_XXX.png 저장을 위해 그리기 수행 */
	outputIdx = pContext->phy_base_output_idx;
	outputMapBase = pContext->memory_context.map_base_output[outputIdx];
	outputWidth = pContext->outputWidth;
	outputHeight = pContext->outputHeight;

	pNeuralNetwork = &pContext->inferenceContext.neuralNetwork[netIdx];

	if(pNeuralNetwork->type == TELECHIPS_NPU_POST_CLASSIFIER)
	{
		int32_t xPos = 100;
		int32_t yPos = 150;
		customColor = redColor;

		cvDrawCls(outputMapBase, outputWidth, outputHeight, pNeuralNetwork->resultCls.class_ids[0], xPos, yPos, customColor, fontSize);
	}
	else if(pNeuralNetwork->type == TELECHIPS_NPU_POST_DETECTOR)
	{
		Box_t boundingBoxes[256];
		uint16_t boxImageWidth = 0;
		uint16_t boxImageHeight = 0;
		int32_t boxCount = pNeuralNetwork->resultObj.cnt;
		customColor = greenColor;

		for (int index = 0; index < boxCount; index++)
		{
			boxImageWidth = pNeuralNetwork->resultObj.obj[index].img_w;
			boxImageHeight = pNeuralNetwork->resultObj.obj[index].img_h;
			boundingBoxes[index].cls = pNeuralNetwork->resultObj.obj[index].cls;
			boundingBoxes[index].score  = pNeuralNetwork->resultObj.obj[index].score;
			boundingBoxes[index].xmin = (uint16_t)(pNeuralNetwork->resultObj.obj[index].x_min + 0.5);
			boundingBoxes[index].ymin  = (uint16_t)(pNeuralNetwork->resultObj.obj[index].y_min + 0.5);
			boundingBoxes[index].xmax  = (uint16_t)(pNeuralNetwork->resultObj.obj[index].x_max + 0.5);
			boundingBoxes[index].ymax  = (uint16_t)(pNeuralNetwork->resultObj.obj[index].y_max + 0.5);
		}
		if(boxCount > 0)
		{
			cvDrawBoxes(outputMapBase, boundingBoxes, boxCount, outputWidth, outputHeight, boxImageWidth, boxImageHeight, customColor, customColor, fontSize, textOffset);
		}
	}
	else if(pNeuralNetwork->type == TELECHIPS_NPU_POST_CUSTOM)
	{
		;
	}
	else
	{
		// TELECHIPS_NPU_POST_NONE or Invaild PostProccess...
		;
	}
	cvDrawInfo(outputMapBase, outputWidth, outputHeight, DRAW_INFO_NETWORK, pNeuralNetwork->inferenceTime, netIdx, baseXPos, currentYPos, fontSize, customColor);
	currentYPos += spacing;
	cvDrawInfo(outputMapBase, outputWidth, outputHeight, DRAW_INFO_NPU, pNeuralNetwork->npuUtilization, netIdx, baseXPos, currentYPos, fontSize, customColor);
	currentYPos += spacing;

	//for common draw
	cvDrawInfo(outputMapBase, outputWidth, outputHeight, DRAW_INFO_CPU, pContext->perfContext.pPerfInfo.cpuUtil[0], 0, baseXPos, currentYPos, fontSize, whiteColor);
	currentYPos += spacing;
	cvDrawInfo(outputMapBase, outputWidth, outputHeight, DRAW_INFO_MEMORY, pContext->perfContext.pPerfInfo.memUsage, 0, baseXPos, currentYPos, fontSize, whiteColor);

	cvDrawInfo(outputMapBase, outputWidth, outputHeight, DRAW_INFO_FPS, NnGetFPS(), 0, 100, 100, fontSize, whiteColor);
}

static int32_t NnOutputResultFrame(app_context_t *pContext, DisplayHandle dispHandle, MessageHandle msgHandle)
{
	int32_t sizeRet = -1;
	uint32_t phyBaseOutputIdx = pContext->phy_base_output_idx;
	unsigned char *outputMapBase;
	uint64_t outputPhyBase;
	uint32_t outputWidth;
	uint32_t outputHeight;

	input_data_type_t input_mode = pContext->inputDataType;
	output_data_type_t output_mode = pContext->outputDataType;

	outputMapBase = pContext->memory_context.map_base_output[phyBaseOutputIdx];
	outputPhyBase = pContext->memory_context.phy_base_output[phyBaseOutputIdx];
	outputWidth = pContext->outputWidth;
	outputHeight = pContext->outputHeight;

	switch (output_mode)
	{
		case OUTPUT_MODE_LCD:
			/* LCD mode */
			DisplayShow(dispHandle, outputPhyBase, pContext->outputLcdSx, pContext->outputLcdSy, outputWidth, outputHeight);
			break;

		case OUTPUT_MODE_FILE:
			/* File save mode */
			cvSaveImage(outputMapBase, pContext->outputPath, outputWidth, outputHeight);
			break;

		case OUTPUT_MODE_RTPM:
			if(input_mode == INPUT_MODE_CAMERA)
			{
				MessagePushSendBuffer(msgHandle, outputMapBase, pContext->inputRtpmBufferIndex);
			}
			break;

		case OUTPUT_MODE_NONE:
			break;

		default:
			// TBD
			break;
	}

	pContext->phy_base_output_idx = (phyBaseOutputIdx + 1) % 2;

	return sizeRet;
}

/* ---------- 공유 메모리 export (OpenCV 프로세스에 raw 프레임 전달) ----------
 * 색공간: 카메라(V4L2 RGB32) → 입력/모델/출력버퍼까지 전부 RGB.
 * 모델(IMAGE_FMT_RGB24)에도 RGB 입력으로 인식 유리.
 * OpenCV(소켓/SHM 소비자)는 BGR 기준이므로, SHM 쓰기 직전에 RGB→BGR 1회만 수행.
 * (연산량은 소비자에서 하든 생산자에서 하든 동일; 여기서 하면 OpenCV 쪽 변환 제거)
 */
static int g_export_shm_fd = -1;
static void *g_export_shm_base = NULL;
static size_t g_export_shm_size = 0;
static uint32_t g_export_shm_write_slot = 0;

static int NnExportShmInit(app_context_t *pContext)
{
	uint32_t w = pContext->outputWidth;
	uint32_t h = pContext->outputHeight;
	size_t frame_size = (size_t)w * (size_t)h * 3U;
	size_t total = NN_EXPORT_SHM_HEADER_SIZE + 2U * frame_size;

	g_export_shm_fd = shm_open(NN_EXPORT_SHM_NAME, O_CREAT | O_RDWR, 0666);
	if (g_export_shm_fd < 0)
	{
		NN_LOG("[WARN] NnExportShmInit shm_open failed: %s\n", strerror(errno));
		return -1;
	}
	if (ftruncate(g_export_shm_fd, (off_t)total) != 0)
	{
		NN_LOG("[WARN] NnExportShmInit ftruncate failed\n");
		close(g_export_shm_fd);
		g_export_shm_fd = -1;
		return -1;
	}
	g_export_shm_base = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, g_export_shm_fd, 0);
	if (g_export_shm_base == MAP_FAILED)
	{
		NN_LOG("[WARN] NnExportShmInit mmap failed\n");
		close(g_export_shm_fd);
		g_export_shm_fd = -1;
		return -1;
	}
	g_export_shm_size = total;
	g_export_shm_write_slot = 0;
	nn_export_shm_header_t *hp = (nn_export_shm_header_t *)g_export_shm_base;
	hp->magic = NN_EXPORT_SHM_MAGIC;
	hp->version = NN_EXPORT_SHM_VERSION;
	hp->width = w;
	hp->height = h;
	hp->stride = w * 3;
	hp->write_slot = 0;
	hp->frame_index = 0;
	NN_LOG("[INFO] NnExportShmInit ok: %s size=%zu (%ux%u)\n", NN_EXPORT_SHM_NAME, total, w, h);
	return 0;
}

static void NnExportShmDeinit(void)
{
	if (g_export_shm_base != NULL && g_export_shm_base != MAP_FAILED)
	{
		munmap(g_export_shm_base, g_export_shm_size);
		g_export_shm_base = NULL;
		g_export_shm_size = 0;
	}
	if (g_export_shm_fd >= 0)
	{
		close(g_export_shm_fd);
		g_export_shm_fd = -1;
	}
	shm_unlink(NN_EXPORT_SHM_NAME);
}

static void NnExportFrameToShm(app_context_t *pContext)
{
	if (g_export_shm_base == NULL || g_export_shm_size == 0)
		return;
	uint32_t idx = pContext->phy_base_output_idx;
	unsigned char *outBuf = pContext->memory_context.map_base_output[idx];
	if (outBuf == NULL)
		return;
	nn_export_shm_header_t *hp = (nn_export_shm_header_t *)g_export_shm_base;
	uint32_t w = hp->width;
	uint32_t h_val = hp->height;
	size_t frame_size = (size_t)w * (size_t)h_val * 3U;
	if (NN_EXPORT_SHM_HEADER_SIZE + 2U * frame_size > g_export_shm_size)
		return;
	uint32_t slot = g_export_shm_write_slot;
	unsigned char *dst = (unsigned char *)g_export_shm_base + NN_EXPORT_SHM_HEADER_SIZE + slot * frame_size;
	/* 내부 파이프라인은 RGB; OpenCV(소켓/SHM 소비자)는 BGR 사용 → 경계에서 1회만 변환 */
	cvRgbToBgrCopy(outBuf, dst, w, h_val);
	g_export_shm_write_slot = 1U - slot;
	hp->write_slot = g_export_shm_write_slot;
	hp->frame_index++;
}

/* ---------- 비동기 영상 녹화 (VideoWriter, -R 옵션) ---------- */
#define VIDEO_RECORDER_FPS 30.0
static unsigned char *g_video_recorder_buf = NULL;
static size_t g_video_recorder_buf_size = 0;
static void *g_video_recorder_writer = NULL;
static pthread_t g_video_recorder_thread;
static pthread_mutex_t g_video_recorder_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_video_recorder_cond = PTHREAD_COND_INITIALIZER;
static volatile int g_video_recorder_has_new = 0;
static volatile int g_video_recorder_running = 0;
static uint32_t g_video_recorder_w = 0;
static uint32_t g_video_recorder_h = 0;
static double g_video_recorder_frame_time = 0.0;   /* enqueue 시각 (초) */
static double g_video_recorder_last_written_time = 0.0;

static void *NnVideoRecorderThread(void *arg)
{
	(void)arg;
	unsigned char *local_buf = NULL;
	size_t local_size = 0;
	while (g_video_recorder_running)
	{
		pthread_mutex_lock(&g_video_recorder_mutex);
		while (!g_video_recorder_has_new && g_video_recorder_running)
			pthread_cond_wait(&g_video_recorder_cond, &g_video_recorder_mutex);
		if (!g_video_recorder_running)
		{
			pthread_mutex_unlock(&g_video_recorder_mutex);
			break;
		}
		if (g_video_recorder_buf && g_video_recorder_buf_size > 0)
		{
			local_size = g_video_recorder_buf_size;
			local_buf = (unsigned char *)malloc(local_size);
			if (local_buf)
				memcpy(local_buf, g_video_recorder_buf, local_size);
			g_video_recorder_has_new = 0;
		}
		double frame_time = g_video_recorder_frame_time;
		pthread_mutex_unlock(&g_video_recorder_mutex);
		if (local_buf && g_video_recorder_writer && g_video_recorder_w && g_video_recorder_h)
		{
			/* 재생 시 이 프레임이 경과 시간만큼 유지되도록 같은 프레임을 여러 번 기록 */
			int n = 1;
			if (g_video_recorder_last_written_time > 0.0)
			{
				double delta = frame_time - g_video_recorder_last_written_time;
				n = (int)(delta * VIDEO_RECORDER_FPS + 0.5);
				if (n < 1)
					n = 1;
				if (n > 300)
					n = 300;  /* 최대 10초 분량 */
			}
			g_video_recorder_last_written_time = frame_time;
			for (int i = 0; i < n; i++)
				cvVideoWriterWrite(g_video_recorder_writer, local_buf, g_video_recorder_w, g_video_recorder_h);
			free(local_buf);
			local_buf = NULL;
		}
	}
	return NULL;
}

static int NnVideoRecorderInit(app_context_t *pContext)
{
	if (!pContext->recordVideo || pContext->outputsBaseDir[0] == '\0')
		return 0;
	uint32_t w = pContext->outputWidth;
	uint32_t h = pContext->outputHeight;
	size_t frame_size = (size_t)w * (size_t)h * 3U;
	char path[PATH_MAX + 32];
	(void)snprintf(path, sizeof(path), "%s/outputs/record.avi", pContext->outputsBaseDir);
	g_video_recorder_buf = (unsigned char *)malloc(frame_size);
	if (!g_video_recorder_buf)
		return -1;
	g_video_recorder_buf_size = frame_size;
	g_video_recorder_w = w;
	g_video_recorder_h = h;
	g_video_recorder_writer = cvVideoWriterCreate(path, w, h, VIDEO_RECORDER_FPS);
	if (!g_video_recorder_writer)
	{
		free(g_video_recorder_buf);
		g_video_recorder_buf = NULL;
		return -1;
	}
	g_video_recorder_has_new = 0;
	g_video_recorder_running = 1;
	if (pthread_create(&g_video_recorder_thread, NULL, NnVideoRecorderThread, NULL) != 0)
	{
		cvVideoWriterRelease(&g_video_recorder_writer);
		free(g_video_recorder_buf);
		g_video_recorder_buf = NULL;
		g_video_recorder_running = 0;
		return -1;
	}
	NN_LOG("[INFO] NnVideoRecorderInit ok: %s\n", path);
	return 0;
}

static void NnVideoRecorderDeinit(void)
{
	g_video_recorder_running = 0;
	pthread_cond_signal(&g_video_recorder_cond);
	if (g_video_recorder_thread)
	{
		pthread_join(g_video_recorder_thread, NULL);
		g_video_recorder_thread = (pthread_t)0;
	}
	if (g_video_recorder_writer)
	{
		cvVideoWriterRelease(&g_video_recorder_writer);
		g_video_recorder_writer = NULL;
	}
	if (g_video_recorder_buf)
	{
		free(g_video_recorder_buf);
		g_video_recorder_buf = NULL;
	}
	g_video_recorder_buf_size = 0;
}

static void NnVideoRecorderEnqueue(app_context_t *pContext)
{
	if (!g_video_recorder_writer || !g_video_recorder_buf)
		return;
	uint32_t idx = pContext->phy_base_output_idx;
	unsigned char *outBuf = pContext->memory_context.map_base_output[idx];
	if (!outBuf)
		return;
	size_t frame_size = (size_t)pContext->outputWidth * (size_t)pContext->outputHeight * 3U;
	if (frame_size != g_video_recorder_buf_size)
		return;
	pthread_mutex_lock(&g_video_recorder_mutex);
	memcpy(g_video_recorder_buf, outBuf, frame_size);
	g_video_recorder_frame_time = getCurrentTime();
	g_video_recorder_has_new = 1;
	pthread_cond_signal(&g_video_recorder_cond);
	pthread_mutex_unlock(&g_video_recorder_mutex);
}

static int32_t NnOutputResultData(app_context_t *pContext, MessageHandle msgHandle, int netIdx)
{
	int32_t sizeRet = -1;
	output_data_type_t output_mode = pContext->outputDataType;

	if(output_mode == OUTPUT_MODE_RTPM)
	{
		/* Injection mode */
		int32_t objCount[NETWORK_INDEX_MAX];
		boxes_t objs[NETWORK_INDEX_MAX];
		message_result_type_t resultType[NETWORK_INDEX_MAX];

		memset(objCount, 0, sizeof(objCount));
		memset(objs, 0, sizeof(objs));
		memset(resultType, 0, sizeof(resultType));

		// netIdx만 처리
		network_context_t *neural_network = &pContext->inferenceContext.neuralNetwork[netIdx];
		enlight_postproc_t network_detect_type = pContext->inferenceContext.neuralNetwork[netIdx].type;

		switch(network_detect_type)
		{
			case TELECHIPS_NPU_POST_CLASSIFIER:
				RtpmPostProcessClassifierResults(neural_network, resultType, objCount, netIdx);
				break;

			case TELECHIPS_NPU_POST_DETECTOR:
				RtpmPostProcessDetectionResults(objs, neural_network, objCount, resultType, netIdx, pContext->outputWidth, pContext->outputHeight);
				break;

			case TELECHIPS_NPU_POST_CUSTOM:
				// User Custom Area
				break;

			default:
				NN_LOG("[INFO][Unknown Enlight Post-processing][%d]\n", network_detect_type);
				break;
		}

		// 양쪽 슬롯을 전달 (비활성 쪽은 memset=0)
		sizeRet = RtpmSendResultDataAsJson(msgHandle,
											syncStamp,
											resultType[NETWORK_INDEX_0],(uint16_t)objCount[NETWORK_INDEX_0], objs[NETWORK_INDEX_0].boxes,
											resultType[NETWORK_INDEX_1], (uint16_t)objCount[NETWORK_INDEX_1], objs[NETWORK_INDEX_1].boxes);
		if(sizeRet < 0)
		{
			NN_LOG("[ERROR] [RtpmSendResultDataAsJson] size : %d\n", sizeRet);
		}
		else
		{
			NN_LOG("[INFO] [RtpmSendResultDataAsJson] size : %d\n", sizeRet);
		}

		sizeRet = RtpmSendInferencePerformanceToRTPM(msgHandle, &pContext->inferenceContext);
		if(sizeRet < 0)
		{
			NN_LOG("[ERROR] [RtpmSendInferencePerformanceToRTPM] size : %d\n", sizeRet);
		}
		else
		{
			NN_LOG("[INFO] [RtpmSendInferencePerformanceToRTPM] size : %d\n", sizeRet);
		}
	}
	else if(output_mode == OUTPUT_MODE_FILE)
	{
		/* File mode */
		// TODO : Save yolo format or coco format
	}
	else /* OUTPUT_MODE_LCD mode */
	{
		;
	}

	return sizeRet;
}

static void NnPerfMonitorInit(app_context_t *pContext, MessageHandle msgHandle)
{
	createSystemMonitorThread(&pContext->perfContext);				// Init Performance Monitor

	if(pContext->inferenceContext.npuRunMode == NPU_RUN_ASYNC) //profile mode, NPU CNT set to zero...
	{
		createNpuResourcesMonitorThread(&pContext->inferenceContext);
	}
	else
	{
		NN_LOG("[INFO] [%s] Monitoring of NPU usage cannot be used in sync mode.\n", __FUNCTION__);
	}

	if(pContext->inputDataType == INPUT_MODE_RTPM || pContext->outputDataType == OUTPUT_MODE_RTPM)
	{
		RtpmCreatePerfmanceDataSendThread(msgHandle);
	}
	else
	{
		NN_LOG("[INFO] [%s] Input/Output mode is not RTPM. Performance data send thread not created.\n", __FUNCTION__);
	}
}

static void NnPerfMonitorDeinit(app_context_t *pContext)
{
	destroySystemMonitorThread();

	if(pContext->inferenceContext.npuRunMode == NPU_RUN_ASYNC) //profile mode, NPU CNT set to zero...
	{
		destroyNpuResourcesMonitorThread();
	}
	else
	{
		NN_LOG("[INFO] [%s] Monitoring of NPU usage cannot be used in sync mode.\n", __FUNCTION__);
	}

	if(pContext->inputDataType == INPUT_MODE_RTPM || pContext->outputDataType == OUTPUT_MODE_RTPM)
	{
		RtpmDestroyPerfmanceDataSendThread();
	}
	else
	{
		NN_LOG("[INFO] [%s] Input/Output mode is not RTPM. Performance data send thread not destruction.\n", __FUNCTION__);
	}
}

static int32_t NnCreateAPI(app_context_t *pContext, app_obj_t *pObj)
{
	int32_t ret = -1;

	if(pContext->inputDataType == INPUT_MODE_CAMERA)
	{
		ret = CameraCreate(&pObj->cam_handle);
		if (ret < 0)
		{
			NN_LOG("[ERROR] [Camera Create] fail : %d\n", ret);
			exit(1);
		}
		else
		{
			NN_LOG("[INFO] [Camera Create] successs : %d\n", ret);
		}
	}
	else
	{
		NN_LOG("[INFO] Camera capture mode is not activated\n");
	}

	ret = ScalerCreate(&pObj->scaler_handle);
	if (ret < 0)
	{
		NN_LOG("[ERROR] ScalerCreate() error\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		NN_LOG("[INFO] ScalerCreate() success\n");
	}

	if(pContext->outputDataType == OUTPUT_MODE_LCD)
	{
		ret = DisplayCreate(&pObj->display_handle);
		if (ret < 0)
		{
			printf("[ERROR] Display create error\n");
			exit(EXIT_FAILURE);
		}
		else
		{
			NN_LOG("[INFO] Display create success\n");
		}
	}
	else
	{
		NN_LOG("[INFO] LCD Output mode is not activated\n");
	}

	if(pContext->inputDataType == INPUT_MODE_RTPM || pContext->outputDataType == OUTPUT_MODE_RTPM)
	{
		ret = MessageCreate(&pObj->msg_handle);
		if (ret < 0)
		{
			printf("[ERROR] Message create error\n");
			exit(EXIT_FAILURE);
		}
		else
		{
			NN_LOG("[INFO] Message create success\n");
		}
	}
	else
	{
		NN_LOG("[INFO] RTPM mode is not activated\n");
	}

	return ret;
}

static int32_t NnDestroyAPI(app_context_t *pContext, app_obj_t *pObj)
{
	int32_t ret = -1;

	if(pContext->inputDataType == INPUT_MODE_CAMERA)
	{
		ret = CameraDestroy(pObj->cam_handle);
		printf("[INFO] Camera capture mode destruction Done\n");
	}
	else
	{
		NN_LOG("[INFO] Camera capture mode is not activated\n");
	}

	if(pContext->outputDataType == OUTPUT_MODE_LCD)
	{
		ret = DisplayDestroy(pObj->display_handle);
		printf("[INFO] LCD Output mode destruction Done\n");
	}
	else
	{
		NN_LOG("[INFO] LCD Output mode is not activated\n");
	}

	ret = ScalerDestroy(pObj->scaler_handle);
	if(ret < 0)
	{
		printf("[INFO] Scaler destruction Fail\n");
	}
	else
	{
		NN_LOG("[INFO] Scaler destruction Done\n");
	}

	if(pContext->inputDataType == INPUT_MODE_RTPM || pContext->outputDataType == OUTPUT_MODE_RTPM)
	{
		ret = MessageDestroy(pObj->msg_handle);
		printf("[INFO] RTPM mode destruction Done\n");
	}
	else
	{
		NN_LOG("[INFO] RTPM mode is not activated\n");
	}

	return ret;
}

int main(int argc, char **argv)
{
	app_context_t *pContext = NULL;
	param_info_t *pParam = NULL;
	app_obj_t *pObj = &g_AppObj;

	pContext = (app_context_t *)malloc(sizeof(app_context_t));
	pParam = (param_info_t *)malloc(sizeof(param_info_t));

	/* Initialize */
	NnparseArgs(pParam, argc, argv);																				// Parsing parameters
	NnModeChecker(pParam);																							// Check mode
	NnInitAppContext(pContext, pParam);																				// Init App Context
	NnEnsureOutputsDirReady(pContext);																				// outputs/ 생성 및 내용 비우기
	NnSignalChecker();																								// Handle Segfault
	NnSetDebugMode(pContext->debugMode, pContext->inferenceContext.npuDebugMode);									// Set Debug Log Level
	NnCreateAPI(pContext, pObj);																					// Create API handle
	NnMemoryInit(&pContext->memory_context, pContext->inputPath, pContext->outputWidth, pContext->outputHeight);	// Init Memory
	NnInputModeInit(pContext, pObj->cam_handle, pObj->msg_handle);													// Init input mode
	NnOutputModeInit(pContext, pObj->display_handle, pObj->msg_handle);												// Init output mode
	NnScalerInit(pObj->scaler_handle);																				// Init Scaler

	(void)NnExportShmInit(pContext);																					// 공유 메모리 export (OpenCV 프로세스용)
	(void)NnCanLaneSenderInit();																						// CAN 송신 스레드 (lane_status SHM 소비)
	(void)NnVideoRecorderInit(pContext);																				// 비동기 영상 녹화 (-R 시)

	NnPerfMonitorInit(pContext, pObj->msg_handle);

#if defined(TCC7500) || defined(TCC7501)          // Dual Cluster
	/* The initialization order of NPU_CLUSTER_INDEX_0 and NPU_CLUSTER_INDEX_1 must not be changed. */
	NnNpuInit(&pContext->inferenceContext, NPU_CLUSTER_INDEX_0);
	NnNpuInit(&pContext->inferenceContext, NPU_CLUSTER_INDEX_1);

	NnNeuralNetworkInit(&pContext->inferenceContext, NPU_CLUSTER_INDEX_0 ,NETWORK_INDEX_0, SCALER_INDEX_0, IMAGE_FMT_RGB24); // Init Network, set pipeline, npu cluster - network
	NnNeuralNetworkInit(&pContext->inferenceContext, NPU_CLUSTER_INDEX_1, NETWORK_INDEX_1, SCALER_INDEX_1, IMAGE_FMT_RGB24); // Init Network, set pipeline, npu cluster - network
#else                                             // Single Cluster
	NnNpuInit(&pContext->inferenceContext, NPU_CLUSTER_INDEX_0);

	NnNeuralNetworkInit(&pContext->inferenceContext, NPU_CLUSTER_INDEX_0 ,NETWORK_INDEX_0, SCALER_INDEX_0, IMAGE_FMT_RGB24); // Init Network, set pipeline, npu cluster - network
	NnNeuralNetworkInit(&pContext->inferenceContext, NPU_CLUSTER_INDEX_1, NETWORK_INDEX_1, SCALER_INDEX_1, IMAGE_FMT_RGB24); // Init Network, set pipeline, npu cluster - network
#endif

#ifdef INTERACTIVE_MODE
	pthread_create(&g_InteractiveThread, NULL, NnInteractive, NULL);
#endif // _INTERACTIVE_MODE

	/* Run - Pipelined dual-NPU processing */
	int curNet = 0;
	pthread_t infThread;
	int hasPrev = 0;

	while(NnCheckExitFlag() != true)
	{
		double t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] frame start ms=%.2f\n", t_ms);

		// Phase A: Output previous frame results (when hasPrev is set)
		if (hasPrev) {
			int prevNet = 1 - curNet;
			
			// Wait for previous inference to complete
			NnWaitInference(infThread);
			
			// Step 3-1. Print detection results
			t_ms = getCurrentTime() * 1000.0;
			NN_LOG("[PERF] Step3-1 NnPrintDetectionResults start ms=%.2f\n", t_ms);
			NnPrintDetectionResults(pContext, prevNet);
			
			// Step 5. Draw result on previous output buffer
			t_ms = getCurrentTime() * 1000.0;
			NN_LOG("[PERF] Step5 NnDrawResult start ms=%.2f\n", t_ms);
			NnDrawResult(pContext, prevNet);
			
			if (pContext->recordVideo)
				NnVideoRecorderEnqueue(pContext);
			pContext->outputFrameCounter++;
			
			// Step 6. Output Frame
			t_ms = getCurrentTime() * 1000.0;
			NN_LOG("[PERF] Step6 NnOutputResultFrame start ms=%.2f\n", t_ms);
			NnOutputResultFrame(pContext, pObj->display_handle, pObj->msg_handle);
			
			// Step 7. Output Data
			t_ms = getCurrentTime() * 1000.0;
			NN_LOG("[PERF] Step7 NnOutputResultData start ms=%.2f\n", t_ms);
			NnOutputResultData(pContext, pObj->msg_handle, prevNet);
		}

		// Phase B: Capture and prepare new frame
		// Step 1. Get frame: 0.1ms
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step1 GetFrame start ms=%.2f\n", t_ms);
		NnGetFrame(pContext, pObj->cam_handle, pObj->msg_handle);
		/* Capture timestamp (epoch-based) right after frame acquisition */
		{
			struct timespec ts;
			if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
			{
				uint64_t ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
				g_latest_capture_ms_u16[curNet] = (uint16_t)(ms & 0xFFFFU);
			}
		}
		
		// Step 2. Resize frame for inference: 10ms
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step2 ResizeInputFrame start ms=%.2f\n", t_ms);
		NnResizeInputFrame(pContext, pObj->scaler_handle, curNet);
		
		// Step 4. Resize frame for output device: 5ms + depend on tcp send
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step4 NnResizeOutputFrame start ms=%.2f\n", t_ms);
		NnResizeOutputFrame(pContext, pObj->scaler_handle, pObj->msg_handle);
		
		// Step 4.6. Export raw frame to SHM (OSD 없는 원본, lane_tuning용)
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step4-6 ExportFrameToShm start ms=%.2f\n", t_ms);
		NnExportFrameToShm(pContext);
		
		// Step 8. Release Frame
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step8 NnReleaseFrame start ms=%.2f\n", t_ms);
		NnReleaseFrame(pContext, pObj->cam_handle, pObj->msg_handle);

		// Phase C: Start async inference on current NPU
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step3 NnStartInferenceAsync start ms=%.2f\n", t_ms);
		NnStartInferenceAsync(&pContext->inferenceContext, curNet, &infThread);

		hasPrev = 1;
		curNet = 1 - curNet;  // Alternate between NPU 0 and 1

#ifdef DEV_LIMIT_1FPS
		sleep(1);  /* 개발용: 1초에 1프레임 */
#endif
	}

	// Drain: Output last frame results
	if (hasPrev) {
		int prevNet = 1 - curNet;
		double t_ms;
		
		// Wait for last inference to complete
		NnWaitInference(infThread);
		
		// Output last frame
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step3-1 NnPrintDetectionResults (last frame) start ms=%.2f\n", t_ms);
		NnPrintDetectionResults(pContext, prevNet);
		
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step5 NnDrawResult (last frame) start ms=%.2f\n", t_ms);
		NnDrawResult(pContext, prevNet);
		
		if (pContext->recordVideo)
			NnVideoRecorderEnqueue(pContext);
		pContext->outputFrameCounter++;
		
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step6 NnOutputResultFrame (last frame) start ms=%.2f\n", t_ms);
		NnOutputResultFrame(pContext, pObj->display_handle, pObj->msg_handle);
		
		t_ms = getCurrentTime() * 1000.0;
		NN_LOG("[PERF] Step7 NnOutputResultData (last frame) start ms=%.2f\n", t_ms);
		NnOutputResultData(pContext, pObj->msg_handle, prevNet);
	}

	/* Deinitialize */
	NnVideoRecorderDeinit();
	NnCanLaneSenderDeinit();
	NnExportShmDeinit();
	NnNeuralNetworkDeinit(&pContext->inferenceContext, NETWORK_INDEX_0);
	NnNeuralNetworkDeinit(&pContext->inferenceContext, NETWORK_INDEX_1);
	NnNpuDeinit(&pContext->inferenceContext);
	NnScalerDenit(pObj->scaler_handle);
	NnInputModeDeinit(pContext, pObj->cam_handle, pObj->msg_handle);
	NnOutputModeDeinit(pContext, pObj->display_handle, pObj->msg_handle);
	NnMemoryDeinit(&pContext->memory_context, pContext->outputWidth, pContext->outputHeight);

	NnDestroyInferenceThread();

	NnPerfMonitorDeinit(pContext);

	// API Deinitialize
	NnDestroyAPI(pContext, pObj);

#ifdef INTERACTIVE_MODE
	pthread_join(g_InteractiveThread, NULL);
#endif // _INTERACTIVE_MODE

	free(pParam);
	free(pContext);

	printf("tc-nn-app finish\n");

	return 0;
}



