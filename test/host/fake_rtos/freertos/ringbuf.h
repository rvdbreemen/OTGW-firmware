// TASK-1736 host fake of the ESP-IDF byte ring buffer API that cbuf.cpp calls.
// The ring keeps byte-buffer semantics (FIFO, a send that does not fit fails) and
// reports every create and delete to the allocation tracker in the test.
#pragma once
#include "FreeRTOS.h"
typedef void *RingbufHandle_t;
typedef enum { RINGBUF_TYPE_NOSPLIT = 0, RINGBUF_TYPE_ALLOWSPLIT, RINGBUF_TYPE_BYTEBUF } RingbufferType_t;
#ifdef __cplusplus
extern "C" {
#endif
RingbufHandle_t xRingbufferCreate(size_t xBufferSize, RingbufferType_t xBufferType);
void vRingbufferDelete(RingbufHandle_t xRingbuffer);
size_t xRingbufferGetMaxItemSize(RingbufHandle_t xRingbuffer);
size_t xRingbufferGetCurFreeSize(RingbufHandle_t xRingbuffer);
void *xRingbufferReceiveUpTo(RingbufHandle_t xRingbuffer, size_t *pxItemSize, TickType_t xTicksToWait, size_t xMaxSize);
void vRingbufferReturnItem(RingbufHandle_t xRingbuffer, void *pvItem);
BaseType_t xRingbufferSend(RingbufHandle_t xRingbuffer, const void *pvItem, size_t xItemSize, TickType_t xTicksToWait);
void vRingbufferGetInfo(RingbufHandle_t xRingbuffer, UBaseType_t *uxFree, UBaseType_t *uxRead, UBaseType_t *uxWrite,
                        UBaseType_t *uxAcquire, UBaseType_t *uxItemsWaiting);
#ifdef __cplusplus
}
#endif
