#pragma once

#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <atomic>

class ConfigurationWebServer {
private:
    AsyncWebServer server;
    Preferences prefs;
    std::atomic<bool> displayEnabled{true};
    std::atomic<uint8_t> displayBrightness{255};

public:
    ConfigurationWebServer() : server(80), prefs() {}
    ConfigurationWebServer(int port) : server(port), prefs() {}

    void Initialise();
    [[nodiscard]] const String GetStoredString(const char* key);
    [[nodiscard]] bool IsDisplayEnabled() const {
        return displayEnabled.load();
    }
    [[nodiscard]] uint8_t GetDisplayBrightness() const {
        return displayBrightness.load();
    }
};