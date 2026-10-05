#ifndef NN_CAN_H
#define NN_CAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int NnCanLaneSenderInit(void);
void NnCanLaneSenderDeinit(void);
int NnCanSendObjectNow(uint8_t cls, uint16_t capture_ms_u16);

#ifdef __cplusplus
}
#endif

#endif /* NN_CAN_H */
