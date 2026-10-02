#include "tappy_time_manager.h"

#include <cstdlib>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <cJSON.h>
#include "board.h"
#include "settings.h"

#define TAG "TappyTime"

namespace {
std::string HttpGet(const std::string& url) {
    auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
    http->SetHeader("Accept", "application/json");
    auto opened = http->Open("GET", url);
    if (!opened) return {};
    auto status = http->GetStatusCode();
    if (!status || *status != 200) {
        http->Close();
        return {};
    }
    std::string body = http->ReadAll();
    http->Close();
    return body;
}
}

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

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        2, ESP_SNTP_SERVER_LIST("pool.ntp.org", "time.google.com"));
    config.start = false;

    // Detect the public-IP location first. IP geolocation is approximate, but it gives
    // TAPPY the network's city/timezone instead of assuming Colombo.
    DetectLocation();

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        initialized_ = true;
        ESP_LOGI(TAG, "NTP initialized; timezone UTC+05:30");
    } else {
        ESP_LOGE(TAG, "Failed to initialize NTP: %s", esp_err_to_name(err));
    }
}

bool TappyTimeManager::DetectLocation() {
    std::string body = HttpGet("https://ipapi.co/json/");
    if (body.empty()) {
        ESP_LOGW(TAG, "IP geolocation request failed");
        return false;
    }

    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return false;

    cJSON* city = cJSON_GetObjectItem(root, "city");
    cJSON* region = cJSON_GetObjectItem(root, "region");
    cJSON* country = cJSON_GetObjectItem(root, "country_name");
    cJSON* timezone = cJSON_GetObjectItem(root, "timezone");
    cJSON* latitude = cJSON_GetObjectItem(root, "latitude");
    cJSON* longitude = cJSON_GetObjectItem(root, "longitude");

    if (!cJSON_IsString(timezone) || !cJSON_IsNumber(latitude) || !cJSON_IsNumber(longitude)) {
        cJSON_Delete(root);
        return false;
    }

    std::string location;
    if (cJSON_IsString(city)) location = city->valuestring;
    if (cJSON_IsString(region) && !region->valuestring[0] == '\0') {
        if (!location.empty()) location += ", ";
        location += region->valuestring;
    }
    if (cJSON_IsString(country)) {
        if (!location.empty()) location += ", ";
        location += country->valuestring;
    }

    Settings settings("tappy_time", true);
    settings.SetString("location", location);
    settings.SetString("timezone", timezone->valuestring);
    settings.SetString("latitude", std::to_string(latitude->valuedouble));
    settings.SetString("longitude", std::to_string(longitude->valuedouble));

    setenv("TZ", timezone->valuestring, 1);
    tzset();

    ESP_LOGI(TAG, "IP location: %s | timezone: %s | %.5f, %.5f",
             location.c_str(), timezone->valuestring,
             latitude->valuedouble, longitude->valuedouble);

    cJSON_Delete(root);
    return true;
}

std::string TappyTimeManager::GetLocation() const {
    Settings settings("tappy_time", false);
    return settings.GetString("location", "Unknown location");
}

double TappyTimeManager::GetLatitude() const {
    Settings settings("tappy_time", false);
    return std::stod(settings.GetString("latitude", "6.927079"));
}

double TappyTimeManager::GetLongitude() const {
    Settings settings("tappy_time", false);
    return std::stod(settings.GetString("longitude", "79.861244"));
}

void TappyTimeManager::StartSync() {
    if (!initialized_) {
        Initialize();
        return;
    }

    // Re-resolve IP location after the network is fully connected.
    DetectLocation();

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
