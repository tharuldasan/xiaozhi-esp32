#ifndef TAPPY_TIME_MANAGER_H
#define TAPPY_TIME_MANAGER_H

#include <ctime>
#include <string>

class TappyTimeManager {
public:
    static TappyTimeManager& GetInstance();

    void Initialize();
    void StartSync();
    bool IsValid() const;
    time_t Now() const;

    bool GetLocalTime(struct tm& out) const;
    std::string GetLocalTimeString() const;
    std::string GetLocalDateString() const;

private:
    TappyTimeManager() = default;
    bool initialized_ = false;
};

#endif
