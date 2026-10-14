#include "ConfigurationWebServer.h"
#include "AircraftInfo.h"
#include "DisplayRotation.h"
#include "DisplayBrightness.h"
#include <ESPmDNS.h>

// HTML stored in flash
// %PLACEHOLDER% tokens are substituted at serve time by the template processor
static const char CONFIG_HTML[] PROGMEM = R"(
<html>
    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>Configure Micro Radar</title>
        <script src="https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4.3.0"></script>
    </head>
    <body class="font-mono bg-gray-900 text-green-500 min-h-screen p-4 sm:p-0 text-md sm:text-sm">
        <fieldset class="border border-green-500 p-5 w-full max-w-2xl mx-auto sm:m-10">
            <legend class="px-2">Configure Micro Radar</legend>

            <fieldset id="display-controls" class="border border-green-500 p-3 mb-6" aria-describedby="display-help">
                <legend class="px-2">Live display controls</legend>
                <p id="display-help" class="text-sm mb-3">Apply without saving or restarting. Off sleeps the screen and pauses aircraft updates; WiFi and these controls stay available.</p>
                <p class="mb-3">Requested state: <strong id="display-state">%DISPLAY_STATE%</strong></p>
                <div class="flex flex-wrap gap-3 mb-4">
                    <button id="display-on" type="button" class="border border-green-500 px-4 py-3 cursor-pointer hover:bg-green-500 hover:text-black focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-green-500 disabled:opacity-50 disabled:cursor-wait">Turn on</button>
                    <button id="display-off" type="button" class="border border-green-500 px-4 py-3 cursor-pointer hover:bg-green-500 hover:text-black focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-green-500 disabled:opacity-50 disabled:cursor-wait">Turn off</button>
                </div>
                <form id="display-brightness-form" class="flex flex-col gap-3">
                    <label for="display-brightness">Brightness (0&ndash;255):</label>
                    <div class="flex flex-wrap items-center gap-3">
                        <input id="display-brightness" type="number" min="0" max="255" step="1" required value="%DISPLAY_BRIGHTNESS%" aria-describedby="display-brightness-help" class="border border-green-500 bg-gray-900 w-24 px-3 py-3 text-lg focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-green-500">
                        <button type="submit" class="bg-green-500 text-black px-4 py-3 cursor-pointer hover:bg-green-400 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-green-500 disabled:opacity-50 disabled:cursor-wait">Apply brightness</button>
                    </div>
                    <p id="display-brightness-help" class="text-sm">0 is dark; 255 is full brightness. Changing brightness while off does not wake the display. Controls reset to on at 255 after reboot.</p>
                </form>
                <p id="display-result" role="status" class="text-sm mt-3"></p>
                <p class="text-sm mt-3">Reload to refresh settings changed by another controller.</p>
                <noscript>JavaScript is required for live display controls. You can also use the REST API.</noscript>
            </fieldset>

            <form id="cfg" action="/save" method="POST" class="flex flex-col gap-4 sm:gap-2">

                <div class="flex flex-col sm:flex-row gap-4 sm:gap-5">
                    <label class="flex flex-col sm:flex-row gap-2 flex-1">
                        <span>Latitude:</span>
                        <input
                            name="latitude"
                            type="number"
                            min="-90"
                            step="0.000001"
                            max="90"
                            value='%LATITUDE%'
                            class="border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                    </label>

                    <label class="flex flex-col sm:flex-row gap-2 flex-1">
                        <span>Longitude:</span>
                        <input
                            name="longitude"
                            type="number"
                            min="-180"
                            step="0.000001"
                            max="180"
                            value='%LONGITUDE%'
                            class="border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                    </label>
                </div>

                <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                    <span>Radius (in &deg;):</span>
                    <input
                        name="radius"
                        type="number"
                        min="0.000001"
                        step="0.000001"
                        max="2.499999"
                        value='%RADIUS%'
                        aria-describedby="radius-help radius-distance radius-limit"
                        class="flex-1 border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                </label>
                <div class="text-sm">
                    <p id="radius-help">1 degree is about 111.3 km north/south. East/west distance depends on latitude. The search area is rectangular, not circular.</p>
                    <p id="radius-distance" aria-live="polite"></p>
                    <p id="radius-limit" class="mt-2">Maximum: just under 2.5 degrees (about 278 km north/south each way). Larger areas can include more aircraft, using more ESP32 memory and taking longer to download and draw. Use a smaller radius to reduce load.</p>
                </div>

                <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                    <span>OpenSkyAPI Client ID:</span>
                    <input
                        name="opensky-id"
                        value='%OPENSKY_ID%'
                        class="flex-1 border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                </label>

                <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                    <span>OpenSkyAPI Client Secret:</span>
                    <input
                        name="opensky-secret"
                        value='%OPENSKY_SECRET%'
                        class="flex-1 border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                </label>

                <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                    <span>Display rotation (in &deg;):</span>
                    <input
                        name="rotation"
                        type="number"
                        min="-180"
                        max="180"
                        step="1"
                        required
                        value='%ROTATION%'
                        aria-describedby="rotation-help"
                        class="flex-1 border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                </label>
                <p id="rotation-help" class="text-sm">Correct the screen's installed angle in 1-degree steps. Negative = counterclockwise; positive = clockwise; 0 = no rotation. Applies to the whole display after saving.</p>

                <div class="flex flex-col sm:flex-row gap-4 sm:justify-between">
                    <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                        <span>Radar sweep:</span>
                        <input
                            name="scanline"
                            type="checkbox"
                            %SCANLINE%
                            class="px-3 sm:px-1 accent-green-500">
                    </label>
                    <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                        <span>Aircraft Info:</span>
                        <input
                            name="infotext"
                            type="checkbox"
                            %INFOTEXT%
                            class="px-3 sm:px-1 accent-green-500">
                    </label>
                    <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                        <span>Directional Aircraft:</span>
                        <input
                            name="triangle"
                            type="checkbox"
                            %TRIANGLE%
                            class="px-3 sm:px-1 accent-green-500">
                    </label>
                </div>

                <fieldset class="border border-green-500 p-3">
                    <legend class="px-2">Aircraft info fields</legend>
                    <p id="info-help" class="mb-3">Choose up to 3 fields. Uncheck one to select another. The Aircraft Info toggle controls visibility.</p>
                    <div class="grid grid-cols-1 sm:grid-cols-2 gap-3">
                        <label class="flex items-center gap-2"><input name="info-callsign" type="checkbox" %INFO_CALLSIGN% class="accent-green-500" aria-describedby="info-help">Callsign</label>
                        <label class="flex items-center gap-2"><input name="info-speed" type="checkbox" %INFO_SPEED% class="accent-green-500" aria-describedby="info-help">Ground speed (m/s)</label>
                        <label class="flex items-center gap-2"><input name="info-altitude" type="checkbox" %INFO_ALTITUDE% class="accent-green-500" aria-describedby="info-help">Barometric altitude (m)</label>
                        <label class="flex items-center gap-2"><input name="info-country" type="checkbox" %INFO_COUNTRY% class="accent-green-500" aria-describedby="info-help">Origin country</label>
                        <label class="flex items-center gap-2"><input name="info-category" type="checkbox" %INFO_CATEGORY% class="accent-green-500" aria-describedby="info-help">Aircraft category</label>
                        <label class="flex items-center gap-2"><input name="info-track" type="checkbox" %INFO_TRACK% class="accent-green-500" aria-describedby="info-help">True track (degrees)</label>
                    </div>
                </fieldset>

                <div class="flex flex-col sm:flex-row gap-4 sm:gap-5">
                    <input
                        type="submit"
                        value="Save"
                        class="bg-green-500 text-black mt-4 px-4 py-3 text-lg sm:text-base sm:px-2 sm:py-0 self-start cursor-pointer">

                        <div id="result" role="status" class="mt-4 px-1 sm:px-10"></div>
                </div>
            </form>
        </fieldset>

        <script>
            const displayControls = document.getElementById('display-controls');
            const displayResult = document.getElementById('display-result');
            const displayState = document.getElementById('display-state');
            const brightnessForm = document.getElementById('display-brightness-form');
            const brightnessInput = document.getElementById('display-brightness');
            async function updateDisplay(url, onAccepted) {
                displayControls.disabled = true;
                displayControls.setAttribute('aria-busy', 'true');
                displayResult.textContent = 'Sending display command...';
                try {
                    const response = await fetch(url, { method: 'PUT' });
                    const message = await response.text();
                    if (!response.ok) throw new Error(message || 'The device rejected the command.');
                    onAccepted();
                    displayResult.textContent = message + ' Applies on the next radar loop; an ongoing network request may delay it.';
                } catch (error) {
                    displayResult.textContent = 'Could not update display. ' + error.message + ' Check your connection and try again.';
                } finally {
                    displayControls.disabled = false;
                    displayControls.removeAttribute('aria-busy');
                }
            }
            document.getElementById('display-on').addEventListener('click', () => {
                updateDisplay('/api/display/on', () => displayState.textContent = 'On');
            });
            document.getElementById('display-off').addEventListener('click', () => {
                updateDisplay('/api/display/off', () => displayState.textContent = 'Off');
            });
            brightnessForm.addEventListener('submit', (event) => {
                event.preventDefault();
                if (!brightnessForm.reportValidity()) return;
                const brightness = brightnessInput.valueAsNumber;
                updateDisplay('/api/display/brightness?value=' + encodeURIComponent(brightness),
                    () => brightnessInput.value = brightness);
            });

            const latitudeInput = document.querySelector('input[name="latitude"]');
            const radiusInput = document.querySelector('input[name="radius"]');
            function updateRadiusDistance() {
                const latitude = latitudeInput.valueAsNumber;
                const radius = radiusInput.valueAsNumber;
                const output = document.getElementById('radius-distance');
                if (!Number.isFinite(latitude) || !Number.isFinite(radius) ||
                    !latitudeInput.validity.valid || !radiusInput.validity.valid) {
                    output.textContent = 'Enter a valid latitude and radius to estimate distances.';
                    return;
                }
                const northSouth = radius * 111.32;
                const eastWest = northSouth * Math.cos(latitude * Math.PI / 180);
                output.textContent = 'Approx. reach from centre: ' + northSouth.toFixed(1) +
                    ' km each way north/south; ' + eastWest.toFixed(1) + ' km each way east/west.';
            }
            latitudeInput.addEventListener('input', updateRadiusDistance);
            radiusInput.addEventListener('input', updateRadiusDistance);
            updateRadiusDistance();

            const infoFields = Array.from(document.querySelectorAll('input[name^="info-"]'));
            const maxInfoFields = %MAX_INFO_FIELDS%;
            function updateInfoFields() {
                const atLimit = infoFields.filter(field => field.checked).length >= maxInfoFields;
                infoFields.forEach(field => {
                    field.disabled = atLimit && !field.checked;
                    field.parentElement.classList.toggle('opacity-50', field.disabled);
                });
            }
            infoFields.forEach(field => field.addEventListener('change', updateInfoFields));
            updateInfoFields();
            document.getElementById('cfg').addEventListener('submit', function(e) {
                e.preventDefault();
                if (infoFields.filter(field => field.checked).length > maxInfoFields) {
                    document.getElementById('result').textContent = 'Choose no more than 3 aircraft info fields.';
                    return;
                }
                fetch(this.action, { method: 'POST', body: new FormData(this) })
                    .then(r => r.text())
                    .then(html => document.getElementById('result').innerHTML = html)
                    .catch(() => document.getElementById('result').textContent = 'Could not save. Check your connection and try again.');
            });
        </script>
    </body>
</html>
)";

void ConfigurationWebServer::Initialise() {
    // start mDNS and check result
    if (!MDNS.begin("microradar")) {
        Serial.println("[WARN] Failed to start mDNS. Continuing without mDNS...");
    }

    // Handle visit to config web server
    server.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
        Serial.println("[GET] Handling request to config web server...");

        // read all values up front so the processor lambda can capture by value
        prefs.begin("config", true);
        const String latitude = prefs.getString("latitude", "");
        const String longitude = prefs.getString("longitude", "");
        const String radius = prefs.getString("radius", "1.0");
        const String rotation = prefs.getString("rotation", "0");
        const String openskyClientId = prefs.getString("opensky-id", "");
        String openskySecret = prefs.getString("opensky-secret", "");
        const String scanlineEnabled = prefs.getString("scanline", "true");
        const String infoTextEnabled = prefs.getString("infotext", "true");
        const String triangleEnabled = prefs.getString("triangle", "true");
        const String displayState = IsDisplayEnabled() ? "On" : "Off";
        const String displayBrightness = String(GetDisplayBrightness());
        unsigned infoFields = 0;
        for (const auto& option : AircraftInfoOptions) {
            const char* defaultValue = (DefaultAircraftInfoFields & option.field) ? "true" : "false";
            if (prefs.getString(option.key, defaultValue) == "true") infoFields |= option.field;
        }
        prefs.end();

        // mask secret before sending to client
        std::fill(openskySecret.begin(), openskySecret.end(), '*');

        // template processor called once per %PLACEHOLDER% token found in CONFIG_HTML.
        AsyncWebServerResponse* response = request->beginResponse(
            200, "text/html",
            (const uint8_t*)CONFIG_HTML, sizeof(CONFIG_HTML) - 1,
            [latitude, longitude, radius, rotation, openskyClientId, openskySecret, scanlineEnabled, infoTextEnabled, triangleEnabled, infoFields, displayState, displayBrightness]
            (const String& var) -> String {
                if (var == "LATITUDE")       return latitude;
                if (var == "LONGITUDE")      return longitude;
                if (var == "RADIUS")         return radius;
                if (var == "ROTATION")       return rotation;
                if (var == "OPENSKY_ID")     return openskyClientId;
                if (var == "OPENSKY_SECRET") return openskySecret;
                if (var == "SCANLINE")       return scanlineEnabled == "true" ? "checked" : "";
                if (var == "INFOTEXT")       return infoTextEnabled == "true" ? "checked" : "";
                if (var == "TRIANGLE")       return triangleEnabled == "true" ? "checked" : "";
                if (var == "MAX_INFO_FIELDS") return String(MaxAircraftInfoFields);
                if (var == "DISPLAY_STATE") return displayState;
                if (var == "DISPLAY_BRIGHTNESS") return displayBrightness;
                for (const auto& option : AircraftInfoOptions) {
                    if (var == option.placeholder) return (infoFields & option.field) ? "checked" : "";
                }
                return "";
            }
        );
        request->send(response);
        }
    );

    // Handle save submission to web server
    server.on("/save", HTTP_POST, [&](AsyncWebServerRequest* request) {
        Serial.println("[POST] Handling form submission to config web server...");

        const auto* rotationParam = request->getParam("rotation", true);
        int rotation = 0;
        if (!rotationParam || !ParseDisplayRotation(rotationParam->value().c_str(), rotation)) {
            Serial.println("[WARN] Rejected invalid display rotation.");
            request->send(400, "text/plain", "Display rotation must be a whole number from -180 to 180 degrees. Settings were not saved.");
            return;
        }

        unsigned infoFields = 0;
        for (const auto& option : AircraftInfoOptions) {
            if (request->hasParam(option.key, true)) infoFields |= option.field;
        }
        if (!IsValidAircraftInfoSelection(infoFields)) {
            Serial.println("[WARN] Rejected more than 3 aircraft info fields.");
            request->send(400, "text/plain", "Choose no more than 3 aircraft info fields. Settings were not saved.");
            return;
        }

        // safe parameter retrieval helper lambda
        auto TrySaveParam = [request, this](const char* paramName) {
            const auto* param = request->getParam(paramName, true);
            if (param == nullptr)
                return false;

            prefs.putString(paramName, param->value());
            return true;
            };

        prefs.begin("config", false);

        TrySaveParam("latitude");
        TrySaveParam("longitude");
        TrySaveParam("radius");
        prefs.putString("rotation", String(rotation));
        TrySaveParam("opensky-id");

        const auto* param = request->getParam("opensky-secret", true);
        if (param != nullptr) {
            const String& secret = param->value();
            if (secret.indexOf('*') == -1) { // Special handling for secret: don't overwrite with masked value
                prefs.putString("opensky-secret", secret);
            }
        }

        prefs.putString("scanline", request->hasParam("scanline", true) ? "true" : "false");
        prefs.putString("triangle", request->hasParam("triangle", true) ? "true" : "false");
        prefs.putString("infotext", request->hasParam("infotext", true) ? "true" : "false");
        for (const auto& option : AircraftInfoOptions) {
            prefs.putString(option.key, (infoFields & option.field) ? "true" : "false");
        }
        prefs.end();

        request->send(200, "text/html", "Saved - restarting device...");
        ESP.restart();
        }
    );

    server.on("/api/display/on", HTTP_PUT, [this](AsyncWebServerRequest* request) {
        displayEnabled.store(true);
        request->send(202, "text/plain", "Display on requested.");
    });

    server.on("/api/display/off", HTTP_PUT, [this](AsyncWebServerRequest* request) {
        displayEnabled.store(false);
        request->send(202, "text/plain", "Display off requested.");
    });

    server.on("/api/display/brightness", HTTP_PUT, [this](AsyncWebServerRequest* request) {
        const auto* param = request->getParam("value");
        uint8_t brightness = 0;
        if (!param || !ParseDisplayBrightness(param->value().c_str(), brightness)) {
            Serial.println("[WARN] Rejected invalid display brightness.");
            request->send(400, "text/plain", "Brightness must be a whole number from 0 to 255 in the value query parameter.");
            return;
        }
        displayBrightness.store(brightness);
        request->send(202, "text/plain", "Display brightness requested.");
    });

    server.begin();
}

const String ConfigurationWebServer::GetStoredString(const char* key)
{
    prefs.begin("config", true);
    const String value = prefs.getString(key, "");
    prefs.end();
    return value;
}