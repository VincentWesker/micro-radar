#pragma once

enum AircraftInfoField : unsigned {
    Callsign = 1u << 0,
    Speed = 1u << 1,
    Altitude = 1u << 2,
    Country = 1u << 3,
    Category = 1u << 4,
    TrueTrack = 1u << 5
};

struct AircraftInfoOption {
    const char* key;
    const char* placeholder;
    unsigned field;
};

inline constexpr AircraftInfoOption AircraftInfoOptions[] = {
    { "info-callsign", "INFO_CALLSIGN", Callsign },
    { "info-speed", "INFO_SPEED", Speed },
    { "info-altitude", "INFO_ALTITUDE", Altitude },
    { "info-country", "INFO_COUNTRY", Country },
    { "info-category", "INFO_CATEGORY", Category },
    { "info-track", "INFO_TRACK", TrueTrack }
};

inline constexpr unsigned DefaultAircraftInfoFields = Callsign | Speed | Altitude;
inline constexpr unsigned MaxAircraftInfoFields = 3;

constexpr bool IsValidAircraftInfoSelection(unsigned fields) {
    if (fields & ~63u) return false;
    unsigned count = 0;
    for (; fields; fields >>= 1) count += fields & 1u;
    return count <= MaxAircraftInfoFields;
}

constexpr const char* AircraftCategoryName(int category) {
    constexpr const char* names[] = {
        "Unknown", "Unspecified", "Light", "Small", "Large",
        "High vortex large", "Heavy", "High performance", "Rotorcraft",
        "Glider / sailplane", "Lighter-than-air", "Parachutist",
        "Ultralight", "Reserved", "UAV", "Space vehicle",
        "Emergency vehicle", "Service vehicle", "Point obstacle",
        "Cluster obstacle", "Line obstacle"
    };
    return category >= 0 && category <= 20 ? names[category] : "Unknown";
}
