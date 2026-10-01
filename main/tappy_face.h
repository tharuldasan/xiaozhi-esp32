#pragma once

#include <string>

class TappyFace {
public:
    static TappyFace& GetInstance();

    void Initialize();
    bool SetEmotion(const char* emotion);
    static bool IsValidEmotion(const char* emotion);

private:
    TappyFace() = default;
    ~TappyFace() = default;
    TappyFace(const TappyFace&) = delete;
    TappyFace& operator=(const TappyFace&) = delete;

    static void FaceTask(void* arg);
    void Update();

    class Impl;
    Impl* impl_ = nullptr;
};
