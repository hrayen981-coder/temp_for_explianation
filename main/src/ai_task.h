#ifndef AI_TASK_H
#define AI_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class AiVisionSystem {
private:
    TaskHandle_t task_handle_ = nullptr;
    bool is_running_ = false;

    static void TaskWrapper(void* arg);
    void Loop();

public:
    AiVisionSystem() = default;
    ~AiVisionSystem();

    void Start();
    void Stop();
};

#endif // AI_TASK_H