#include "ai_task.h"
#include "esp_camera.h"
#include "application.h"
#include "board.h"
#include "esp_log.h"
#include <future>
#include "display.h"
// 引入你的原生 AI 组件
#include "hand_detect.hpp"
#include "hand_gesture_recognition.hpp"

using namespace dl::image;

static const char* TAG = "AiVision";

void AiVisionSystem::Start() {
    if (is_running_) return;
    is_running_ = true;
    
    // 启动 AI 任务，绑定在 Core 0
    xTaskCreatePinnedToCore(
        &AiVisionSystem::TaskWrapper,
        "ai_vision_task",
        16384, // 保持你原来的大栈空间
        this,
        5,
        &task_handle_,
        0 
    );
    ESP_LOGI(TAG, "AI Vision Task Started on Core 0");
}

void AiVisionSystem::TaskWrapper(void* arg) {
    static_cast<AiVisionSystem*>(arg)->Loop();
}

void AiVisionSystem::Loop() {
    auto camera = Board::GetInstance().GetCamera();
    if (!camera) {
        ESP_LOGE(TAG, "未找到摄像头硬件组件，AI 任务退出");
        vTaskDelete(NULL);
        return;
    }

    // 1. 初始化两级手势模型
    ESP_LOGI(TAG, "正在初始化 手势识别级联模型...");
    HandDetect hand_detector;
    HandGestureRecognizer gesture_recognizer;
    
    // 提前构建 img_t 结构体外壳
    dl::image::img_t img = {
        .data       = nullptr, 
        .width      = 320,
        .height     = 240,
        .pix_type   = dl::image::DL_IMAGE_PIX_TYPE_RGB565BE, 
    };

    while (is_running_) {
        auto& app = Application::GetInstance();
        std::promise<const camera_fb_t*> fb_promise;
        auto fb_future = fb_promise.get_future();

        app.Schedule([camera, &fb_promise]() {
            if (camera->Capture()) {
                fb_promise.set_value(camera->GetFrameBuffer());
            } else {
                fb_promise.set_value(nullptr);
            }
        });

        const camera_fb_t* fb = fb_future.get();
        
        if (fb && fb->buf) {
            img.data = fb->buf;
            img.width = fb->width;    // <--- 补上这一行，动态获取真实宽度
            img.height = fb->height;  // <--- 补上这一行，动态获取真实高度

            // 预设默认的 UI 状态为“发呆”
            std::string target_emotion = "neutral"; 

            // --- 核心推理区：串级流水线 ---
            // 第一级：先找画面里有没有手
            auto detect_results = hand_detector.run(img);
            
            if (!detect_results.empty()) {
                // 第二级：有手，再去识别手势类别
                auto recognize_results = gesture_recognizer.recognize(img, detect_results);
                
                if (!recognize_results.empty()) {
                    // 获取手势类别名称
                    // result_t 结构体定义: { const char *cat_name; float score; }
                    // 没有 cat_id 成员，只能用 cat_name 字符串来判断
                    const char* gesture_name = recognize_results.front().cat_name;
                    
                    // --- 手势名称映射表情 ---
                    // 手势类别名参考 hand_gesture_category_name.hpp:
                    // "one","two","three","four","five","like","ok","no_gesture","call","dislike","no_hand"
                    if (strcmp(gesture_name, "like") == 0) {
                        target_emotion = "my_like";      // 识别到大拇指，立刻调用自定义的 UI 图片
                    } else if (strcmp(gesture_name, "ok") == 0) {
                        target_emotion = "happy";    // OK 手势
                    } else if (strcmp(gesture_name, "five") == 0) {
                        target_emotion = "crying";   // 手掌张开
                    } else if (strcmp(gesture_name, "call") == 0) {
                        target_emotion = "kissy";      // 打电话手势
                    } else if (strcmp(gesture_name, "one") == 0) {
                        target_emotion = "confused";      // 食指手势
                    }
                }
            }

            // --- 防抖与下发区 ---
            static std::string last_emotion = "";
            static bool is_first_run = true;

            if (target_emotion != last_emotion || is_first_run) {
                last_emotion = target_emotion;
                is_first_run = false;
                
                ESP_LOGI(TAG, "手势状态突变！准备切换 UI 至: %s", target_emotion.c_str());
                
                // 利用 Schedule 将 UI 刷新动作丢给主线程
                app.Schedule([target_emotion]() {
                    auto display = Board::GetInstance().GetDisplay();
                    if (display) {
                        display->SetEmotion(target_emotion.c_str());
                    }
                });
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150)); // 略微增加延时，因为两级模型计算量稍大
    }
    vTaskDelete(NULL);
}

AiVisionSystem::~AiVisionSystem() {
    Stop();
}

void AiVisionSystem::Stop() {
    is_running_ = false;
}