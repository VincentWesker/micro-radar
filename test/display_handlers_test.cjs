const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

test('backlight PWM explicitly uses a supported ESP32-C3 channel', () => {
    const root = path.join(__dirname, '..');
    const source = fs.readFileSync(path.join(root, 'include', 'LGFX.h'), 'utf8');
    const lightConfig = source.match(/auto cfg = _light\.config\(\);([\s\S]*?)_light\.config\(cfg\);/);
    assert.ok(lightConfig, 'Find the backlight configuration');
    const channel = lightConfig[1].match(/cfg\.pwm_channel\s*=\s*(\d+)\s*;/);
    assert.ok(channel, 'Override LovyanGFX default channel 7, which is invalid on ESP32-C3');
    assert.ok(Number(channel[1]) < 6, 'ESP32-C3 supports PWM channels 0 through 5');
    assert.match(lightConfig[1], /cfg\.pin_bl\s*=\s*3\s*;/, 'Keep the board backlight pin');
    assert.match(source, /_panel\.setLight\(&_light\)/, 'Attach the configured backlight to the panel');
});

test('registered display handlers validate requests and update only the requested state', () => {
    const root = path.join(__dirname, '..');
    const source = fs.readFileSync(path.join(root, 'src', 'ConfigurationWebServer.cpp'), 'utf8').replace(/\r\n/g, '\n');
    const header = fs.readFileSync(path.join(root, 'src', 'ConfigurationWebServer.h'), 'utf8');
    const handlers = ['on', 'off', 'brightness'].map(endpoint => {
        const pattern = new RegExp(`server\\.on\\("/api/display/${endpoint}", (HTTP_\\w+), \\[this\\]\\(AsyncWebServerRequest\\* request\\) \\{([\\s\\S]*?)\\n    \\}\\);`);
        const handler = source.match(pattern);
        assert.ok(handler, `Find the actual ${endpoint} handler`);
        assert.equal(handler[1], 'HTTP_PUT', `${endpoint} must remain a PUT endpoint`);
        return `constexpr void ${endpoint}(AsyncWebServerRequest* request) {${handler[2]}\n}`;
    });
    const state = header.match(/std::atomic<[^>]+> display(?:Enabled|Brightness)\{[^}]+\};/g);
    assert.equal(state.length, 2, 'Use both actual atomic state declarations and their defaults');
    const enabledGetter = header.match(/bool IsDisplayEnabled\(\) const \{([\s\S]*?)\n    \}/);
    const brightnessGetter = header.match(/uint8_t GetDisplayBrightness\(\) const \{([\s\S]*?)\n    \}/);
    assert.ok(enabledGetter && brightnessGetter, 'Exercise the actual state getters');
    const main = fs.readFileSync(path.join(root, 'src', 'main.cpp'), 'utf8').replace(/\r\n/g, '\n');
    const loop = main.match(/void loop\(\)\n\{([\s\S]*?)\n  \/\/ draw cycle/);
    assert.ok(loop, 'Exercise the actual control and polling portion of the main loop');
    assert.match(loop[1], /static bool displayAwake = true;/);
    const harness = `
#include "DisplayBrightness.h"
constexpr bool Equal(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
struct String {
    const char* text;
    constexpr const char* c_str() const { return text; }
};
struct Param {
    String text;
    constexpr const String& value() const { return text; }
};
struct AsyncWebServerRequest {
    bool queryPresent = false;
    Param query{{""}};
    bool bodyPresent = false;
    int status = 0;
    const char* contentType = "";
    const char* message = "";
    constexpr const Param* getParam(const char* key, bool body = false) const {
        return Equal(key, "value") && (body ? bodyPresent : queryPresent) ? &query : nullptr;
    }
    constexpr void send(int code, const char* type, const char* text) {
        status = code; contentType = type; message = text;
    }
};
template<class T> struct AtomicMock {
    T value;
    int writes = 0;
    constexpr AtomicMock(T initial) : value(initial) {}
    constexpr T load() const { return value; }
    constexpr void store(T next) { value = next; ++writes; }
};
struct SerialMock {
    int warnings = 0;
    constexpr void println(const char*) { ++warnings; }
};
struct Server {
    ${state.join('\n    ').replaceAll('std::atomic', 'AtomicMock')}
    SerialMock Serial;
    constexpr bool IsDisplayEnabled() const {${enabledGetter[1]}}
    constexpr uint8_t GetDisplayBrightness() const {${brightnessGetter[1]}}
    ${handlers.join('\n    ')}
};
constexpr bool CheckPower() {
    Server server;
    AsyncWebServerRequest request;
    if (!server.IsDisplayEnabled() || server.GetDisplayBrightness() != 255) return false;
    server.displayBrightness.store(96);
    for (int i = 0; i < 2; ++i) {
        server.off(&request);
        if (request.status != 202 || !Equal(request.contentType, "text/plain") ||
            server.IsDisplayEnabled() || server.GetDisplayBrightness() != 96) return false;
    }
    for (int i = 0; i < 2; ++i) {
        server.on(&request);
        if (request.status != 202 || !Equal(request.contentType, "text/plain") ||
            !server.IsDisplayEnabled() || server.GetDisplayBrightness() != 96) return false;
    }
    return server.Serial.warnings == 0;
}
constexpr bool CheckBrightness(bool present, const char* value, bool valid,
                               uint8_t expected = 96, bool body = false, bool enabled = true) {
    Server server;
    server.displayEnabled.store(enabled);
    server.displayBrightness.store(96);
    AsyncWebServerRequest request{present, {{value}}, body};
    server.brightness(&request);
    return request.status == (valid ? 202 : 400) &&
        Equal(request.contentType, "text/plain") && *request.message &&
        server.IsDisplayEnabled() == enabled &&
        server.displayEnabled.writes == 1 &&
        server.GetDisplayBrightness() == expected &&
        server.displayBrightness.writes == (valid ? 2 : 1) &&
        server.Serial.warnings == (valid ? 0 : 1);
}
constexpr bool CheckEveryBrightness() {
    for (unsigned value = 0; value <= 255; ++value) {
        char text[] = {char('0' + value / 100), char('0' + value / 10 % 10), char('0' + value % 10), '\\0'};
        if (!CheckBrightness(true, text, true, value) ||
            !CheckBrightness(true, text, true, value, false, false)) return false;
    }
    return true;
}
static_assert(CheckPower(), "Repeated on/off commands preserve brightness and return 202");
static_assert(CheckEveryBrightness(), "Every valid query value updates brightness without changing power");
static_assert(CheckBrightness(false, "", false), "Missing query returns 400 without changing state");
static_assert(CheckBrightness(false, "128", false, 96, true), "Body parameters are not query parameters");
static_assert(CheckBrightness(true, "", false), "Empty brightness returns 400");
static_assert(CheckBrightness(true, "-1", false), "Negative brightness returns 400");
static_assert(CheckBrightness(true, "+1", false), "Signed brightness returns 400");
static_assert(CheckBrightness(true, "256", false), "Out-of-range brightness returns 400");
static_assert(CheckBrightness(true, "1.5", false), "Fractional brightness returns 400");
static_assert(CheckBrightness(true, "1e2", false), "Exponent notation returns 400");
static_assert(CheckBrightness(true, "128abc", false), "Trailing text returns 400");
static_assert(CheckBrightness(true, " 128", false), "Leading whitespace returns 400");
static_assert(CheckBrightness(true, "128 ", false), "Trailing whitespace returns 400");
static_assert(CheckBrightness(true, "999999999999999999999999", false), "Overflow returns 400");
static_assert(CheckBrightness(true, "256", false, 96, false, false), "Invalid requests do not wake the display");
struct DisplayMock {
    uint8_t brightness = 255;
    bool sleeping = false;
    int sleeps = 0, wakes = 0, brightnessWrites = 0;
    constexpr uint8_t getBrightness() const { return brightness; }
    constexpr void setBrightness(uint8_t next) { brightness = next; ++brightnessWrites; }
    constexpr void sleep() { sleeping = true; ++sleeps; }
    constexpr void wakeup() { sleeping = false; ++wakes; }
};
struct ConfigMock {
    bool enabled = true;
    uint8_t brightness = 255;
    constexpr bool IsDisplayEnabled() const { return enabled; }
    constexpr uint8_t GetDisplayBrightness() const { return brightness; }
};
struct AircraftMock {
    DisplayMock& display;
    int updates = 0;
    uint8_t brightnessAtPoll = 0;
    bool sleepingAtPoll = false;
    constexpr void Update() {
        ++updates;
        brightnessAtPoll = display.brightness;
        sleepingAtPoll = display.sleeping;
    }
};
struct MainLoop {
    DisplayMock tft;
    ConfigMock configServer;
    AircraftMock aircraftManager{tft};
    bool displayAwake = true;
    int delayed = 0;
    constexpr void delay(int milliseconds) { delayed += milliseconds; }
    constexpr void loop() {
${loop[1].replace('static bool displayAwake = true;', '')}
    }
};
constexpr bool CheckLoop() {
    MainLoop main;
    main.configServer.brightness = 128;
    main.loop();
    if (main.aircraftManager.updates != 1 || main.aircraftManager.brightnessAtPoll != 128) return false;
    main.configServer.enabled = false;
    main.loop();
    main.configServer.brightness = 96;
    main.loop();
    if (main.aircraftManager.updates != 1 || main.tft.brightness != 0 ||
        main.tft.sleeps != 1 || main.delayed != 20) return false;
    main.configServer.enabled = true;
    main.loop();
    if (main.tft.wakes != 1 || main.aircraftManager.updates != 2 ||
        main.aircraftManager.brightnessAtPoll != 96 || main.aircraftManager.sleepingAtPoll) return false;
    main.loop();
    return main.tft.wakes == 1 && main.tft.brightnessWrites == 3 &&
        main.aircraftManager.updates == 3;
}
static_assert(CheckLoop(), "Apply brightness before polling, skip updates while off, and wake only once");
`;
    const compiler = process.env.CXX || path.join(
        process.env.USERPROFILE, '.platformio', 'packages', 'toolchain-riscv32-esp',
        'bin', 'riscv32-esp-elf-g++.exe'
    );
    const result = spawnSync(compiler, [
        '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Iinclude', '-x', 'c++', '-fsyntax-only', '-'
    ], { cwd: root, input: harness, encoding: 'utf8' });
    assert.ifError(result.error);
    assert.equal(result.status, 0, result.stdout + result.stderr);
});
