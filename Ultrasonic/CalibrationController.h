#pragma once
#include "DataTypes.h"
#include "PAParams.h"
#include <memory>
#include <QObject>

class CScanEngine;
class HomePage;
class IDriver;
class ParamPage;

/// 校准状态机 — 从 MainWindow 分离的纯业务逻辑
class CalibrationController : public QObject
{
    Q_OBJECT
public:
    CalibrationController(IDriver *driver, ParamPage *paramPage,
                          CScanEngine *cScanEngine, HomePage *homePage,
                          QObject *parent = nullptr);

    bool isCalibrating() const { return m_calibrating; }
    bool isEncoderCalibrating() const { return m_encoderCalibrating; }

    /// 存储最新数据包（shared_ptr，避免拷贝 53KB）
    void setLatestPacket(std::shared_ptr<DataPacket> pkt) { m_latestPacket = pkt; }

public slots:
    void onCalibrationRequested(int item);
    void onEncoderCalibrationRequested();

    /// 接收回放状态（替代 AppState::replayState）
    void setReplayActive(bool active) { m_replayActive = active; }
    /// 接收编码器位置（替代 AppState::encoderCount）
    void setEncoderPosition(int pos) { m_encoderPosition = pos; }

signals:
    void statusMessage(const QString &msg);

private:
    IDriver      *m_driver;
    ParamPage    *m_paramPage;
    CScanEngine  *m_cScanEngine;
    HomePage     *m_homePage;

    std::shared_ptr<DataPacket> m_latestPacket;
    bool m_calibrating = false;
    int  m_calibrationItem = -1;
    int  m_calibrationTargetPercent = 80;
    bool m_encoderCalibrating = false;
    int  m_encoderCalibrationStart = 0;
    bool m_replayActive = false;
    int  m_encoderPosition = 0;
};
