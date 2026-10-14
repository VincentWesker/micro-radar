#pragma once

#include <cstdint>

constexpr bool ParseDisplayBrightness(const char* text, uint8_t& brightness)
{
    if (!text || !*text) return false;

    unsigned value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        value = value * 10 + (*text - '0');
        if (value > 255) return false;
    }
    brightness = static_cast<uint8_t>(value);
    return true;
}
