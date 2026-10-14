#pragma once

#include "LGFX.h"

inline void PushDisplayFrame(LGFX_Sprite& buf, int rotation)
{
    if (rotation == 0) {
        buf.pushSprite(0, 0);
    } else {
        const float centreX = (buf.width() - 1) / 2.0f;
        const float centreY = (buf.height() - 1) / 2.0f;
        buf.setPivot(centreX, centreY);
        buf.pushRotateZoom(centreX, centreY, rotation, 1.0f, 1.0f);
    }
}

void DrawScanLines(LGFX_Sprite& buf, const int x0, const int y0, const int x1, const int y1, const int thickness, const int trailBrightness, const int spacing)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrt(dx * dx + dy * dy);

    // perpendicular unit vector
    float px = -dy / len;
    float py = dx / len;

    for (int i = 0; i <= thickness; i++) {
        // 1.0 at centre, 0.0 at edges
        float t = i / (float)(thickness);
        uint8_t brightness = (uint8_t)(t * trailBrightness);

        buf.drawLine(
            x0, y0,
            x1 + (px * (i * spacing)), y1 + (py * (i * spacing)),
            lgfx::color888(0, brightness, 0)
        );
    }

    buf.drawLine(
        x0, y0,
        x1 + (px * (thickness * spacing)), y1 + (py * (thickness * spacing)),
        lgfx::color888(0, 200, 0)
    );
}