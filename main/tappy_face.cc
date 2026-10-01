#include "tappy_face.h"

#include "components/esp32-eyes/Face.h"
#include "components/esp32-eyes/FaceEmotions.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

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

    // TAPPY owns I2C0 for the SSD1306. Initialize the ESP-IDF I2C master
    // bus before U8g2 tries to reuse it. OLED wiring is fixed at SDA=15/SCL=7.
    i2c_master_bus_handle_t bus = nullptr;
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = GPIO_NUM_15;
    bus_config.scl_io_num = GPIO_NUM_7;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t i2c_err = i2c_new_master_bus(&bus_config, &bus);
    if (i2c_err != ESP_OK && i2c_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize OLED I2C bus: %s", esp_err_to_name(i2c_err));
        vSemaphoreDelete(impl_->mutex);
        impl_->mutex = nullptr;
        return;
    }

    if (i2c_err == ESP_OK) {
        ESP_LOGI(TAG, "OLED I2C0 initialized on SDA=15 SCL=7");
    } else {
        ESP_LOGI(TAG, "OLED I2C0 already initialized; reusing existing bus");
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
