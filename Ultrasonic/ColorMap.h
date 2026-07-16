#pragma once
#include <QColor>

/// 超声幅度 → RGB 颜色映射（5 段 HSV，移植自 MFC Value2Color4）
inline QRgb amplitudeToColor(int value)
{
    double h, s, v;
    if (value <= 60) {
        h = 240.0 - 24.0 / 61.0 * value;
        s = 73.0 / 61.0 * value;
        v = 100.0 - 51.0 / 61.0 * value;
    } else if (value <= 102) {
        h = 216.0 - 75.0 / 42.0 * (value - 61);
        s = 73.0 + 7.0 / 42.0 * (value - 61);
        v = 49.0 + 6.0 / 42.0 * (value - 61);
    } else if (value <= 179) {
        h = 141.0 - 84.0 / 76.0 * (value - 103);
        s = 86.0 - 15.0 / 76.0 * (value - 103);
        v = 55.0 + 36.0 / 76.0 * (value - 103);
    } else if (value <= 230) {
        h = 57.0 - 56.0 / 50.0 * (value - 180);
        s = 71.0 + 5.0 / 50.0 * (value - 180);
        v = 91.0 - 9.0 / 50.0 * (value - 180);
    } else {
        h = 1.0;
        s = 86.0 + 13.0 / 25.0 * (value - 231);
        v = 82.0 + 17.0 / 25.0 * (value - 231);
    }
    return QColor::fromHsvF(h / 360.0, s / 100.0, v / 100.0).rgb();
}
