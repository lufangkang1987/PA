#pragma once
#include "PAParams.h"
#include <QObject>
#include <QTimer>

class IDriver;

/// 参数下发中间层 — 解耦 ParamPage(UI) 和 IDriver(硬件)
///
/// 简单参数（增益/PRF/范围等）→ 立即透传到 IDriver
/// 闸门参数                → 50ms debounce 后下发
/// 几何参数                → 批量缓存，applyLaw() 统一发送
class ParameterDispatcher : public QObject
{
    Q_OBJECT
public:
    explicit ParameterDispatcher(QObject *parent = nullptr);
    void setDriver(IDriver *driver);

    bool isConnected() const;

    // 简单参数 — 立即下发
    void setAnalogGain(float dB);
    void setDigitalGain(float dB);
    void setTemperatureCompensation(bool on);
    void setHighVoltage(int level);
    void setPulseWidth(int width);
    void setPRF(int prf);
    void setRange(float range);
    void setCurrentBeam(int beam);
    void setRectify(int mode);
    void setFilter(int filter);
    void setADataLen(int len);
    void setASmooth(bool enable);
    void setVideoDetect(bool enable);

    // 闸门参数 — debounce 下发
    void setGate(char gate, float start, float width, float threshold, int measureType);

    // 几何参数（"应用法则"批量下发）
    void applyLaw(const PAParams &params);

private:
    void doSetGate();
    IDriver *m_driver = nullptr;

    // 闸门 debounce
    QTimer  m_gateTimer;
    char    m_pendingGate = 'A';
    float   m_pendingStart = 0;
    float   m_pendingWidth = 0;
    float   m_pendingThresh = 0;
    int     m_pendingMeasure = 0;
    bool    m_gatePending = false;
};
