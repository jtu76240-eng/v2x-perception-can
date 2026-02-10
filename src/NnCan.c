#include "NnCan.h"

#include "NnDebug.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

/* ---------- CAN lane status sender (reads /dev/shm/lane_status) ---------- */
#define LANE_SHM_NAME "/lane_status"
#define LANE_SHM_MAGIC 0x4C414E45U /* "LANE" */
#define LANE_SHM_SIZE 24
#define CAN_IFNAME "can0"
#define CAN_LANE_STATUS_ID 0x80U
#define CAN_OBJ_STATUS_ID 0x30U

typedef struct __attribute__((packed)) _lane_shm_payload {
	uint32_t magic;
	uint32_t lane_num;
	uint64_t frame_index;
	int64_t timestamp_us;
} lane_shm_payload_t;

static pthread_t g_can_lane_thread;
static volatile int g_can_lane_running = 0;


static int NnCanOpenSocket(const char *ifname)
{
	int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (s < 0)
		return -1;

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0)
	{
		close(s);
		return -1;
	}

	struct sockaddr_can addr;
	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		close(s);
		return -1;
	}
	return s;
}

static int NnCanSendLaneStatus(int can_fd, const lane_shm_payload_t *lane)
{
	struct can_frame frame;
	memset(&frame, 0, sizeof(frame));
	frame.can_id = CAN_LANE_STATUS_ID;
	frame.can_dlc = 8;

	/* Timestamp1: monotonic ms % 65536 */
	struct timespec ts;
	uint16_t ts1 = 0;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
	{
		uint64_t ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
		ts1 = (uint16_t)(ms & 0xFFFFU);
	}

	/* Timestamp0: capture ms % 65536 (fallback to ts1) */
	uint16_t ts0 = ts1;
	if (lane->magic == LANE_SHM_MAGIC && lane->timestamp_us > 0)
	{
		uint64_t cap_ms = (uint64_t)(lane->timestamp_us / 1000);
		ts0 = (uint16_t)(cap_ms & 0xFFFFU);
	}

	static uint8_t counter_lane = 0;

	uint32_t lane_num = lane->lane_num;
	if (lane_num > 5U)
		lane_num = 0;
	frame.data[0] = (uint8_t)(lane_num & 0xFFU);
	frame.data[1] = 0;
	frame.data[2] = (uint8_t)(ts0 & 0xFFU);
	frame.data[3] = (uint8_t)((ts0 >> 8) & 0xFFU);
	frame.data[4] = (uint8_t)(ts1 & 0xFFU);
	frame.data[5] = (uint8_t)((ts1 >> 8) & 0xFFU);
	frame.data[6] = counter_lane;

	/* MAC */
	const uint8_t key[4] = {0x19, 0x99, 0x06, 0x15};
	uint8_t mac = (uint8_t)(0x5A ^ counter_lane);
	for (int i = 0; i <= 6; i++)
	{
		mac ^= frame.data[i];
		mac = (uint8_t)(mac + key[i % 4]);
		mac = (uint8_t)((mac << 1) | (mac >> 7));
	}
	frame.data[7] = mac;

	NN_LOG("[INFO] CAN lane tx: id=0x%X lane=%u cnt=%u mac=%02X data=%02X %02X %02X %02X %02X %02X %02X %02X\n",
		   CAN_LANE_STATUS_ID, (unsigned)lane_num, (unsigned)counter_lane, mac,
		   frame.data[0], frame.data[1], frame.data[2], frame.data[3],
		   frame.data[4], frame.data[5], frame.data[6], frame.data[7]);

	counter_lane++;

	return (write(can_fd, &frame, sizeof(frame)) == (ssize_t)sizeof(frame)) ? 0 : -1;
}

static void *NnCanLaneSenderThread(void *arg)
{
	(void)arg;
	int can_fd = -1;
	int shm_fd = -1;
	uint64_t last_frame_index = 0;

	while (g_can_lane_running)
	{
		if (can_fd < 0)
		{
			can_fd = NnCanOpenSocket(CAN_IFNAME);
			if (can_fd < 0)
			{
				usleep(200 * 1000);
				continue;
			}
			NN_LOG("[INFO] CAN socket opened: %s\n", CAN_IFNAME);
		}

		if (shm_fd < 0)
		{
			shm_fd = open("/dev/shm" LANE_SHM_NAME, O_RDONLY);
			if (shm_fd < 0)
			{
				usleep(200 * 1000);
				continue;
			}
			NN_LOG("[INFO] Lane status SHM connected: /dev/shm%s\n", LANE_SHM_NAME);
		}

		lane_shm_payload_t lane;
		ssize_t nread = pread(shm_fd, &lane, sizeof(lane), 0);
		if (nread != (ssize_t)sizeof(lane))
		{
			usleep(10 * 1000);
			continue;
		}

		if (lane.magic != LANE_SHM_MAGIC)
		{
			lane.lane_num = 0;
			lane.timestamp_us = 0;
		}

		if (lane.frame_index != last_frame_index)
		{
			(void)NnCanSendLaneStatus(can_fd, &lane);
			last_frame_index = lane.frame_index;
		}

		usleep(10 * 1000);
	}

	if (shm_fd >= 0)
		close(shm_fd);
	if (can_fd >= 0)
		close(can_fd);
	return NULL;
}

int NnCanSendObjectNow(uint8_t cls, uint16_t cap_ms_u16)
{
	static int can_fd = -1;
	static uint8_t counter_obj = 0;

	if (can_fd < 0)
	{
		can_fd = NnCanOpenSocket(CAN_IFNAME);
		if (can_fd < 0)
		{
			NN_LOG("[WARN] CAN object socket open failed: %s\n", CAN_IFNAME);
			return -1;
		}
	}

	uint8_t obj_code = 0;
	if (cls != 0xFFU)
		obj_code = (cls == 5U) ? 2U : 1U;

	struct can_frame frame;
	memset(&frame, 0, sizeof(frame));
	frame.can_id = CAN_OBJ_STATUS_ID;
	frame.can_dlc = 8;

	struct timespec ts;
	uint16_t ts1 = 0;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
	{
		uint64_t ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
		ts1 = (uint16_t)(ms & 0xFFFFU);
	}
	uint16_t ts0 = (cap_ms_u16 != 0U) ? cap_ms_u16 : ts1;

	frame.data[0] = obj_code;
	frame.data[1] = 0;
	frame.data[2] = (uint8_t)(ts0 & 0xFFU);
	frame.data[3] = (uint8_t)((ts0 >> 8) & 0xFFU);
	frame.data[4] = (uint8_t)(ts1 & 0xFFU);
	frame.data[5] = (uint8_t)((ts1 >> 8) & 0xFFU);
	frame.data[6] = counter_obj;

	const uint8_t key[4] = {0x19, 0x99, 0x06, 0x15};
	uint8_t mac = (uint8_t)(0x5A ^ counter_obj);
	for (int i = 0; i <= 6; i++)
	{
		mac ^= frame.data[i];
		mac = (uint8_t)(mac + key[i % 4]);
		mac = (uint8_t)((mac << 1) | (mac >> 7));
	}
	frame.data[7] = mac;

	NN_LOG("[INFO] CAN obj tx: id=0x%X cls=%u code=%u cnt=%u mac=%02X data=%02X %02X %02X %02X %02X %02X %02X %02X\n",
		   CAN_OBJ_STATUS_ID, (unsigned)cls, (unsigned)obj_code, (unsigned)counter_obj, mac,
		   frame.data[0], frame.data[1], frame.data[2], frame.data[3],
		   frame.data[4], frame.data[5], frame.data[6], frame.data[7]);

	counter_obj++;

	return (write(can_fd, &frame, sizeof(frame)) == (ssize_t)sizeof(frame)) ? 0 : -1;
}

int NnCanLaneSenderInit(void)
{
	if (g_can_lane_running)
		return 0;
	g_can_lane_running = 1;
	if (pthread_create(&g_can_lane_thread, NULL, NnCanLaneSenderThread, NULL) != 0)
	{
		g_can_lane_running = 0;
		NN_LOG("[WARN] CAN lane sender thread create failed\n");
		return -1;
	}
	return 0;
}

void NnCanLaneSenderDeinit(void)
{
	if (!g_can_lane_running)
		return;
	g_can_lane_running = 0;
	pthread_join(g_can_lane_thread, NULL);
	g_can_lane_thread = (pthread_t)0;
}
