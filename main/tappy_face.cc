#include "tappy_face.h"

#include "components/esp32-eyes/Face.h"
#include "components/esp32-eyes/FaceEmotions.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>

#include <cctype>
#include <cstring>

#define TAG "TappyFace"

class TappyFace::Impl {
public:
    Face* face = nullptr;
    SemaphoreHandle_t mutex = nullptr;
    TaskHandle_t task = nullptr;
    bool initialized = false;

    static eEmotions ParseEmotion(const char* value) {
        if (value == nullptr) return Normal;

        struct Entry {
            const char* name;
            eEmotions emotion;
        };

        static const Entry entries[] = {
            {"Normal", Normal},
            {"Angry", Angry},
            {"Glee", Glee},
            {"Happy", Happy},
            {"Sad", Sad},
            {"Worried", Worried},
            {"Focused", Focused},
            {"Annoyed", Annoyed},
            {"Surprised", Surprised},
            {"Skeptic", Skeptic},
            {"Frustrated", Frustrated},
            {"Unimpressed", Unimpressed},
            {"Sleepy", Sleepy},
            {"Suspicious", Suspicious},
            {"Squint", Squint},
            {"Furious", Furious},
            {"Scared", Scared},
            {"Awe", Awe},
        };

        for (const auto& entry : entries) {
            if (strcasecmp(value, entry.name) == 0) {
                return entry.emotion;
            }
        }

        return Normal;
    }
};

TappyFace& TappyFace::GetInstance() {
    static TappyFace instance;
    return instance;
}

void TappyFace::Initialize() {
    if (impl_ != nullptr && impl_->initialized) {
        return;
    }

    if (impl_ == nullptr) {
        impl_ = new Impl();
    }

    impl_->mutex = xSemaphoreCreateMutex();
    if (impl_->mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create face mutex");
        return;
    }

    impl_->face = new Face(128, 64, 40);
    if (impl_->face == nullptr) {
        ESP_LOGE(TAG, "Failed to create Face");
        vSemaphoreDelete(impl_->mutex);
        impl_->mutex = nullptr;
        return;
    }

    // AI controls the emotion. Random blinking and looking remain enabled.
    impl_->face->RandomBehavior = false;
    impl_->face->RandomLook = true;
    impl_->face->RandomBlink = true;
    impl_->face->Behavior.GoToEmotion(Normal);

    impl_->initialized = true;

    xTaskCreatePinnedToCore(
        &TappyFace::FaceTask,
        "tappy_face",
        4096,
        this,
        3,
        &impl_->task,
        1);

    ESP_LOGI(TAG, "TAPPY face initialized");
}

void TappyFace::SetEmotion(const char* emotion) {
    if (impl_ == nullptr || !impl_->initialized || impl_->face == nullptr) {
        return;
    }

    const eEmotions parsed = Impl::ParseEmotion(emotion);

    if (xSemaphoreTake(impl_->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        impl_->face->Behavior.GoToEmotion(parsed);
        xSemaphoreGive(impl_->mutex);
    }

    ESP_LOGI(TAG, "AI emotion: %s", emotion ? emotion : "Normal");
}

void TappyFace::FaceTask(void* arg) {
    auto* self = static_cast<TappyFace*>(arg);

    while (true) {
        self->Update();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void TappyFace::Update() {
    if (impl_ == nullptr || !impl_->initialized || impl_->face == nullptr) {
        return;
    }

    if (xSemaphoreTake(impl_->mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        impl_->face->Update();
        xSemaphoreGive(impl_->mutex);
    }
}
