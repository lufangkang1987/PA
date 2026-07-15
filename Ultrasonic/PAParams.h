#pragma once
#include "DataTypes.h"
#include <QVector>
#include <QtGlobal>

// ── 参数值结构（镜像 CTSPA22S PARTNODE + PARANODE 全部字段）──
// BeamDesc 已在 DataTypes.h 中定义
struct PAParams
{
    // ════════════════════════════════════════════════════════
    // 发射
    // ════════════════════════════════════════════════════════
    int   highVoltage    = 0;     // 0=110V 1=40V 2=20V
    int   pulseWidth     = 100;   // ns
    int   prf            = 2000;  // Hz
    float range          = 10.0f; // mm
    int   tempCorrect    = 1;     // 0=关 1=开
    int   aDataLen       = 2;     // 0=100 1=200 2=400

    // ════════════════════════════════════════════════════════
    // 接收
    // ════════════════════════════════════════════════════════
    float aGain          = 18.0f; // dB  模拟增益
    float dGain          = 0.0f;  // dB  数字增益
    int   curBeam        = 0;
    int   rectify        = 0;     // 0=全波 1=正半波 2=负半波 3=RF
    int   filter         = 0;     // 0-15
    int   video          = 2;     // 0-4=视频检波 5=平滑

    // ════════════════════════════════════════════════════════
    // 闸门
    // ════════════════════════════════════════════════════════
    int   gateSelect     = 0;     // 0=A 1=B 2=C
    float gateStart[3]   = {2.5f, 6.2f, 0.5f};
    float gateWidth[3]   = {4.0f, 3.0f, 3.0f};
    float gateThreshold[3] = {40.0f, 30.0f, 30.0f};
    int   gateMeasure[3] = {0, 0, 0};   // 0=峰值 1=前沿
    int   gateAlarm[3]   = {1, 1, 1};   // 0=关 1=开 (A/B/C 独立报警，默认全开与MFC一致)
    int   gateTrace[3]   = {0, 0, 0};   // 0=关 1=开 (A/B/C 独立跟踪)
    int   alarmSound     = 0;     // 0=关 1=A门 2=B门 3=AB门

    // ════════════════════════════════════════════════════════
    // 探头
    // ════════════════════════════════════════════════════════
    int   probeType      = 0;     // 0=自定义 1=2.5L16 2=5.0S64
    float probeFreq      = 5.0f;  // MHz
    int   probeCount     = 64;
    float probePitch     = 0.60f; // mm
    float probeDelay     = 0.0f;  // us  探头延迟

    // ════════════════════════════════════════════════════════
    // 楔块
    // ════════════════════════════════════════════════════════
    int   wedgeEnable    = 1;
    int   wedgeType      = 0;     // 0=自定义 1=GW-PA
    float wedgeAngle     = 0.0f;  // deg
    int   wedgeVelocity  = 2337;  // m/s
    float wedgeHeight    = 20.0f; // mm

    // ════════════════════════════════════════════════════════
    // 工件
    // ════════════════════════════════════════════════════════
    int   material       = 0;     // 0=钢纵波 1=钢横波
    int   lVelocity      = 5900;  // m/s  纵波声速
    int   sVelocity      = 3230;  // m/s  横波声速
    int   traceEnable    = 0;     // 0=关 1=开
    int   diameter       = 0;     // mm   工件直径（管材）

    // ════════════════════════════════════════════════════════
    // 扫查
    // ════════════════════════════════════════════════════════
    int   scanType       = 0;     // 0=S扫 1=L扫 2=CL扫 3=TFM
    int   eleStart       = 1;
    int   eleAperture    = 8;
    int   eleEnd         = 64;
    float angleFrom      = 0.0f;  // deg (S扫起始角度)
    float angleTo        = 45.0f; // deg (S扫结束角度)
    float angle          = 0.0f;  // deg (L扫角度)
    float focus          = 5.0f;  // mm
    float innerR         = 10.0f; // mm (CL扫内径)
    float outerR         = 30.0f; // mm (CL扫外径)

    // ════════════════════════════════════════════════════════
    // TFM 参数
    // ════════════════════════════════════════════════════════
    float dimX           = 256.0f;// mm
    float dimY           = 256.0f;// mm
    float offsetX        = -128.0f;
    float offsetY        = 0.0f;
    float pixelSize      = 0.5f;  // mm/pixel
    float pieceThickness = 50.0f; // mm  工件厚度
    float tfmDGain       = 0.0f;  // dB  TFM 数字增益
    int   tfmSmooth      = 0;     // TFM 平滑
    int   parRestrainH16 = 0;     // 并行约束高16位
    int   parRestrainL16 = 0;     // 并行约束低16位

    // ════════════════════════════════════════════════════════
    // TCG / 校准
    // ════════════════════════════════════════════════════════
    float tcgX[6]        = {2.0f, 2.6f, 3.6f, 4.6f, 5.6f, 6.6f};
    float tcgRatio[6]    = {1.0f, 1.7f, 2.1f, 2.0f, 5.6f, 6.0f};
    float tcgCoeff       = 0.0f;  // TCG系数
    int   tcgStart       = 0;     // TCG起始点
    int   tcgEnd         = 0;     // TCG结束点
    int   acgSwitch      = 0;     // 0=关 1=ACG
    int   tcgSwitch      = 0;     // 0=关 1=TCG
    int   calibItem      = 0;     // 0=声速 1=声束延迟 2=ACG 3=TCG
    int   calibEnable    = 0;     // 0=关闭 1=ACG
    float realDistance   = 100.0f;// mm
    float beamDelay      = 0.0f;  // us

    // ── TCG/ACG 大数组 ──
    float acgValue[128]  = {};           // ACG 各声束增益值
    short tcgPointX[10][128]  = {};      // TCG 参考点 X 坐标 (10点×128声束)
    float tcgPointValue[10][128] = {};   // TCG 参考点增益值

    // ════════════════════════════════════════════════════════
    // 成像 (C扫)
    // ════════════════════════════════════════════════════════
    int   imgLineX1      = 150;
    int   imgLineX2      = 350;
    int   imgLineY1      = 180;
    int   imgLineY2      = 250;
    float degPerPoint    = 0.5f;  // mm/d

    // ════════════════════════════════════════════════════════
    // 编码器
    // ════════════════════════════════════════════════════════
    int   direction      = 0;     // 0=正向 1=反向
    float coderDeg       = 0.05f; // mm/p
    float checkDistance  = 50.0f; // mm
    int   circleDeg      = 0;     // 周数

    // ════════════════════════════════════════════════════════
    // 成像扫描范围
    // ════════════════════════════════════════════════════════
    float imgSpanStart   = 0.0f;  // 成像起始角度/位置
    float imgSpanEnd     = 0.0f;  // 成像结束角度/位置

    // ════════════════════════════════════════════════════════
    // 分析 (C扫回放)
    // ════════════════════════════════════════════════════════
    int   anaLineX1      = 110;
    int   anaLineX2      = 140;
    int   anaLineY1      = 100;
    int   anaLineY2      = 120;

    // ════════════════════════════════════════════════════════
    // 全局状态（对应 PARANODE 外层）
    // ════════════════════════════════════════════════════════
    int   readNum        = 0;     // C扫读数帧计数
    int   beamCount      = 128;   // 实际声束数
    int   tempBeamCount  = 128;   // 存储的检测声束数
    BeamDesc beams[128]  = {};    // 声束描述符
};

// ============================================================
//  共享扫查法则计算（BScanWidget + CScanEngine 共用）
// ============================================================
inline int computeScanRules(const PAParams &params,
                            const QVector<double> *rulePositions,
                            const QVector<ScanRule> *explicitRules,
                            ScanRule *rulesOut,
                            float *imgSpanStart = nullptr,
                            float *imgSpanEnd = nullptr)
{
    const int count = qBound(1, params.beamCount, MaxBeams);
    if (explicitRules && explicitRules->size() >= count) {
        std::copy_n(explicitRules->cbegin(), count, rulesOut);
        return count;
    }
    const bool hasPositions = rulePositions && rulePositions->size() >= count;

    if (params.scanType == 0) {
        const double angleRange = params.angleTo - params.angleFrom;
        for (int b = 0; b < count; ++b) {
            const double t = count > 1 ? double(b) / (count - 1) : 0.0;
            rulesOut[b].ang = params.angleFrom + angleRange * t;
            if (!hasPositions) {
                rulesOut[b].x = qBound(0.0, 255.0 + (t - 0.5) * 400.0, 511.0);
            } else if (params.angleFrom > -0.01f) {
                rulesOut[b].x = 55.0 + ((*rulePositions)[b] - (*rulePositions)[0])
                    / std::max(0.001, double(params.range)) * 400.0;
            } else if (params.angleTo < 0.01f) {
                rulesOut[b].x = 455.0 + ((*rulePositions)[b] - (*rulePositions)[0])
                    / std::max(0.001, double(params.range)) * 400.0;
            } else {
                rulesOut[b].x = 255.0
                    + ((*rulePositions)[b] - ((*rulePositions)[0] + (*rulePositions)[count - 1]) / 2.0)
                        / std::max(0.001, double(params.range)) * 400.0
                    + (std::abs(params.angleFrom) - std::abs(params.angleTo)) * 200.0 / 90.0;
                rulesOut[b].x = std::min(rulesOut[b].x, 455.0
                    + ((*rulePositions)[b] - (*rulePositions)[0])
                        / std::max(0.001, double(params.range)) * 400.0);
            }
        }
    } else if (params.scanType == 1) {
        const double angle = params.angle;
        const double usable = std::max(1.0, 512.0 - 400.0 * std::sin(angle * 3.14159265358979323846 / 180.0));
        for (int b = 0; b < count; ++b) {
            const double t = count > 1 ? double(b) / (count - 1) : 0.0;
            rulesOut[b].x = usable * t;
            if (hasPositions && angle > 0.01)
                rulesOut[b].x = ((*rulePositions)[b] - (*rulePositions)[0])
                    / std::max(0.001, double(params.range)) * 400.0;
            rulesOut[b].ang = angle;
        }
    } else {
        for (int b = 0; b < count; ++b) {
            const double t = count > 1 ? double(b) / (count - 1) : 0.0;
            rulesOut[b].x = 511.0 * t;
            rulesOut[b].ang = params.angle;
        }
    }

    if (imgSpanStart && imgSpanEnd) {
        if (params.scanType == 0) {
            const double dotMm = params.range / 400.0;
            *imgSpanStart = float(dotMm * (0.0 - rulesOut[0].x));
            *imgSpanEnd   = float(dotMm * (510.0 - rulesOut[0].x));
        } else if (params.scanType == 1) {
            const float halfMm = params.probePitch * (params.probeCount - params.eleAperture) / 2.0f;
            *imgSpanStart = -halfMm;
            *imgSpanEnd   =  halfMm;
        } else {
            *imgSpanStart = -180.0f;
            *imgSpanEnd   = float(180.0 - 360.0 / std::max(1, params.probeCount));
        }
    }
    return count;
}
