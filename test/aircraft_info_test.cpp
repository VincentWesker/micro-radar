#include "AircraftInfo.h"

constexpr bool CheckEverySelection() {
    for (unsigned fields = 0; fields < 64; ++fields) {
        unsigned count = 0;
        for (const auto& option : AircraftInfoOptions) {
            if (fields & option.field) ++count;
        }
        if (IsValidAircraftInfoSelection(fields) != (count <= 3)) return false;
    }
    return true;
}

constexpr bool Equal(const char* left, const char* right) {
    while (*left && *left == *right) { ++left; ++right; }
    return *left == *right;
}

static_assert(CheckEverySelection(), "Allow exactly zero through three selected fields");
static_assert(DefaultAircraftInfoFields == (Callsign | Speed | Altitude), "Preserve existing defaults");
static_assert(!IsValidAircraftInfoSelection(64), "Reject unknown fields");
static_assert(Equal(AircraftCategoryName(-1), "Unknown"), "Handle invalid categories");
static_assert(Equal(AircraftCategoryName(0), "Unknown"), "Handle missing category");
static_assert(Equal(AircraftCategoryName(1), "Unspecified"), "Handle unspecified category");
static_assert(Equal(AircraftCategoryName(8), "Rotorcraft"), "Map aircraft categories");
static_assert(Equal(AircraftCategoryName(20), "Line obstacle"), "Map the last category");
static_assert(Equal(AircraftCategoryName(21), "Unknown"), "Handle future categories");
