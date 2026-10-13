#include "ConfigurationWebServer.h"
#include "AircraftInfo.h"
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
        const String openskyClientId = prefs.getString("opensky-id", "");
        String openskySecret = prefs.getString("opensky-secret", "");
        const String scanlineEnabled = prefs.getString("scanline", "true");
        const String infoTextEnabled = prefs.getString("infotext", "true");
        const String triangleEnabled = prefs.getString("triangle", "true");
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
            [latitude, longitude, radius, openskyClientId, openskySecret, scanlineEnabled, infoTextEnabled, triangleEnabled, infoFields]
            (const String& var) -> String {
                if (var == "LATITUDE")       return latitude;
                if (var == "LONGITUDE")      return longitude;
                if (var == "RADIUS")         return radius;
                if (var == "OPENSKY_ID")     return openskyClientId;
                if (var == "OPENSKY_SECRET") return openskySecret;
                if (var == "SCANLINE")       return scanlineEnabled == "true" ? "checked" : "";
                if (var == "INFOTEXT")       return infoTextEnabled == "true" ? "checked" : "";
                if (var == "TRIANGLE")       return triangleEnabled == "true" ? "checked" : "";
                if (var == "MAX_INFO_FIELDS") return String(MaxAircraftInfoFields);
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

    server.begin();
}

const String ConfigurationWebServer::GetStoredString(const char* key)
{
    prefs.begin("config", true);
    const String value = prefs.getString(key, "");
    prefs.end();
    return value;
}