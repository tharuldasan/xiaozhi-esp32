#ifndef TAPPY_ALARM_MANAGER_H
#define TAPPY_ALARM_MANAGER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct TappyAlarm {
    int32_t id = 0;
    int64_t timestamp = 0;
    std::string label;
};

class TappyAlarmManager {
public:
    using TriggerCallback = std::function<void(const TappyAlarm&)>;

    static TappyAlarmManager& GetInstance();
    void Initialize();
    void SetTriggerCallback(TriggerCallback callback);
    void Tick();

    std::string SetAlarm(int year, int month, int day, int hour, int minute,
                         const std::string& label = "");
    bool CancelAlarm(int32_t id);
    std::string ListAlarms() const;
    int Count() const;
    bool IsRinging() const { return ringing_; }
    void DismissRinging();

    bool IsRinging() const { return ringing_; }
    void DismissRinging();

private:
    TappyAlarmManager() = default;
    bool Save();
    bool Load();
    int32_t NextId();

    std::vector<TappyAlarm> alarms_;
    TriggerCallback trigger_callback_;
    int32_t next_id_ = 1;
    bool initialized_ = false;
    bool ringing_ = false;
    TappyAlarm ringing_alarm_;
    int ring_tick_ = 0;
};

#endif
