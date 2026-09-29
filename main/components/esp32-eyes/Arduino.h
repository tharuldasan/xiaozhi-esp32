#ifndef TAPPY_ESP32_EYES_ARDUINO_COMPAT_H
#define TAPPY_ESP32_EYES_ARDUINO_COMPAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <algorithm>
#include <esp_timer.h>
#include <esp_random.h>

using std::min;
using std::max;

using byte = uint8_t;

inline unsigned long millis() {
    return static_cast<unsigned long>(esp_timer_get_time() / 1000ULL);
}

inline long random(long max_value) {
    if (max_value <= 0) return 0;
    return static_cast<long>(esp_random() % static_cast<uint32_t>(max_value));
}

inline long random(long min_value, long max_value) {
    if (max_value <= min_value) return min_value;
    return min_value + static_cast<long>(
        esp_random() % static_cast<uint32_t>(max_value - min_value));
}

#endif
