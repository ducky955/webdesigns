#pragma once
#include "FreeRTOS.h"
typedef void *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t h, TickType_t wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t h);
