#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#include "LGFX.h"
#include "WiFiManagerHelpers.h"
#include "ConfigurationWebServer.h"
#include "HttpRequestManager.h"
#include "OpenSkyAuthTokenHandler.h"
#include "AircraftManager.h"
#include "DrawHelpers.h"
#include "DisplayRotation.h"
#include "RadarGeometry.h"
#include "models/Aircraft.h"
#include "models/TrackedAircraft.h"

// Optional hard-coded Wi-Fi credentials. Leave both blank to skip pre-baking them and use the setup hotspot instead.
const char* preconfiguredWifiSsid = "";
const char* preconfiguredWifiPassword = "";

LGFX tft;
LGFX_Sprite backbuffer(&tft);
int displayRotation = 0;

WiFiManager wm;
ConfigurationWebServer configServer;
HttpRequestManager http;
OpenSkyAuthTokenHandler authHandler(http);

AircraftManager aircraftManager(configServer, authHandler, http, tft);

void setup()
{
  Serial.begin(115200);
  // delay(1000); // avoids immediate serial output being cut off - uncomment if needed

  // initialise LGFX + screen
  tft.init();
  tft.invertDisplay(true);
  tft.setBrightness(configServer.GetDisplayBrightness());

  backbuffer.setColorDepth(8);
  backbuffer.createSprite(RadarDisplay::ScreenSize, RadarDisplay::ScreenSize);

  const String storedRotation = configServer.GetStoredString("rotation");
  if (!storedRotation.isEmpty() && !ParseDisplayRotation(storedRotation.c_str(), displayRotation)) {
    Serial.println("[WARN] Invalid stored display rotation; using 0 degrees.");
  }

  // establish WiFi connection
  tft.fillScreen(lgfx::color888(0, 0, 0));
  backbuffer.fillScreen(lgfx::color888(0, 0, 0));
  PushDisplayFrame(backbuffer, displayRotation);
  tft.setTextColor(lgfx::color888(0, 255, 0));
  tft.drawCentreString("Connecting to WiFi...", RadarDisplay::ScreenSize / 2, RadarDisplay::ScreenSize / 2);

  WiFiManagerHelpers::ConfigureWiFiManager(wm, backbuffer, tft, displayRotation);

  if (strlen(preconfiguredWifiSsid) > 0) {
    WiFi.begin(preconfiguredWifiSsid, preconfiguredWifiPassword);
    WiFi.waitForConnectResult();
  }

  wm.autoConnect(WiFiManagerHelpers::WiFiManagerName);

  // begin background server for configuration
  configServer.Initialise();

  // initialise aircraft manager
  aircraftManager.Initialise();
}

void loop()
{
  // Apply async API requests here so display access stays on the drawing task.
  static bool displayAwake = true;
  const bool displayEnabled = configServer.IsDisplayEnabled();
  if (displayEnabled != displayAwake) {
    if (displayEnabled) {
      tft.wakeup();
    } else {
      tft.setBrightness(0);
      tft.sleep();
    }
    displayAwake = displayEnabled;
  }

  if (!displayAwake) {
    delay(10);
    return;
  }

  const uint8_t brightness = configServer.GetDisplayBrightness();
  if (tft.getBrightness() != brightness) tft.setBrightness(brightness);

  aircraftManager.Update();

  // draw cycle
  backbuffer.fillScreen(lgfx::color888(0, 0, 0));

  String renderScanlines = configServer.GetStoredString("scanline");
  if (renderScanlines.isEmpty() || renderScanlines == "true") {
    const float sweepAngle = millis() / 3000.0f;
    DrawScanLines(backbuffer,
      RadarDisplay::Center,
      RadarDisplay::Center,
      RadarDisplay::Center + (std::cos(sweepAngle) * RadarDisplay::Radius),
      RadarDisplay::Center + (std::sin(sweepAngle) * RadarDisplay::Radius),
      20, 128, 5
    );
  }

  aircraftManager.Draw(backbuffer);
  PushDisplayFrame(backbuffer, displayRotation);
  aircraftManager.DrawInfo(displayRotation);
}
