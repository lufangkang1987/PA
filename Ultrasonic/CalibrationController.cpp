#include "CalibrationController.h"
#include "CScanEngine.h"
#include "IDriver.h"
#include <QtGlobal>

CalibrationController::CalibrationController(IDriver *driver, CScanEngine *cScanEngine,
                                             QObject *parent)
    : QObject(parent), m_driver(driver), m_cScanEngine(cScanEngine)
{
}

void CalibrationController::onCalibrationRequested(int item)
{
    if (!m_driver->isConnected()) {
        emit statusMessage("校准需要先连接设备");
        return;
    }
    if (m_cScanEngine->isScanning() || m_replayActive) {
        emit statusMessage("请先退出扫查或回放状态");
        return;
    }
    if (!m_calibrating) {
        m_calibrating = true;
        m_calibrationItem = item;
        const PAParams &p = *m_params;
        if (item == 2 || item == 3) {
            emit calibrationGuideChanged(true, m_calibrationTargetPercent);
        } else {
            emit calibrationGuideChanged(false, 0);
        }
        const int centerBeam = p.scan.scanType == 0 ? 63
            : (p.scan.scanType == 1 ? (p.probe.probeCount - p.scan.eleAperture + 1) / 2
                               : p.probe.probeCount / 2);
        if (item < 3) emit beamSelectRequested(qBound(0, centerBeam, MaxBeams - 1));
        if (item == 2) m_driver->setACG(false, p);
        if (item == 3) m_driver->setTCG(false, p);
        emit statusMessage("校准已开始，取得稳定回波后再次点击完成");
        return;
    }
    if (!m_latestPacket || m_latestPacket->beamCount <= 0) {
        emit statusMessage("尚未收到有效采集数据");
        return;
    }

    const PAParams &p = *m_params;
    const int beam = qBound(0, p.rx.curBeam, m_latestPacket->beamCount - 1);
    const BeamWaveform &wave = m_latestPacket->beams[beam];
    auto pathMm = [&p](quint16 path) {
        return p.gate.gateTrace[2] ? path * p.tx.range / WaveSampleCount
                              : path * S22_SP * p.wp.lVelocity / 2000000.0;
    };
    bool ok = false;
    if (m_calibrationItem == 0) {
        if (p.wp.lVelocity <= 0 || p.tcg.realDistance <= 0.0) {
            emit statusMessage("声速或实际距离参数无效");
        } else {
            const double difference = pathMm(wave.path1) - pathMm(wave.path0);
            if (difference > 1.0) {
                emit velocityCalibrated(qRound(p.tcg.realDistance * p.wp.lVelocity / difference));
                ok = true;
            } else {
                emit statusMessage("闸门读数差异过小，无法校准声速");
            }
        }
    } else if (m_calibrationItem == 1) {
        if (p.wp.lVelocity <= 0) {
            emit statusMessage("工件声速参数无效");
        } else {
            emit probeDelayCalibrated(float((pathMm(wave.path0) - p.tcg.realDistance)
                                      * 2000.0 / p.wp.lVelocity + p.probe.probeDelay));
            ok = true;
        }
    } else if (m_calibrationItem == 2) {
        QVector<float> values(m_latestPacket->beamCount, 1.0f);
        for (int i = 0; i < m_latestPacket->beamCount; ++i) {
            const int amplitude = m_latestPacket->beams[i].amp0;
            values[i] = amplitude > 0
                ? qBound(0.0f, m_calibrationTargetPercent * 2.5f / amplitude, 256.0f)
                : 256.0f;
        }
        emit acgCalibrated(values);
        ok = true;
    } else if (m_calibrationItem == 3) {
        m_driver->setTCG(true, p);
        ok = true;
    }
    m_calibrating = false;
    m_calibrationItem = -1;
    emit calibrationGuideChanged(false, 0);
    if (ok)
        emit statusMessage("校准完成并已应用");
}

void CalibrationController::onEncoderCalibrationRequested()
{
    if (!m_driver || !m_driver->isConnected()) {
        emit statusMessage("编码器校准需要先连接设备");
        return;
    }
    if (m_cScanEngine->isScanning() || m_replayActive || m_calibrating) {
        emit statusMessage("请先退出扫查、回放或其他校准状态");
        return;
    }
    const int position = m_encoderPosition;
    if (!m_encoderCalibrating) {
        m_driver->resetEncoder(0);
        m_encoderCalibrationStart = 0;
        m_encoderCalibrating = true;
        emit statusMessage("编码器校准已开始，请移动指定距离后再次点击");
    } else {
        const int pulses = qAbs(position - m_encoderCalibrationStart);
        if (pulses > 0)
            emit coderDegCalibrated(m_params->enc.checkDistance / pulses);
        m_encoderCalibrating = false;
        emit statusMessage(QString("编码器精度：%1 mm/p")
            .arg(m_params->enc.coderDeg, 0, 'f', 4));
    }
}
