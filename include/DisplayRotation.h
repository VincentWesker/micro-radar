#pragma once

constexpr bool ParseDisplayRotation(const char* text, int& degrees)
{
    const bool negative = *text == '-';
    if (*text == '-' || *text == '+') ++text;
    if (!*text) return false;

    int value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        value = value * 10 + (*text - '0');
        if (value > 180) return false;
    }
    degrees = negative ? -value : value;
    return true;
}
