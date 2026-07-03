#pragma once
// ============================================================
// DataTypes.h — CTSPA22S 统一数据结构定义
// ============================================================
// 迁移自 CTSPA22SDoc.h (MFC 原版)，合并 Driver 层数据帧定义。
// 本文件零 Qt 依赖，可被 core/ 算法层、通信层、UI 层共用。
// ============================================================

#include <cstdint>
#include <cstring>

// ============================================================
//  常量
// ============================================================

constexpr int S22_SP         = 10;
constexpr double S22_SP_TFOCUS  = 1.25;
constexpr double S22_SP_RFOCUS  = 0.625;

constexpr int MaxBeams        = 128;
constexpr int WaveSampleCount = 400;
constexpr int MaxCScanFrames  = 2776;
constexpr int CScanWidth      = 512;
constexpr int SImageSize      = 512 * 400;     // 204800
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
    Wired    = 0,  // 有线：USB 转网口 → 仪器 IP 192.168.22.121，PC 需同网段静态 IP
    Wireless = 1   // 无线：USB 转 WIFI → 仪器热点 PA22S-xxxxxx，密码 12345678，IP 192.168.0.51
};
