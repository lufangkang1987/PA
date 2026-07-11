#pragma once
#include <QObject>
#include "DataTypes.h"

/// @brief 超声仪器 Driver 统一接口
///
/// 所有仪器 Driver（USM100、CTSPA22S 等）都实现此接口，
/// 使得 MainWindow 可以无缝切换不同仪器。
///
/// @see USM100Driver  GE USM100 模拟 Driver
/// @see CTSPA22SDriver  汕头超声 CTSPA22S 真实 TCP Driver
class IDriver
{
public:
    virtual ~IDriver() = default;

    /// 将自身暴露为 QObject*，用于 connect / qobject_cast 等 Qt 操作
    virtual QObject* asQObject() = 0;

    // -------- 连接管理 --------
    virtual bool connectDevice(const QString &ip, quint16 port) = 0;
    virtual bool connectDevice(ConnectionMode mode) = 0;
    virtual void disconnectDevice() = 0;
    virtual bool isConnected() const = 0;

    // -------- 采集控制 --------
    virtual void startAcquisition() = 0;
    virtual void stopAcquisition() = 0;

    // -------- 设备控制 --------
    virtual void powerOff() = 0;

    // -------- 参数设置 --------
    virtual void setScanType(int type) = 0;
    virtual void setAnalogGain(float dB) = 0;
    virtual void setDigitalGain(float dB) = 0;
    virtual void setHighVoltage(int level) = 0;
    virtual void setPulseWidth(int width) = 0;
    virtual void setPRF(int prf) = 0;
    virtual void setRange(float range) = 0;
    virtual void setRectify(int mode) = 0;
    virtual void setFilter(int filter) = 0;
    virtual void setADataLen(int len) = 0;
    virtual void setASmooth(bool enable) = 0;
    virtual void setVideoDetect(bool enable) = 0;
    virtual void setGate(char gate, float start, float width,
                        float threshold, const QString &measureType) = 0;
    virtual void setBeamDelay() = 0;
    virtual void setCommonRDelay() = 0;
    virtual void setTFMImageProcess(double subtract, double supress, double smooth) = 0;
    virtual void resetEncoder(int idx) = 0;

    // -------- 扫查配置（供"应用法则"批量下发）--------
    virtual void setVelocity(float mps) = 0;
    virtual void setProbeGeometry(int count, float freqMHz, float pitchMm) = 0;
    virtual void setElementGeometry(int start, int end, int aperture) = 0;
    virtual void setSscanAngles(float fromDeg, float toDeg) = 0;
    virtual void setLscanAngle(float angleDeg) = 0;
    virtual void setFocusMm(float mm) = 0;
    virtual void setWedgeGeometry(bool enable, float angleDeg, int velocityMps, float heightMm) = 0;
};
