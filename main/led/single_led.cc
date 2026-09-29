#include "single_led.h"
#include "application.h"
#include <esp_log.h>
#include <cmath>
#include <algorithm>

#define TAG "SingleLed"

namespace {
constexpr uint8_t kMaxBrightness = 20;
constexpr uint8_t kLowBrightness = 2;
constexpr int kBreathePeriodMs = 1800;
constexpr int kEffectTickMs = 35;
constexpr int kRgbCyclePeriodMs = 600;
constexpr int kReadyDimAfterMs = 10000;
constexpr int kReadyDimBrightness = 2;
constexpr int kReadyFullBrightness = 14;

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};
}

SingleLed::SingleLed(gpio_num_t gpio) {
    if (gpio == GPIO_NUM_NC) {
        ESP_LOGW(TAG, "RGB LED disabled");
        return;
    }

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio;
    strip_config.max_leds = 1;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    led_strip_clear(led_strip_);

    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            static_cast<SingleLed*>(arg)->OnBlinkTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "tappy_rgb",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &blink_timer_));
}

SingleLed::~SingleLed() {
    if (blink_timer_ != nullptr) {
        esp_timer_stop(blink_timer_);
        esp_timer_delete(blink_timer_);
        blink_timer_ = nullptr;
    }
    if (led_strip_ != nullptr) {
        led_strip_del(led_strip_);
        led_strip_ = nullptr;
    }
}

void SingleLed::SetColor(uint8_t r, uint8_t g, uint8_t b) {
    std::lock_guard<std::mutex> lock(mutex_);
    r_ = r;
    g_ = g;
    b_ = b;
}

void SingleLed::TurnOn() {
    if (!led_strip_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(blink_timer_);
    led_strip_set_pixel(led_strip_, 0, r_, g_, b_);
    led_strip_refresh(led_strip_);
}

void SingleLed::TurnOff() {
    if (!led_strip_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(blink_timer_);
    led_strip_clear(led_strip_);
}

void SingleLed::BlinkOnce() {
    Blink(1, 100);
}

void SingleLed::Blink(int times, int interval_ms) {
    StartBlinkTask(times, interval_ms);
}

void SingleLed::StartContinuousBlink(int interval_ms) {
    StartBlinkTask(BLINK_INFINITE, interval_ms);
}

void SingleLed::StartBlinkTask(int times, int interval_ms) {
    if (!led_strip_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(blink_timer_);
    blink_counter_ = times == BLINK_INFINITE ? BLINK_INFINITE : times * 2;
    blink_interval_ms_ = std::max(10, interval_ms);
    esp_timer_start_periodic(blink_timer_, static_cast<uint64_t>(blink_interval_ms_) * 1000ULL);
}

void SingleLed::OnBlinkTimer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!led_strip_) return;

    // The public class is still used by XiaoZhi for simple LED state changes,
    // but TAPPY's explicit effect modes are handled here.
    if (blink_counter_ == BLINK_INFINITE) {
        // Default fallback heartbeat.
        if (r_ == 0 && g_ == 0 && b_ == 0) {
            led_strip_clear(led_strip_);
            return;
        }
        static bool phase = false;
        phase = !phase;
        if (phase) {
            led_strip_set_pixel(led_strip_, 0, r_, g_, b_);
            led_strip_refresh(led_strip_);
        } else {
            led_strip_clear(led_strip_);
        }
        return;
    }

    blink_counter_--;
    if (blink_counter_ & 1) {
        led_strip_set_pixel(led_strip_, 0, r_, g_, b_);
        led_strip_refresh(led_strip_);
    } else {
        led_strip_clear(led_strip_);
        if (blink_counter_ <= 0) {
            esp_timer_stop(blink_timer_);
        }
    }
}

void SingleLed::OnStateChanged() {
    auto& app = Application::GetInstance();
    const DeviceState state = app.GetDeviceState();

    // System status LED only. AI emotion is handled independently by TappyFace.
    switch (state) {
        case kDeviceStateStarting:
            // Green breathing = boot/network initialization.
            SetColor(0, kMaxBrightness, 0);
            StartContinuousBlink(700);
            break;

        case kDeviceStateWifiConfiguring:
            // Yellow breathing = Wi-Fi setup/configuration mode.
            SetColor(kMaxBrightness, kMaxBrightness, 0);
            StartContinuousBlink(500);
            break;

        case kDeviceStateConnecting:
        case kDeviceStateActivating:
            // Green breathing = connecting to Wi-Fi/server.
            SetColor(0, kMaxBrightness, 0);
            StartContinuousBlink(500);
            break;

        case kDeviceStateIdle:
            // Ready = green solid, then dim after 10 seconds.
            SetColor(0, kReadyFullBrightness, 0);
            TurnOn();
            // This timer is intentionally one-shot; the timer callback below
            // performs the ready-state dimming rather than blinking it.
            esp_timer_stop(blink_timer_);
            esp_timer_start_once(
                blink_timer_,
                static_cast<uint64_t>(kReadyDimAfterMs) * 1000ULL);
            break;

        case kDeviceStateListening:
            // Listening = blue. VAD refreshes the same blue state.
            SetColor(0, 0, kMaxBrightness);
            StartContinuousBlink(450);
            break;

        case kDeviceStateSpeaking:
        case kDeviceStateNotifying:
            // TTS = white breathing.
            SetColor(kMaxBrightness, kMaxBrightness, kMaxBrightness);
            StartContinuousBlink(250);
            break;

        case kDeviceStateUpgrading:
            // Fast green heartbeat during firmware update.
            SetColor(0, kMaxBrightness, 0);
            StartContinuousBlink(180);
            break;

        case kDeviceStateAudioTesting:
            // Diagnostic mode.
            SetColor(kMaxBrightness, 0, kMaxBrightness);
            StartContinuousBlink(300);
            break;

        case kDeviceStateFatalError:
            // Error / lost connection fallback.
            SetColor(kMaxBrightness, 0, 0);
            TurnOn();
            break;

        default:
            TurnOff();
            break;
    }
}
