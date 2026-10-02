#include "tappy_weather_service.h"

#include "board.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_log.h>

#define TAG "TappyWeather"

namespace {
constexpr const char* kDefaultLocation = "Colombo, Sri Lanka";
constexpr double kDefaultLatitude = 6.927079;
constexpr double kDefaultLongitude = 79.861244;
}

TappyWeatherService& TappyWeatherService::GetInstance() {
    static TappyWeatherService instance;
    return instance;
}

void TappyWeatherService::Initialize() {
    Settings settings("tappy_weather", true);
    if (settings.GetString("location").empty()) {
        settings.SetString("location", kDefaultLocation);
        settings.SetString("latitude", std::to_string(kDefaultLatitude));
        settings.SetString("longitude", std::to_string(kDefaultLongitude));
    }
}

std::string TappyWeatherService::GetLocation() const {
    Settings settings("tappy_weather", false);
    return settings.GetString("location", kDefaultLocation);
}

std::string TappyWeatherService::UrlEncode(const std::string& value) const {
    static const char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            result += static_cast<char>(c);
        } else if (c == ' ') {
            result += '+';
        } else {
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 0x0F];
        }
    }
    return result;
}

std::string TappyWeatherService::HttpGet(const std::string& url) const {
    auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
    http->SetHeader("Accept", "application/json");

    auto opened = http->Open("GET", url);
    if (!opened) {
        ESP_LOGE(TAG, "HTTP open failed: %s", opened.error().ToString().c_str());
        return {};
    }

    auto status = http->GetStatusCode();
    if (!status || *status != 200) {
        ESP_LOGE(TAG, "HTTP status failed for %s", url.c_str());
        http->Close();
        return {};
    }

    std::string body = http->ReadAll();
    http->Close();
    return body;
}

std::string TappyWeatherService::SetLocation(const std::string& location) {
    if (location.empty()) {
        return "ERROR: location is empty";
    }

    std::string url = "https://geocoding-api.open-meteo.com/v1/search?name=" +
                      UrlEncode(location) + "&count=1&language=en&format=json";

    std::string body = HttpGet(url);
    if (body.empty()) {
        return "ERROR: Open-Meteo geocoding request failed";
    }

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        return "ERROR: invalid Open-Meteo geocoding response";
    }

    cJSON* results = cJSON_GetObjectItem(root, "results");
    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        cJSON_Delete(root);
        return "ERROR: location not found";
    }

    cJSON* first = cJSON_GetArrayItem(results, 0);
    cJSON* name = cJSON_GetObjectItem(first, "name");
    cJSON* country = cJSON_GetObjectItem(first, "country");
    cJSON* latitude = cJSON_GetObjectItem(first, "latitude");
    cJSON* longitude = cJSON_GetObjectItem(first, "longitude");

    if (!cJSON_IsNumber(latitude) || !cJSON_IsNumber(longitude)) {
        cJSON_Delete(root);
        return "ERROR: geocoding response has no coordinates";
    }

    std::string display_name = cJSON_IsString(name) ? name->valuestring : location;
    if (cJSON_IsString(country)) {
        display_name += ", ";
        display_name += country->valuestring;
    }

    Settings settings("tappy_weather", true);
    settings.SetString("location", display_name);
    settings.SetString("latitude", std::to_string(latitude->valuedouble));
    settings.SetString("longitude", std::to_string(longitude->valuedouble));

    cJSON_Delete(root);
    return "Weather location saved as " + display_name;
}

std::string TappyWeatherService::WeatherDescription(int code) const {
    switch (code) {
        case 0: return "clear sky";
        case 1: return "mainly clear";
        case 2: return "partly cloudy";
        case 3: return "overcast";
        case 45:
        case 48: return "fog";
        case 51:
        case 53:
        case 55: return "drizzle";
        case 56:
        case 57: return "freezing drizzle";
        case 61:
        case 63:
        case 65: return "rain";
        case 66:
        case 67: return "freezing rain";
        case 71:
        case 73:
        case 75: return "snow";
        case 77: return "snow grains";
        case 80:
        case 81:
        case 82: return "rain showers";
        case 85:
        case 86: return "snow showers";
        case 95: return "thunderstorm";
        case 96:
        case 99: return "thunderstorm with hail";
        default: return "unknown conditions";
    }
}

std::string TappyWeatherService::GetCurrent(const std::string& location) {
    if (!location.empty()) {
        std::string saved = GetLocation();
        if (saved != location) {
            std::string set_result = SetLocation(location);
            if (set_result.rfind("ERROR:", 0) == 0) {
                return set_result;
            }
        }
    }

    Settings settings("tappy_weather", false);
    double latitude = std::stod(settings.GetString("latitude", std::to_string(kDefaultLatitude)));
    double longitude = std::stod(settings.GetString("longitude", std::to_string(kDefaultLongitude)));
    std::string saved_location = settings.GetString("location", kDefaultLocation);

    std::string url = "https://api.open-meteo.com/v1/forecast?latitude=" +
                      std::to_string(latitude) + "&longitude=" + std::to_string(longitude) +
                      "&current=temperature_2m,relative_humidity_2m,precipitation,rain,showers,"
                      "weather_code,wind_speed_10m&temperature_unit=celsius&wind_speed_unit=kmh"
                      "&timezone=auto";

    std::string body = HttpGet(url);
    if (body.empty()) {
        return "ERROR: Open-Meteo weather request failed";
    }

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        return "ERROR: invalid Open-Meteo weather response";
    }

    cJSON* current = cJSON_GetObjectItem(root, "current");
    cJSON* temperature = current ? cJSON_GetObjectItem(current, "temperature_2m") : nullptr;
    cJSON* humidity = current ? cJSON_GetObjectItem(current, "relative_humidity_2m") : nullptr;
    cJSON* precipitation = current ? cJSON_GetObjectItem(current, "precipitation") : nullptr;
    cJSON* rain = current ? cJSON_GetObjectItem(current, "rain") : nullptr;
    cJSON* showers = current ? cJSON_GetObjectItem(current, "showers") : nullptr;
    cJSON* code = current ? cJSON_GetObjectItem(current, "weather_code") : nullptr;
    cJSON* wind = current ? cJSON_GetObjectItem(current, "wind_speed_10m") : nullptr;

    if (!cJSON_IsNumber(temperature) || !cJSON_IsNumber(code)) {
        cJSON_Delete(root);
        return "ERROR: weather response is missing current conditions";
    }

    char buffer[512];
    snprintf(buffer, sizeof(buffer),
             "Weather in %s: %.1f C, %s, humidity %.0f%%, wind %.1f km/h, "
             "precipitation %.1f mm, rain %.1f mm, showers %.1f mm.",
             saved_location.c_str(),
             temperature->valuedouble,
             WeatherDescription(code->valueint).c_str(),
             cJSON_IsNumber(humidity) ? humidity->valuedouble : 0.0,
             cJSON_IsNumber(wind) ? wind->valuedouble : 0.0,
             cJSON_IsNumber(precipitation) ? precipitation->valuedouble : 0.0,
             cJSON_IsNumber(rain) ? rain->valuedouble : 0.0,
             cJSON_IsNumber(showers) ? showers->valuedouble : 0.0);

    cJSON_Delete(root);
    return buffer;
}
