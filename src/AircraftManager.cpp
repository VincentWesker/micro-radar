#include "AircraftManager.h"
#include "RadarGeometry.h"
#include "DisplayRotation.h"

#include <ArduinoJson.h>

void AircraftManager::Initialise()
{
    // get centre point + radius
    lat = configServer.GetStoredString("latitude").toDouble();
    lon = configServer.GetStoredString("longitude").toDouble();
    rad = configServer.GetStoredString("radius").toDouble();

    // configuration
    const String renderText = configServer.GetStoredString("infotext");
    const String renderTris = configServer.GetStoredString("triangle");
    if (!renderText.isEmpty()) displayInfoText = renderText == "true" ? true : false;
    if (!renderTris.isEmpty()) displayTriangles = renderTris == "true" ? true : false;
    for (const auto& option : AircraftInfoOptions) {
        const String enabled = configServer.GetStoredString(option.key);
        if (enabled.isEmpty()) continue;
        if (enabled == "true") infoFields |= option.field;
        else infoFields &= ~option.field;
    }
    if (!IsValidAircraftInfoSelection(infoFields)) {
        Serial.println("[WARN] Invalid aircraft info selection; using default fields.");
        infoFields = DefaultAircraftInfoFields;
    }

    // calculate how often we can call OpenSky API before being rate limited
    constexpr int MS_PER_DAY = 24 * 60 * 60 * 1000;
    constexpr int ANONYMOUS_TOKENS_PER_DAY = 400;
    constexpr int AUTHED_TOKENS_PER_DAY = 4000;
    constexpr int TOKEN_BUFFER = 3;
    int dailyRequestBudget = ANONYMOUS_TOKENS_PER_DAY - TOKEN_BUFFER; // non-authed tokens minus buffer

    const String token = authHandler.GetValidToken(configServer.GetStoredString("opensky-id"), configServer.GetStoredString("opensky-secret"));
    if (!token.isEmpty())
        dailyRequestBudget = AUTHED_TOKENS_PER_DAY - TOKEN_BUFFER; // authed tokens minus buffer

    fetchInterval = MS_PER_DAY / dailyRequestBudget;
}

void AircraftManager::Update()
{
    unsigned long now = millis();

    // fetch cycle
    if (now - lastFetch >= fetchInterval) {
        lastFetch = now;

        // auth
        const String token = authHandler.GetValidToken(
            configServer.GetStoredString("opensky-id"),
            configServer.GetStoredString("opensky-secret")
        );

        std::vector<std::pair<String, String>> headers = {};
        if (!token.isEmpty()) headers.push_back({ "Authorization", "Bearer " + token });

        // request
        HttpResult result = http.Get(
            "https://opensky-network.org/api/states/all",
            {
              {"lamin", String(lat - rad)},
              {"lamax", String(lat + rad)},
              {"lomin", String(lon - rad)},
              {"lomax", String(lon + rad)},
              {"extended", "1"}
            },
            headers
        );

        // If request failed, skip this update
        if (!result.success) {
            Serial.print("[WARN] OpenSky API request failed: ");
            Serial.println(result.errorMessage);
            return;
        }

        // track
        JsonDocument doc;
        deserializeJson(doc, result.response);
        auto aircraft = JsonParser::ParseArray<Aircraft>(doc["states"]);
        now = millis(); // override with post-parse timestamp

        for (auto& ac : aircraft) {
            auto it = trackedAircraft.find(ac.icao24);
            if (it == trackedAircraft.end())
                trackedAircraft.emplace(ac.icao24, TrackedAircraft{ ac, now });
            else
                it->second.Update(ac, now);
        }

        // remove any planes that disappeared from the feed
        for (auto it = trackedAircraft.begin(); it != trackedAircraft.end(); ) {
            bool aircraftPresent = std::any_of(aircraft.begin(), aircraft.end(), [&](const Aircraft& ac) { return ac.icao24 == it->first; });
            if (!aircraftPresent)
                it = trackedAircraft.erase(it);
            else
                ++it;
        }
    }
}

void AircraftManager::Draw(LGFX_Sprite& backbuffer)
{
    DrawRadarCircles(backbuffer);

    for (auto& [icao, tracked] : trackedAircraft) {
        if (tracked.state.onGround) continue;

        tracked.Tick();
        auto [predLat, predLon] = tracked.GetDisplayPosition();
        auto [x, y] = ProjectCoordinateToScreen(predLat, predLon);

        if (displayTriangles)
            DrawAircraftTriangle(backbuffer, x, y, tracked);
        else
            backbuffer.fillCircle(x, y, 3, lgfx::color888(0, 255, 0));
    }
}

void AircraftManager::DrawInfo(int rotation) const
{
    if (!displayInfoText) return;

    for (const auto& [icao, tracked] : trackedAircraft) {
        if (tracked.state.onGround) continue;

        const auto [lat, lon] = tracked.GetDisplayPosition();
        const auto [x, y] = ProjectCoordinateToScreen(lat, lon);
        DrawAircraftInfo(x, y, tracked, rotation);
    }
}

void AircraftManager::DrawRadarCircles(LGFX_Sprite& backbuffer) const
{
    constexpr int minimum = RadarDisplay::Center - RadarDisplay::Radius;
    constexpr int maximum = RadarDisplay::Center + RadarDisplay::Radius;

    backbuffer.drawLine(minimum, RadarDisplay::Center, maximum, RadarDisplay::Center, lgfx::color888(0, 20, 0));
    backbuffer.drawLine(RadarDisplay::Center, minimum, RadarDisplay::Center, maximum, lgfx::color888(0, 20, 0));
    backbuffer.drawCircle(RadarDisplay::Center, RadarDisplay::Center, (RadarDisplay::Radius / 3) * 2, lgfx::color888(0, 64, 0));
    backbuffer.drawCircle(RadarDisplay::Center, RadarDisplay::Center, RadarDisplay::Radius / 3, lgfx::color888(0, 32, 0));
    backbuffer.drawCircle(RadarDisplay::Center, RadarDisplay::Center, 3, lgfx::color888(0, 48, 0));
    backbuffer.drawCircle(RadarDisplay::Center, RadarDisplay::Center, RadarDisplay::Radius, lgfx::color888(0, 200, 0));
}

std::pair<int, int> AircraftManager::ProjectCoordinateToScreen(float predLat, float predLon) const
{
    const float dLon = predLon - lon;
    const float dLat = predLat - lat;

    const float normLon = (dLon + rad) / (2.0f * rad);
    const float normLat = (dLat + rad) / (2.0f * rad);

    const int x = static_cast<int>(normLon * RadarDisplay::ScreenSize);
    const int y = static_cast<int>(RadarDisplay::ScreenSize - (normLat * RadarDisplay::ScreenSize));

    return { x, y };
}

void AircraftManager::DrawAircraftInfo(int x, int y, const TrackedAircraft& tracked, int rotation) const
{
    const int lineHeight = tft.fontHeight() + 1;

    const auto label = RotateDisplayPoint(
        x + 5, y + 5, rotation,
        (RadarDisplay::ScreenSize - 1) / 2.0f,
        (RadarDisplay::ScreenSize - 1) / 2.0f
    );
    tft.setTextSize(1);
    tft.setTextColor(lgfx::color888(0, 128, 0));
    int textY = label.y;
    const Aircraft& state = tracked.state;
    for (const auto& option : AircraftInfoOptions) {
        if (!(infoFields & option.field)) continue;
        String text;
        switch (option.field) {
            case Callsign: text = state.callsign.isEmpty() ? "N/A" : state.callsign; break;
            case Speed: text = state.hasVelocity ? String(state.velocity) + "m/s" : "N/A"; break;
            case Altitude: text = state.hasBaroAltitude ? String(state.baroAltitude) + "m" : "N/A"; break;
            case Country: text = state.originCountry.isEmpty() ? "N/A" : state.originCountry; break;
            case Category: text = AircraftCategoryName(state.category); break;
            case TrueTrack: text = state.hasTrueTrack ? String(state.trueTrack) + "deg" : "N/A"; break;
        }
        tft.drawString(text, label.x, textY);
        textY += lineHeight;
    }
}

void AircraftManager::DrawAircraftTriangle(LGFX_Sprite& backbuffer, int x, int y, const TrackedAircraft& tracked) const
{
    const float dx = std::sin(radians(tracked.state.trueTrack));
    const float dy = -std::cos(radians(tracked.state.trueTrack));
    const float px = -dy;
    const float py = dx;

    constexpr float TRIANGLE_LENGTH = 6.0f;
    constexpr float TRIANGLE_WIDTH = 3.0f;

    const float tipX = x + dx * TRIANGLE_LENGTH;
    const float tipY = y + dy * TRIANGLE_LENGTH;
    const float leftX = x - dx * TRIANGLE_LENGTH * 0.5f + px * TRIANGLE_WIDTH * 0.5f;
    const float leftY = y - dy * TRIANGLE_LENGTH * 0.5f + py * TRIANGLE_WIDTH * 0.5f;
    const float rightX = x - dx * TRIANGLE_LENGTH * 0.5f - px * TRIANGLE_WIDTH * 0.5f;
    const float rightY = y - dy * TRIANGLE_LENGTH * 0.5f - py * TRIANGLE_WIDTH * 0.5f;

    backbuffer.fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, lgfx::color888(0, 255, 0));
}