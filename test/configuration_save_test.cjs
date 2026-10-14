const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

test('save rejects missing or invalid rotation before writes or restart', () => {
    const root = path.join(__dirname, '..');
    const source = fs.readFileSync(path.join(root, 'src', 'ConfigurationWebServer.cpp'), 'utf8').replace(/\r\n/g, '\n');
    const handler = source.match(/server\.on\("\/save", HTTP_POST, \[&\]\(AsyncWebServerRequest\* request\) \{([\s\S]*?)\n        \}\n    \);/);
    assert.ok(handler, 'Find the actual POST handler to test, not a copy of its validation');
    const harness = `
#include "DisplayRotation.h"
#include "AircraftInfo.h"
constexpr bool Equal(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
struct String {
    const char* text;
    int number = 999;
    constexpr String(const char* value) : text(value) {}
    constexpr String(int value) : text(""), number(value) {}
    constexpr const char* c_str() const { return text; }
    constexpr int indexOf(char ch) const {
        for (int i = 0; text[i]; ++i) if (text[i] == ch) return i;
        return -1;
    }
};
struct Param {
    String text;
    constexpr const String& value() const { return text; }
};
struct AsyncWebServerRequest {
    bool rotationPresent;
    Param rotation;
    Param latitude{"51.5"};
    int status = 0;
    constexpr const Param* getParam(const char* key, bool body) const {
        if (!body) return nullptr;
        if (Equal(key, "rotation")) return rotationPresent ? &rotation : nullptr;
        if (Equal(key, "latitude")) return &latitude;
        return nullptr;
    }
    constexpr bool hasParam(const char* key, bool body) const { return getParam(key, body); }
    constexpr void send(int code, const char*, const char*) { status = code; }
};
struct Preferences {
    int opens = 0, writes = 0, rotation = 999;
    constexpr void begin(const char*, bool) { ++opens; }
    constexpr void putString(const char* key, const String& value) {
        ++writes;
        if (Equal(key, "rotation")) rotation = value.number;
    }
    constexpr void end() {}
};
struct SerialMock { constexpr void println(const char*) {} };
struct EspMock { int restarts = 0; constexpr void restart() { ++restarts; } };
struct Server {
    Preferences prefs;
    SerialMock Serial;
    EspMock ESP;
    constexpr void save(AsyncWebServerRequest* request) {
${handler[1]}
    }
};
constexpr bool Check(bool present, const char* input, bool valid, int expected = 999) {
    AsyncWebServerRequest request{present, {input}};
    Server server;
    server.save(&request);
    return valid
        ? request.status == 200 && server.prefs.opens == 1 && server.prefs.writes > 0 &&
          server.prefs.rotation == expected && server.ESP.restarts == 1
        : request.status == 400 && server.prefs.opens == 0 && server.prefs.writes == 0 &&
          server.ESP.restarts == 0;
}
static_assert(Check(false, "", false), "Missing rotation must not save other submitted settings or restart");
static_assert(Check(true, "", false), "Empty rotation must not save or restart");
static_assert(Check(true, "1.5", false), "Fractional rotation must not save or restart");
static_assert(Check(true, "181", false), "Out-of-range rotation must not save or restart");
static_assert(Check(true, "0", true, 0), "Zero remains valid");
static_assert(Check(true, "-180", true, -180), "Lower boundary remains valid");
static_assert(Check(true, "+180", true, 180), "Upper boundary is stored canonically");
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
