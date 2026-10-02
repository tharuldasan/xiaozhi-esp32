#ifndef TAPPY_WEATHER_SERVICE_H
#define TAPPY_WEATHER_SERVICE_H

#include <string>

class TappyWeatherService {
public:
    static TappyWeatherService& GetInstance();

    void Initialize();
    std::string GetCurrent(const std::string& location);
    std::string SetLocation(const std::string& location);
    std::string GetLocation() const;

private:
    TappyWeatherService() = default;

    std::string UrlEncode(const std::string& value) const;
    std::string HttpGet(const std::string& url) const;
    std::string WeatherDescription(int code) const;
};

#endif
