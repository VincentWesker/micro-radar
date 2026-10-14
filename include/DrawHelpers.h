#pragma once

#include <cmath>
#include "LGFX.h"
#include "RadarGeometry.h"

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

void DrawScanLines(LGFX_Sprite& buf, const int x0, const int y0, const float x1, const float y1, const int thickness, const int trailBrightness, const int spacing)
{
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float radius = std::sqrt(dx * dx + dy * dy);
    if (radius == 0.0f || thickness <= 0) return;

    const float sweepAngle = std::atan2(dy, dx);
    const float trailAngle = (thickness * spacing) / radius;

    for (int i = 0; i < thickness; ++i) {
        const float progress = (i + 1) / static_cast<float>(thickness);
        const float angle = sweepAngle - trailAngle * (1.0f - progress);
        const int brightness = static_cast<int>(progress * trailBrightness);

        buf.drawLine(
            x0, y0,
            x0 + std::cos(angle) * radius,
            y0 + std::sin(angle) * radius,
            lgfx::color888(0, brightness, 0)
        );
    }

    buf.drawLine(
        x0, y0, x1, y1,
        lgfx::color888(0, 220, 0)
    );
}
