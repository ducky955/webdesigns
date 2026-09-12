#pragma once
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
void vTaskDelay(TickType_t ticks);
void vTaskDelete(TaskHandle_t h);
BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, uint32_t stack,
                                   void *arg, unsigned prio, TaskHandle_t *out, int core);
