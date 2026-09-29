#ifndef _SINGLE_LED_H_
#define _SINGLE_LED_H_

#include "led.h"

#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <led_strip.h>

#include <cstdint>
#include <mutex>

class SingleLed : public Led {
public:
    explicit SingleLed(gpio_num_t gpio);
    ~SingleLed() override;

    void OnStateChanged() override;
    void OnWifiLost() override;
    void OnServerLost() override;
    void OnProcessing(bool active) override;

private:
    enum class Effect {
        Off,
        Solid,
        Breathe,
        RgbCycle,
    };

    std::mutex mutex_;
    led_strip_handle_t led_strip_ = nullptr;
    esp_timer_handle_t effect_timer_ = nullptr;

    uint8_t base_r_ = 0;
    uint8_t base_g_ = 0;
    uint8_t base_b_ = 0;
    uint8_t solid_brightness_ = 0;

    Effect effect_ = Effect::Off;
    uint32_t effect_period_ms_ = 0;
    int64_t effect_started_us_ = 0;

    bool ready_dim_pending_ = false;
    bool override_active_ = false;

    void StartEffectLocked(Effect effect, uint32_t period_ms, bool immediate);
    void StopTimerLocked();
    void RenderLocked(uint8_t r, uint8_t g, uint8_t b);
    void RenderScaledLocked(uint8_t r, uint8_t g, uint8_t b, float scale);
    void OnEffectTimer();

    void SetSolidLocked(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
                        bool dim_after_ready);
    void SetBreatheLocked(uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms);
    void SetRgbCycleLocked(uint32_t period_ms);
    void SetOffLocked();
    void SetFaultLocked(uint8_t r, uint8_t g, uint8_t b);
};

#endif // _SINGLE_LED_H_
