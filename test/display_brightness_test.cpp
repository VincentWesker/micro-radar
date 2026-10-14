#include "DisplayBrightness.h"

constexpr bool CheckBrightness(const char* text, bool valid, uint8_t expected = 0)
{
    uint8_t brightness = 42;
    return ParseDisplayBrightness(text, brightness) == valid &&
        brightness == (valid ? expected : 42);
}

constexpr bool CheckEveryBrightness()
{
    for (unsigned value = 0; value <= 256; ++value) {
        char text[] = {char('0' + value / 100), char('0' + value / 10 % 10), char('0' + value % 10), '\0'};
        if (!CheckBrightness(text, value <= 255, static_cast<uint8_t>(value))) return false;
    }
    return true;
}

static_assert(CheckEveryBrightness(), "Accept every brightness level and reject 256");
static_assert(CheckBrightness("0", true, 0), "Allow a dark backlight");
static_assert(CheckBrightness("255", true, 255), "Allow full brightness");
static_assert(CheckBrightness(nullptr, false), "Reject a missing value");
static_assert(CheckBrightness("", false), "Reject an empty value");
static_assert(CheckBrightness("-1", false), "Reject negative values");
static_assert(CheckBrightness("+1", false), "Reject signs");
static_assert(CheckBrightness("1.5", false), "Reject fractional values");
static_assert(CheckBrightness("1e2", false), "Reject exponent notation");
static_assert(CheckBrightness("12abc", false), "Reject trailing text");
static_assert(CheckBrightness(" 12", false), "Reject leading whitespace");
static_assert(CheckBrightness("12 ", false), "Reject trailing whitespace");
static_assert(CheckBrightness("99999999999999999999999999999", false), "Reject overflow");
