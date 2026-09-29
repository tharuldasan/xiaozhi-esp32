#include "single_led.h"

#include "application.h"

#include <esp_log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#define TAG "SingleLed"

namespace {
constexpr uint8_t kMaxBrightness = 20;
constexpr uint8_t kReadyFullBrightness = 14;
constexpr uint8_t kReadyDimBrightness = 2;
constexpr uint8_t kFaultBrightness = 12;
constexpr uint8_t kProcessingBrightness = 10;

constexpr uint32_t kBreathePeriodMs = 1800;
constexpr uint32_t kProcessingCycleMs = 600;
constexpr uint32_t kEffectTickMs = 35;
constexpr uint32_t kReadyDimAfterMs = 10000;
constexpr float kTwoPi = 6.28318530717958647692f;

inline uint8_t ScaleChannel(uint8_t channel, float scale) {
    const float value = static_cast<float>(channel) * std::clamp(scale, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(value));
}

void HsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    h = std::fmod(h, 1.0f);
    if (h < 0.0f) h += 1.0f;

    const float i = std::floor(h * 6.0f);
    const float f = h * 6.0f - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);

    switch (static_cast<int>(i) % 6) {
        case 0: r = std::lround(v * 255); g = std::lround(t * 255); b = std::lround(p * 255); break;
        case 1: r = std::lround(q * 255); g = std::lround(v * 255); b = std::lround(p * 255); break;
        case 2: r = std::lround(p * 255); g = std::lround(v * 255); b = std::lround(t * 255); break;
        case 3: r = std::lround(p * 255); g = std::lround(q * 255); b = std::lround(v * 255); break;
        case 4: r = std::lround(t * 255); g = std::lround(p * 255); b = std::lround(v * 255); break;
        default: r = std::lround(v * 255); g = std::lround(p * 255); b = std::lround(q * 255); break;
    }
}
}  // namespace

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

    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) { static_cast<SingleLed*>(arg)->OnEffectTimer(); },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "tappy_rgb",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &effect_timer_));

    RenderLocked(0, 0, 0);
}

SingleLed::~SingleLed() {
    std::lock_guard<std::mutex> lock(mutex_);
    StopTimerLocked();
    if (effect_timer_ != nullptr) {
        esp_timer_delete(effect_timer_);
        effect_timer_ = nullptr;
    }
    if (led_strip_ != nullptr) {
        led_strip_del(led_strip_);
        led_strip_ = nullptr;
    }
}

void SingleLed::StopTimerLocked() {
    if (effect_timer_ != nullptr) {
        esp_timer_stop(effect_timer_);
    }
}

void SingleLed::RenderLocked(uint8_t r, uint8_t g, uint8_t b) {
    if (led_strip_ == nullptr) return;
    led_strip_set_pixel(led_strip_, 0, r, g, b);
    led_strip_refresh(led_strip_);
}

void SingleLed::RenderScaledLocked(uint8_t r, uint8_t g, uint8_t b, float scale) {
    RenderLocked(ScaleChannel(r, scale), ScaleChannel(g, scale), ScaleChannel(b, scale));
}

void SingleLed::StartEffectLocked(Effect effect, uint32_t period_ms, bool immediate) {
    effect_ = effect;
    effect_period_ms_ = std::max<uint32_t>(period_ms, kEffectTickMs);
    effect_started_us_ = esp_timer_get_time();
    ready_dim_pending_ = false;
    StopTimerLocked();

    if (effect_ == Effect::Off) {
        RenderLocked(0, 0, 0);
        return;
    }

    if (effect_ == Effect::Solid) {
        if (immediate) {
            RenderScaledLocked(base_r_, base_g_, base_b_,
                               static_cast<float>(solid_brightness_) / 255.0f);
        }
        return;
    }

    RenderLocked(0, 0, 0);
    esp_timer_start_periodic(effect_timer_, static_cast<uint64_t>(kEffectTickMs) * 1000ULL);
}

void SingleLed::SetSolidLocked(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
                               bool dim_after_ready) {
    base_r_ = r;
    base_g_ = g;
    base_b_ = b;
    solid_brightness_ = brightness;
    ready_dim_pending_ = dim_after_ready;
    StartEffectLocked(Effect::Solid, kReadyDimAfterMs, true);
    if (dim_after_ready) {
        esp_timer_start_once(effect_timer_, static_cast<uint64_t>(kReadyDimAfterMs) * 1000ULL);
    }
}

void SingleLed::SetBreatheLocked(uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms) {
    base_r_ = r;
    base_g_ = g;
    base_b_ = b;
    solid_brightness_ = kMaxBrightness;
    StartEffectLocked(Effect::Breathe, period_ms, false);
}

void SingleLed::SetRgbCycleLocked(uint32_t period_ms) {
    base_r_ = base_g_ = base_b_ = 0;
    solid_brightness_ = kProcessingBrightness;
    StartEffectLocked(Effect::RgbCycle, period_ms, false);
}

void SingleLed::SetOffLocked() {
    ready_dim_pending_ = false;
    StopTimerLocked();
    effect_ = Effect::Off;
    RenderLocked(0, 0, 0);
}

void SingleLed::SetFaultLocked(uint8_t r, uint8_t g, uint8_t b) {
    ready_dim_pending_ = false;
    StopTimerLocked();
    effect_ = Effect::Solid;
    base_r_ = r;
    base_g_ = g;
    base_b_ = b;
    solid_brightness_ = kFaultBrightness;
    RenderScaledLocked(r, g, b, static_cast<float>(solid_brightness_) / 255.0f);
}

void SingleLed::OnEffectTimer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (led_strip_ == nullptr) return;

    const int64_t now_us = esp_timer_get_time();

    if (ready_dim_pending_) {
        ready_dim_pending_ = false;
        if (Application::GetInstance().GetDeviceState() == kDeviceStateIdle && !override_active_) {
            solid_brightness_ = kReadyDimBrightness;
            effect_ = Effect::Solid;
            RenderScaledLocked(base_r_, base_g_, base_b_,
                               static_cast<float>(solid_brightness_) / 255.0f);
        }
        StopTimerLocked();
        return;
    }

    if (effect_ == Effect::Breathe) {
        const float elapsed_ms = static_cast<float>(now_us - effect_started_us_) / 1000.0f;
        const float phase = std::fmod(elapsed_ms, static_cast<float>(effect_period_ms_)) /
                            static_cast<float>(effect_period_ms_);
        const float wave = 0.5f - 0.5f * std::cos(kTwoPi * phase);
        const float normalized = 0.10f + 0.90f * wave;
        const float scale = normalized * (static_cast<float>(kMaxBrightness) / 255.0f);
        RenderScaledLocked(base_r_, base_g_, base_b_, scale);
        return;
    }

    if (effect_ == Effect::RgbCycle) {
        const float elapsed_ms = static_cast<float>(now_us - effect_started_us_) / 1000.0f;
        const float hue = std::fmod(elapsed_ms / static_cast<float>(effect_period_ms_), 1.0f);
        uint8_t r = 0, g = 0, b = 0;
        HsvToRgb(hue, 1.0f, 1.0f, r, g, b);
        RenderScaledLocked(r, g, b, static_cast<float>(kProcessingBrightness) / 255.0f);
    }
}

void SingleLed::OnWifiLost() {
    std::lock_guard<std::mutex> lock(mutex_);
    override_active_ = true;
    SetFaultLocked(0xFF, 0x00, 0x00);  // logical #FF0000
}

void SingleLed::OnServerLost() {
    std::lock_guard<std::mutex> lock(mutex_);
    override_active_ = true;
    SetFaultLocked(0xFF, 0xFF, 0x00);  // logical #FFFF00
}

void SingleLed::OnProcessing(bool active) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active) {
        override_active_ = false;
        return;
    }
    override_active_ = false;
    SetRgbCycleLocked(kProcessingCycleMs);
}

void SingleLed::OnStateChanged() {
    const DeviceState state = Application::GetInstance().GetDeviceState();

    std::lock_guard<std::mutex> lock(mutex_);
    override_active_ = false;

    switch (state) {
        case kDeviceStateStarting:
        case kDeviceStateConnecting:
        case kDeviceStateActivating:
            SetBreatheLocked(0x00, 0xFF, 0x00, kBreathePeriodMs);
            break;
        case kDeviceStateWifiConfiguring:
            SetBreatheLocked(0xFF, 0xFF, 0x00, 2000);
            break;
        case kDeviceStateIdle:
            SetSolidLocked(0x00, 0xFF, 0x00, kReadyFullBrightness, true);
            break;
        case kDeviceStateListening:
            SetBreatheLocked(0x00, 0x00, 0xFF, kBreathePeriodMs);
            break;
        case kDeviceStateSpeaking:
        case kDeviceStateNotifying:
            SetBreatheLocked(0xFF, 0xFF, 0xFF, kBreathePeriodMs);
            break;
        case kDeviceStateUpgrading:
            SetBreatheLocked(0x00, 0xFF, 0x00, 900);
            break;
        case kDeviceStateAudioTesting:
            SetBreatheLocked(0xFF, 0x00, 0xFF, 1200);
            break;
        case kDeviceStateFatalError:
            SetFaultLocked(0xFF, 0x00, 0x00);
            break;
        default:
            SetOffLocked();
            break;
    }
}
