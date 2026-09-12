// Implementation of the host stand-ins: clock, threads, queue, mutex.
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <chrono>
#include <thread>
#include <mutex>
#include <deque>
#include <vector>
#include <cstring>

SerialClass Serial;
EspClass    ESP;

static const auto t0 = std::chrono::steady_clock::now();

unsigned long millis() {
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - t0).count();
}
void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
unsigned getCpuFrequencyMhz() { return 240; }

static std::mutex g_critical;
void sim_enter_critical() { g_critical.lock(); }
void sim_exit_critical() { g_critical.unlock(); }

void vTaskDelay(TickType_t ticks) { delay(ticks ? ticks : 1); }
void vTaskDelete(TaskHandle_t) {}

BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *), const char *, uint32_t, void *arg,
                                   unsigned, TaskHandle_t *out, int) {
    std::thread(fn, arg).detach();
    if (out) *out = nullptr;
    return pdTRUE;
}

SemaphoreHandle_t xSemaphoreCreateMutex() { return new std::mutex(); }
BaseType_t xSemaphoreTake(SemaphoreHandle_t h, TickType_t) {
    ((std::mutex *)h)->lock();
    return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t h) {
    ((std::mutex *)h)->unlock();
    return pdTRUE;
}

struct SimQueue {
    unsigned itemSize, capacity;
    std::deque<std::vector<uint8_t>> items;
    std::mutex m;
};

QueueHandle_t xQueueCreate(unsigned len, unsigned itemSize) {
    SimQueue *q = new SimQueue();
    q->itemSize = itemSize;
    q->capacity = len;
    return q;
}
BaseType_t xQueueSend(QueueHandle_t h, const void *item, TickType_t) {
    SimQueue *q = (SimQueue *)h;
    std::lock_guard<std::mutex> lock(q->m);
    if (q->items.size() >= q->capacity) return 0;
    q->items.emplace_back((const uint8_t *)item, (const uint8_t *)item + q->itemSize);
    return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t h, void *out, TickType_t) {
    SimQueue *q = (SimQueue *)h;
    std::lock_guard<std::mutex> lock(q->m);
    if (q->items.empty()) return 0;
    memcpy(out, q->items.front().data(), q->itemSize);
    q->items.pop_front();
    return pdTRUE;
}
