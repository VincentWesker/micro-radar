#pragma once

#include <cmath>

struct DisplayPoint
{
    int x;
    int y;
};

constexpr int RoundDisplayCoordinate(float value)
{
    return static_cast<int>(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

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

constexpr DisplayPoint RotateDisplayPoint(int x, int y, int degrees, float centreX, float centreY)
{
    if (degrees == 0) return { x, y };
    if (degrees == 90) return {
        RoundDisplayCoordinate(centreX - (y - centreY)),
        RoundDisplayCoordinate(centreY + (x - centreX))
    };
    if (degrees == -90) return {
        RoundDisplayCoordinate(centreX + (y - centreY)),
        RoundDisplayCoordinate(centreY - (x - centreX))
    };
    if (degrees == 180 || degrees == -180) return {
        RoundDisplayCoordinate(centreX - (x - centreX)),
        RoundDisplayCoordinate(centreY - (y - centreY))
    };

    const float angle = degrees * (3.14159265358979323846f / 180.0f);
    const float dx = x - centreX;
    const float dy = y - centreY;
    return {
        RoundDisplayCoordinate(centreX + dx * std::cos(angle) - dy * std::sin(angle)),
        RoundDisplayCoordinate(centreY + dx * std::sin(angle) + dy * std::cos(angle))
    };
}
