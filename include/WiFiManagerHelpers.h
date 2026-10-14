#pragma once

#include <WiFiManager.h>
#include "DrawHelpers.h"

namespace WiFiManagerHelpers
{
    constexpr const char* WiFiManagerName = "MicroRadar-Setup";

    static void ConfigureWiFiManager(WiFiManager& wm, LGFX_Sprite& backbuffer, int rotation)
    {
        wm.setTitle("Micro Radar - Setup WiFi");
        wm.setCustomHeadElement("<style>body{background:#111;color:#00ff00;font-family:monospace;} div:has(> a){background:#00ff00;} a:hover{color:#111;}</style>");

        wm.setAPCallback([&backbuffer, rotation](WiFiManager* wifiManager) {
            backbuffer.fillScreen(lgfx::color888(0, 0, 0));
            backbuffer.setTextColor(lgfx::color888(0, 255, 0));

            const int lineHeight = backbuffer.fontHeight() + 10;
            const int screenSize = 240;
            backbuffer.drawCenterString("- SETUP -", screenSize / 2, screenSize / 2 - lineHeight);
            backbuffer.drawCentreString("Connect to this WiFi hotspot:", screenSize / 2, screenSize / 2);
            backbuffer.drawCenterString(WiFiManagerName, screenSize / 2, screenSize / 2 + lineHeight);
            PushDisplayFrame(backbuffer, rotation);
            }
        );
    }
}