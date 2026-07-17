#pragma once
#include "DataTypes.h"
#include "PAParams.h"
#include <memory>
#include <QObject>

class CScanEngine;
class IDriver;

/// 校准状态机 — 纯业务逻辑，通过信号与 UI 通信
///
/// 依赖：IDriver + CScanEngine（同层）+ PAParams（数据层）
/// 不依赖任何 Pages/ 或 Widgets/ 层
class CalibrationController : public QObject
{
    Q_OBJECT
public:
    CalibrationController(IDriver *driver, CScanEngine *cScanEngine,
                          QObject *parent = nullptr);

    bool isCalibrating() const { return m_calibrating; }
    bool isEncoderCalibrating() const { return m_encoderCalibrating; }

    void setParams(const PAParams *params) { m_params = params; }
    void setLatestPacket(std::shared_ptr<DataPacket> pkt) { m_latestPacket = pkt; }
    void setCalibrationTargetPercent(int pct) { m_calibrationTargetPercent = pct; }

public slots:
    void onCalibrationRequested(int item);
    void onEncoderCalibrationRequested();
    void setReplayActive(bool active) { m_replayActive = active; }
    void setEncoderPosition(int pos) { m_encoderPosition = pos; }

signals:
    void statusMessage(const QString &msg);

    // 校准结果 → UI 层（MainWindow 连接）
    void calibrationGuideChanged(bool visible, int targetPercent);
    void beamSelectRequested(int beam);
    void velocityCalibrated(int velocityMps);
    void probeDelayCalibrated(float delayUs);
    void acgCalibrated(const QVector<float> &values);
    void coderDegCalibrated(float mmPerPulse);

private:
    IDriver      *m_driver = nullptr;
    CScanEngine  *m_cScanEngine = nullptr;
    const PAParams *m_params = nullptr;

    std::shared_ptr<DataPacket> m_latestPacket;
    bool m_calibrating = false;
    int  m_calibrationItem = -1;
    int  m_calibrationTargetPercent = 80;
    bool m_encoderCalibrating = false;
    int  m_encoderCalibrationStart = 0;
    bool m_replayActive = false;
    int  m_encoderPosition = 0;
};
