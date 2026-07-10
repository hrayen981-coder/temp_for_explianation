#ifndef CAMERA_H
#define CAMERA_H

#include <string>

#include "esp_camera.h"

class Camera {
public:
    virtual void SetExplainUrl(const std::string& url, const std::string& token) = 0;
    virtual bool Capture() = 0;
    virtual bool SetHMirror(bool enabled) = 0;
    virtual bool SetVFlip(bool enabled) = 0;
    virtual bool SetSwapBytes(bool enabled) { return false; }  // Optional, default no-op
    virtual std::string Explain(const std::string& question) = 0;
    
    // 新增：获取图像帧缓冲区的虚接口
    virtual const camera_fb_t* GetFrameBuffer() const { return nullptr; }
};

#endif // CAMERA_H