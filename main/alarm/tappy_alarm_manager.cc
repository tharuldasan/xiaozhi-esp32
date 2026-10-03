#include "tappy_alarm_manager.h"

#include "settings.h"
#include "tappy_time_manager.h"

#include <algorithm>
#include <ctime>
#include <esp_log.h>
#include <utility>
#include <cJSON.h>

#define TAG "TappyAlarm"

namespace {
constexpr size_t kMaxAlarms = 16;
constexpr int kMinYear = 2024;
constexpr int kMaxYear = 2099;

bool MakeTimestamp(int year, int month, int day, int hour, int minute, int64_t& timestamp) {
    if (year < kMinYear || year > kMaxYear ||
        month < 1 || month > 12 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59) {
        return false;
    }

    struct tm value{};
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = 0;
    value.tm_isdst = -1;

    time_t result = mktime(&value);
    if (result == static_cast<time_t>(-1)) {
        return false;
    }

    struct tm normalized{};
    localtime_r(&result, &normalized);
    if (normalized.tm_year != year - 1900 ||
        normalized.tm_mon != month - 1 ||
        normalized.tm_mday != day ||
        normalized.tm_hour != hour ||
        normalized.tm_min != minute) {
        return false;
    }

    timestamp = static_cast<int64_t>(result);
    return true;
}

std::string FormatAlarm(const TappyAlarm& alarm) {
    time_t value = static_cast<time_t>(alarm.timestamp);
    struct tm local{};
    localtime_r(&value, &local);

    char buffer[96];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %I:%M %p", &local);

    std::string result = "#";
    result += std::to_string(alarm.id);
    result += " ";
    result += buffer;
    if (!alarm.label.empty()) {
        result += " (";
        result += alarm.label;
        result += ")";
    }
    return result;
}
}  // namespace

TappyAlarmManager& TappyAlarmManager::GetInstance() {
    static TappyAlarmManager instance;
    return instance;
}

void TappyAlarmManager::Initialize() {
    if (initialized_) {
        return;
    }
    Load();
    initialized_ = true;
    ESP_LOGI(TAG, "Loaded %d persistent alarms", Count());
}

void TappyAlarmManager::SetTriggerCallback(TriggerCallback callback) {
    trigger_callback_ = std::move(callback);
}

int32_t TappyAlarmManager::NextId() {
    if (next_id_ <= 0) {
        next_id_ = 1;
    }
    int32_t id = next_id_++;
    if (next_id_ <= 0) {
        next_id_ = 1;
    }
    return id;
}

bool TappyAlarmManager::Load() {
    Settings settings("tappy_alarm", true);
    next_id_ = settings.GetInt("next_id", 1);
    if (next_id_ <= 0) {
        next_id_ = 1;
    }

    const std::string data = settings.GetString("alarms", "[]");
    cJSON* root = cJSON_Parse(data.c_str());
    if (root == nullptr || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        alarms_.clear();
        Save();
        return false;
    }

    alarms_.clear();
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, root) {
        cJSON* id = cJSON_GetObjectItem(item, "id");
        cJSON* timestamp = cJSON_GetObjectItem(item, "timestamp");
        cJSON* label = cJSON_GetObjectItem(item, "label");
        if (!cJSON_IsNumber(id) || !cJSON_IsNumber(timestamp)) {
            continue;
        }

        TappyAlarm alarm;
        alarm.id = id->valueint;
        alarm.timestamp = static_cast<int64_t>(timestamp->valuedouble);
        if (cJSON_IsString(label)) {
            alarm.label = label->valuestring;
        }

        if (alarm.id > 0 && alarm.timestamp > 0 && alarms_.size() < kMaxAlarms) {
            alarms_.push_back(std::move(alarm));
        }
    }

    cJSON_Delete(root);
    return true;
}

bool TappyAlarmManager::Save() {
    cJSON* root = cJSON_CreateArray();
    if (root == nullptr) {
        return false;
    }

    for (const auto& alarm : alarms_) {
        cJSON* item = cJSON_CreateObject();
        if (item == nullptr) {
            cJSON_Delete(root);
            return false;
        }
        cJSON_AddNumberToObject(item, "id", alarm.id);
        cJSON_AddNumberToObject(item, "timestamp", static_cast<double>(alarm.timestamp));
        cJSON_AddStringToObject(item, "label", alarm.label.c_str());
        cJSON_AddItemToArray(root, item);
    }

    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (printed == nullptr) {
        return false;
    }

    Settings settings("tappy_alarm", true);
    settings.SetString("alarms", printed);
    settings.SetInt("next_id", next_id_);
    cJSON_free(printed);
    return true;
}

std::string TappyAlarmManager::SetAlarm(int year, int month, int day, int hour, int minute,
                                        const std::string& label) {
    if (!TappyTimeManager::GetInstance().IsValid()) {
        return "ERROR: internet time is not synchronized yet";
    }

    int64_t timestamp = 0;
    if (!MakeTimestamp(year, month, day, hour, minute, timestamp)) {
        return "ERROR: invalid date or time";
    }

    const int64_t now = static_cast<int64_t>(TappyTimeManager::GetInstance().Now());
    if (timestamp <= now) {
        return "ERROR: alarm time is already in the past";
    }

    if (alarms_.size() >= kMaxAlarms) {
        return "ERROR: maximum of 16 alarms is reached";
    }

    TappyAlarm alarm;
    alarm.id = NextId();
    alarm.timestamp = timestamp;
    alarm.label = label;
    alarms_.push_back(alarm);

    if (!Save()) {
        alarms_.pop_back();
        return "ERROR: failed to save alarm to flash";
    }

    return "Alarm saved: " + FormatAlarm(alarm);
}

bool TappyAlarmManager::CancelAlarm(int32_t id) {
    auto it = std::find_if(alarms_.begin(), alarms_.end(),
                           [id](const TappyAlarm& alarm) { return alarm.id == id; });
    if (it == alarms_.end()) {
        return false;
    }

    alarms_.erase(it);
    Save();
    return true;
}

std::string TappyAlarmManager::ListAlarms() const {
    if (alarms_.empty()) {
        return "No alarms are saved.";
    }

    std::string result;
    for (size_t i = 0; i < alarms_.size(); ++i) {
        if (i != 0) {
            result += "\n";
        }
        result += FormatAlarm(alarms_[i]);
    }
    return result;
}

int TappyAlarmManager::Count() const {
    return static_cast<int>(alarms_.size());
}

void TappyAlarmManager::Tick() {
    if (!initialized_ || !TappyTimeManager::GetInstance().IsValid() || alarms_.empty()) {
        return;
    }

    const int64_t now = static_cast<int64_t>(TappyTimeManager::GetInstance().Now());
    bool changed = false;

    for (auto it = alarms_.begin(); it != alarms_.end();) {
        if (it->timestamp <= now) {
            TappyAlarm alarm = *it;
            it = alarms_.erase(it);
            changed = true;

            // Persist this deletion before ringing it.
            Save();

            ringing_ = true;
            ringing_alarm_ = alarm;
            ring_tick_ = 0;
            if (trigger_callback_) {
                trigger_callback_(alarm);
            }

            ESP_LOGI(TAG, "Alarm #%ld fired: %s", static_cast<long>(alarm.id),
                     FormatAlarm(alarm).c_str());

        } else {
            ++it;
        }
    }

    if (changed) {
        // Persist removals before playing the alarm so a reboot during the alert
        // cannot resurrect an already-fired one-shot alarm.
        Save();
        // Trigger after persistence; the alarm is already gone from the in-memory list.
        // Fired alarms are intentionally one-shot.
    }
}

void TappyAlarmManager::DismissRinging() {
    if (!ringing_) return;
    ringing_ = false;
    ring_tick_ = 0;
    ringing_alarm_ = TappyAlarm{};
    ESP_LOGI(TAG, "Alarm dismissed by BOOT button");
}
