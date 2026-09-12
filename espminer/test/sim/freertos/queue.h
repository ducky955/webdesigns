#pragma once
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned len, unsigned itemSize);
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait);
BaseType_t xQueueReceive(QueueHandle_t q, void *out, TickType_t wait);
