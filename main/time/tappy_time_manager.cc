#include "tappy_time_manager.h"

#include <cstdlib>
#include <esp_log.h>
#include <esp_netif_sntp.h>

#define TAG "TappyTime"

TappyTimeManager& TappyTimeManager::GetInstance() {
    static TappyTimeManager instance;
    return instance;
}

void TappyTimeManager::Initialize() {
    if (initialized_) {
        return;
    }

    // Sri Lanka Standard Time: UTC+05:30, no daylight-saving changes.
    setenv("TZ", "IST-5:30", 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.start = false;

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        initialized_ = true;
        ESP_LOGI(TAG, "NTP initialized; timezone UTC+05:30");
    } else {
        ESP_LOGE(TAG, "Failed to initialize NTP: %s", esp_err_to_name(err));
    }
}

void TappyTimeManager::StartSync() {
    if (!initialized_) {
        Initialize();
        return;
    }

    esp_err_t err = esp_netif_sntp_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "NTP start failed: %s", esp_err_to_name(err));
    }
}

bool TappyTimeManager::IsValid() const {
    return Now() >= 1704067200; // 2024-01-01 UTC
}

time_t TappyTimeManager::Now() const {
    return time(nullptr);
}

bool TappyTimeManager::GetLocalTime(struct tm& out) const {
    if (!IsValid()) {
        return false;
    }

    time_t now = Now();
    localtime_r(&now, &out);
    return true;
}

std::string TappyTimeManager::GetLocalTimeString() const {
    struct tm local{};
    if (!GetLocalTime(local)) {
        return "time not synchronized";
    }

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%I:%M:%S %p", &local);
    return buffer;
}

std::string TappyTimeManager::GetLocalDateString() const {
    struct tm local{};
    if (!GetLocalTime(local)) {
        return "date not synchronized";
    }

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local);
    return buffer;
}
