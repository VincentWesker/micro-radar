const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

test('rotated display text anchors follow the frame without rotating glyphs', () => {
    const root = path.join(__dirname, '..');
    const main = fs.readFileSync(path.join(root, 'src', 'main.cpp'), 'utf8').replace(/\r\n/g, '\n');
    const aircraft = fs.readFileSync(path.join(root, 'src', 'AircraftManager.cpp'), 'utf8').replace(/\r\n/g, '\n');
    const wifi = fs.readFileSync(path.join(root, 'include', 'WiFiManagerHelpers.h'), 'utf8').replace(/\r\n/g, '\n');

    assert.match(main, /aircraftManager\.Draw\(backbuffer\);\s*PushDisplayFrame\(backbuffer, displayRotation\);\s*aircraftManager\.DrawInfo\(displayRotation\);/);
    assert.match(aircraft, /RotateDisplayPoint\([\s\S]*?rotation,[\s\S]*?tft\.drawString\(text, label\.x, textY\)/);
    assert.match(wifi, /PushDisplayFrame\(backbuffer, rotation\);\s*tft\.setTextColor[\s\S]*?tft\.drawCentreString/);

    const compiler = process.env.CXX || path.join(
        process.env.USERPROFILE, '.platformio', 'packages', 'toolchain-riscv32-esp',
        'bin', 'riscv32-esp-elf-g++.exe'
    );
    const compile = spawnSync(compiler, [
        '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Iinclude', '-fsyntax-only',
        'test/display_rotation_test.cpp'
    ], { cwd: root, encoding: 'utf8' });
    assert.ifError(compile.error);
    assert.equal(compile.status, 0, compile.stdout + compile.stderr);
});
