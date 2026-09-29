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

    // This implementation deliberately leaves AI emotion separate from the
    // LED. The LED reflects system/network/audio-processing state only.
    switch (state) {
        case kDeviceStateStarting:
            SetColor(0, kMaxBrightness, 0);
            StartContinuousBlink(700);       // startup / initializing
            break;

        case kDeviceStateWifiConfiguring:
            SetColor(kMaxBrightness, kMaxBrightness, 0);
            StartContinuousBlink(500);       // setup AP mode
            break;

        case kDeviceStateConnecting:
        case kDeviceStateActivating:
            SetColor(0, kMaxBrightness, 0);
            StartContinuousBlink(500);       // network / server connection
            break;

        case kDeviceStateIdle:
            // Ready: solid green, then dim after 10 seconds.
            SetColor(0, kMaxBrightness, 0);
            TurnOn();
            // The existing state callback has no persistent delayed-event API,
            // so use a one-shot timer to dim the ready LED after 10 seconds.
            esp_timer_stop(blink_timer_);
            esp_timer_start_once(blink_timer_, static_cast<uint64_t>(kReadyDimAfterMs) * 1000ULL);
            break;

        case kDeviceStateListening:
            if (app.IsVoiceDetected()) {
                // Keep blue while speech/VAD is active.
                SetColor(0, 0, kMaxBrightness);
            } else {
                SetColor(0, 0, static_cast<uint8_t>(kMaxBrightness / 2));
            }
            // A steady state refresh is sufficient; VAD events call OnStateChanged.
            TurnOn();
            break;

        case kDeviceStateSpeaking:
        case kDeviceStateNotifying:
            // White breathing while TTS is playing.
            SetColor(kMaxBrightness, kMaxBrightness, kMaxBrightness);
            StartContinuousBlink(250);
            break;

        case kDeviceStateUpgrading:
            SetColor(0, kMaxBrightness, 0);
            StartContinuousBlink(180);
            break;

        case kDeviceStateAudioTesting:
            SetColor(kMaxBrightness, 0, kMaxBrightness);
            StartContinuousBlink(300);
            break;

        case kDeviceStateFatalError:
            // Fatal / network failure indication.
            SetColor(kMaxBrightness, 0, 0);
            TurnOn();
            break;

        default:
            TurnOff();
            break;
    }
}
