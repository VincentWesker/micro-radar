#include "DisplayRotation.h"

constexpr bool CheckRotation(const char* text, bool valid, int expected = 0)
{
    int degrees = 999;
    return ParseDisplayRotation(text, degrees) == valid &&
        degrees == (valid ? expected : 999);
}

constexpr bool CheckEveryDegree()
{
    for (int value = 0; value <= 181; ++value) {
        char text[] = {'+', char('0' + value / 100), char('0' + value / 10 % 10), char('0' + value % 10), '\0'};
        if (!CheckRotation(text, value <= 180, value)) return false;
        text[0] = '-';
        if (!CheckRotation(text, value <= 180, -value)) return false;
    }
    return true;
}

static_assert(CheckEveryDegree(), "Support every whole degree in both directions, including boundaries");
static_assert(CheckRotation("0", true, 0), "Keep the default orientation");
static_assert(CheckRotation("-0", true, 0), "Accept signed zero");
static_assert(CheckRotation("1", true, 1), "Rotate clockwise by one degree");
static_assert(CheckRotation("-1", true, -1), "Rotate counterclockwise by one degree");
static_assert(CheckRotation("+37", true, 37), "Accept an explicit positive sign");
static_assert(CheckRotation("-37", true, -37), "Accept arbitrary whole degrees");
static_assert(CheckRotation("90", true, 90), "Accept quarter turns");
static_assert(CheckRotation("180", true, 180), "Accept the upper boundary");
static_assert(CheckRotation("-180", true, -180), "Accept the lower boundary");
static_assert(CheckRotation("181", false), "Reject values above the maximum");
static_assert(CheckRotation("-181", false), "Reject values below the minimum");
static_assert(CheckRotation("", false), "Reject missing values");
static_assert(CheckRotation("-", false), "Reject a sign without digits");
static_assert(CheckRotation("+", false), "Reject a sign without digits");
static_assert(CheckRotation("1.5", false), "Reject fractional degrees");
static_assert(CheckRotation("1e2", false), "Reject exponent notation");
static_assert(CheckRotation("12degrees", false), "Reject trailing text");
static_assert(CheckRotation(" 12", false), "Reject leading whitespace");
static_assert(CheckRotation("12 ", false), "Reject trailing whitespace");
static_assert(CheckRotation("--1", false), "Reject repeated signs");
static_assert(CheckRotation("99999999999999999999999999999", false), "Reject overflow");

constexpr auto ClockwisePoint = RotateDisplayPoint(110, 100, 90, 100, 100);
static_assert(ClockwisePoint.x == 100 && ClockwisePoint.y == 110, "Rotate labels clockwise with the frame");
constexpr auto CounterclockwisePoint = RotateDisplayPoint(110, 100, -90, 100, 100);
static_assert(CounterclockwisePoint.x == 100 && CounterclockwisePoint.y == 90, "Rotate labels counterclockwise with the frame");
constexpr auto UnchangedPoint = RotateDisplayPoint(110, 100, 0, 100, 100);
static_assert(UnchangedPoint.x == 110 && UnchangedPoint.y == 100, "Keep labels in place without rotation");
