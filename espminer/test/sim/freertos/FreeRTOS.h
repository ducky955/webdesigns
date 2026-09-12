// Functional FreeRTOS stand-in: tasks become std::threads, the mutex and
// queue are real, so the concurrency in miner.cpp is genuinely exercised.
#pragma once
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portMAX_DELAY 0xFFFFFFFF
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
void sim_enter_critical();
void sim_exit_critical();
#define taskENTER_CRITICAL(mux) do { (void)(mux); sim_enter_critical(); } while (0)
#define taskEXIT_CRITICAL(mux)  do { (void)(mux); sim_exit_critical(); } while (0)
