#pragma once
#include <QColor>

// ============================================================
//  PA 项目全局视觉主题常量
// ============================================================

// ── 字体名 ────────────────────────────────────────────────
namespace ThemeFont {
inline const char *const Ui   = "Microsoft YaHei";
inline const char *const Mono = "Consolas";
}

// ── 颜色 ──────────────────────────────────────────────────
namespace ThemeColor {
inline constexpr QColor DeepBg      {  7,  17,  27};
inline constexpr QColor CanvasBg    {  8,  19,  29};
inline constexpr QColor TextPrimary {207, 231, 244};
inline constexpr QColor TextDim     {166, 197, 214};
inline constexpr QColor Border      { 26,  58,  82};
inline constexpr QColor AccentBlue  { 10, 114, 214};
inline constexpr QColor Success     { 10, 110,  59};
inline constexpr QColor Danger      {139,  32,  32};
inline constexpr QColor Warning     {194,  89,  10};
inline constexpr QColor GateA       {255,  30,  30};
inline constexpr QColor GateB       {255, 200,   0};
inline constexpr QColor GateC       {200,  50, 255};
}
