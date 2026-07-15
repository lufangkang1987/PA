#pragma once
// ============================================================
// DataTypes.h — CTSPA22S 统一数据结构定义
// ============================================================
// 迁移自 CTSPA22SDoc.h (MFC 原版)，合并 Driver 层数据帧定义。
// 本文件零 Qt 依赖，可被 core/ 算法层、通信层、UI 层共用。
// ============================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// ============================================================
//  常量
// ============================================================

constexpr int S22_SP         = 10;
constexpr int MaxBeams        = 128;
constexpr int WaveSampleCount = 400;
constexpr int MaxCScanFrames  = 2776;
constexpr int CScanWidth         = 512;
constexpr int CScanLinesPerPage   = 925;               // 2775 / 3 (同 MFC 单页行数)
constexpr int SImageSize         = 512 * 400;          // 204800
constexpr int TFMImageSize    = 256 * 256;     // 65536

// ============================================================
//  编码器计数
// ============================================================

struct Encoder
{
    uint32_t fwd = 0;
    uint32_t rvs = 0;
};

// ============================================================
//  WAVEFORM — 单声束 A 波数据包（packed，与硬件二进制对齐）
// ============================================================

#pragma pack(push, 1)
struct Waveform
{
    uint8_t  waveP[400] = {};   // 0-399: A 波采样点
    uint16_t frame      = 0;    // 400-401: 帧序号
    uint8_t  ch         = 0;    // 402: 通道号
    uint16_t path0      = 0;    // 403-404: A 闸门声程
    uint8_t  amp0       = 0;    // 405: A 闸门幅度
    uint16_t path1      = 0;    // 406-407: B 闸门声程
    uint8_t  amp1       = 0;    // 408: B 闸门幅度
    uint16_t path2      = 0;    // 409-410: C 闸门声程
    uint8_t  amp2       = 0;    // 411: C 闸门幅度
    Encoder  enc[2]     = {};   // 412-427: 编码器
    uint8_t  rev[84]    = {};   // 428-511: 保留
    uint8_t  rev2[512]  = {};   // 512-1023: 扩展(TFM兼容)
};
#pragma pack(pop)

// 编译期验证 layout 与 MFC 原版一致
static_assert(sizeof(Waveform) == 1024, "Waveform must be 1024 bytes");

// ============================================================
//  声束几何描述
// ============================================================

struct BeamDesc
{
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

// ============================================================
//  扫查法则（硬件回读的声束位置/角度）
// ============================================================

struct ScanRule
{
    double x   = 0.0;   // 声束入射点横坐标
    double ang = 0.0;   // 声束角度
};

// ============================================================
//  上层声束波形（取值展开，不做二进制对齐）
// ============================================================

struct BeamWaveform
{
    uint8_t  waveP[400] = {};
    uint16_t frame      = 0;
    uint8_t  channel    = 0;
    uint16_t path0      = 0;    // A 闸门声程
    uint8_t  amp0       = 0;    // A 闸门幅度
    uint16_t path1      = 0;    // B 闸门声程
    uint8_t  amp1       = 0;    // B 闸门幅度
    uint16_t path2      = 0;    // C 闸门声程
    uint8_t  amp2       = 0;    // C 闸门幅度
    uint32_t encFwd     = 0;    // 编码器正向脉冲
    uint32_t encRvs     = 0;    // 编码器反向脉冲
};

// ============================================================
//  一帧完整采集数据（128 声束）
// ============================================================

struct DataPacket
{
    BeamWaveform beams[MaxBeams];
    int          beamCount  = 0;
    uint16_t     frameIndex = 0;
};

// ============================================================
//  A 扫描数据全集（采集归档用）
// ============================================================

struct AScanData
{
    ScanRule rules[MaxBeams];
    Waveform AWave[MaxCScanFrames][MaxBeams];
};

// ============================================================
//  连接方式（来自使用说明书 V3 第 4.2 节）
// ============================================================

enum class ConnectionMode {
    Wired    = 0,
    Wireless = 1
};

// ============================================================
//  共享软件成像算法（BScanWidget + CScanEngine 共用）
// ============================================================
// 输入 waveP[beamCount][400] + rules[beamCount]，输出 SImage[512*400]
inline void softwareImaging(const uint8_t *const *waveP, const ScanRule *rules,
                            int beamCount, uint8_t *outImage)
{
    std::fill_n(outImage, SImageSize, uint8_t(0xFF));
    if (beamCount < 2) return;

    double cosA[MaxBeams], sinA[MaxBeams];
    for (int b = 0; b < beamCount; ++b) {
        const double rad = rules[b].ang * 3.14159265358979323846 / 180.0;
        cosA[b] = std::cos(rad);
        sinA[b] = std::sin(rad);
    }

    uint8_t *out = outImage;
    for (int y = 399; y >= 0; --y) {
        int b = 0;
        for (int x = 0; x < CScanWidth; ++x, ++out) {
            double x1 = 0.0;
            for (; b < beamCount; ++b) {
                x1 = (x - rules[b].x) * cosA[b] - y * sinA[b];
                if (x1 < 0.0) break;
            }
            if (b == 0 || b == beamCount) continue;

            const double y1 = y * cosA[b] + (x - rules[b].x) * sinA[b];
            const double y0 = y * cosA[b - 1] + (x - rules[b - 1].x) * sinA[b - 1];
            const int n0 = int(y0 + 1.0);
            const int n1 = int(y1 + 1.0);
            if (n0 < 0 || n1 < 0 || n0 >= WaveSampleCount || n1 >= WaveSampleCount)
                continue;

            const double x0 = (x - rules[b - 1].x) * cosA[b - 1] - y * sinA[b - 1];
            const double d = x0 - x1;
            const int v0 = waveP[b - 1][n0];
            const int v1 = waveP[b][n1];
            int value = v1;
            if (x0 < 0.25 * d) value = v0;
            else if (x0 < 0.75 * d) value = (v0 + v1) / 2;
            *out = uint8_t(std::min(value, 250));
        }
    }
}

