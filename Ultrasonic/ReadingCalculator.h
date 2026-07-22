#pragma once
#include "DataTypes.h"
#include "PAParams.h"
#include <QVector>
#include <QtGlobal>

/// 闸门读数计算结果
struct GateReadings {
    double angleDegrees      = 0.0;
    double horizontalOffsetMm = 0.0;
    double aAmplitude        = 0.0;
    double aSoundPathMm      = 0.0;
    double bAmplitude        = 0.0;
    double bSoundPathMm      = 0.0;
    double cAmplitude        = 0.0;
    double cSoundPathMm      = 0.0;
    bool   alarmTriggered    = false;
};

/// 纯计算：从数据包+参数+扫查位置计算闸门读数 + 报警判定
inline GateReadings calculateReadings(const PAParams &params, const DataPacket &packet,
                                       const QVector<double> &scanRulePositions)
{
    GateReadings r;
    if (packet.beamCount <= 0) return r;

    const int beam = qBound(0, params.rx.curBeam, packet.beamCount - 1);
    const BeamWaveform &wave = packet.beams[beam];

    // ── 角度插值 ──
    r.angleDegrees = params.scan.angle;
    if (params.scan.scanType == 0) {
        const double t = packet.beamCount > 1
            ? static_cast<double>(beam) / (packet.beamCount - 1) : 0.0;
        r.angleDegrees = params.scan.angleFrom + (params.scan.angleTo - params.scan.angleFrom) * t;
    }

    // ── 楔块水平偏移 ──
    if (params.scan.scanType == 0 && params.wedge.wedgeEnable != 0
            && scanRulePositions.size() >= packet.beamCount) {
        const int centerBeam = qBound(0, 63, packet.beamCount - 1);
        r.horizontalOffsetMm = scanRulePositions[beam]
            - scanRulePositions[centerBeam];
    }

    // ── 声程转换 ──
    auto normalSoundPathMm = [&params](quint16 path) -> double {
        return path * S22_SP * params.wp.lVelocity / 2000000.0;
    };

    // ── 闸门读数 ──
    r.aAmplitude   = qMin(100.0, wave.amp0 / 2.5);
    r.aSoundPathMm = params.gate.gateTrace[2]
        ? wave.path0 * params.tx.range / WaveSampleCount
        : normalSoundPathMm(wave.path0);
    r.bAmplitude   = qMin(100.0, wave.amp1 / 2.5);
    r.bSoundPathMm = normalSoundPathMm(wave.path1);
    r.cAmplitude   = qMin(100.0, wave.amp2 / 2.5);
    r.cSoundPathMm = normalSoundPathMm(wave.path2);

    // ── 报警判定 ──
    if (params.gate.alarmSound != 0) {
        const int threshA = qRound(params.gate.gateThreshold[0] * 2.5);
        const int threshB = qRound(params.gate.gateThreshold[1] * 2.5);
        const int sound = params.gate.alarmSound;
        for (int b = 0; b < packet.beamCount; ++b) {
            if ((sound == 1 || sound == 3) && packet.beams[b].amp0 > threshA)
                { r.alarmTriggered = true; break; }
            if ((sound == 2 || sound == 3) && packet.beams[b].amp1 > threshB)
                { r.alarmTriggered = true; break; }
        }
    }

    return r;
}
