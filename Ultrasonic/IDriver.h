#pragma once
#include "DataTypes.h"
#include <QObject>

class IDriver : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

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

    // -------- 基础参数 --------
    virtual void setScanType(int type) = 0;
    virtual void setAnalogGain(float dB) = 0;
    virtual void setDigitalGain(float dB) = 0;
    virtual void setTemperatureCompensation(bool enabled) = 0;
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

    // -------- 扫查配置（"应用法则"批量下发）--------
    virtual void setVelocity(float mps) = 0;
    virtual void setProbeGeometry(int count, float freqMHz, float pitchMm) = 0;
    virtual void setElementGeometry(int start, int end, int aperture) = 0;
    virtual void setSscanAngles(float fromDeg, float toDeg) = 0;
    virtual void setLscanAngle(float angleDeg) = 0;
    virtual void setFocusMm(float mm) = 0;
    virtual void setWedgeGeometry(bool enable, float angleDeg, int velocityMps, float heightMm) = 0;

    // -------- 遥测 --------
    virtual void queryTemperature() = 0;
    virtual void queryVoltage() = 0;

signals:
    void connectionChanged(bool connected);
    void statusChanged(const QString &status);
    void errorOccurred(const QString &error);
    void waveformReady(const QVector<double> &waveform, int beamIndex, int frameIndex, int rectifyMode);
    void multiBeamWaveformsReady(const QVector<QVector<double>> &waveforms);
    void dataPacketReady(const DataPacket &packet);
    void scanRulePositionsReady(const QVector<double> &positions);
    void gateReadingsReady(char gate, double amplitude, double path);
    void encoderPositionChanged(int position);
    void frameStatisticsChanged(int frameDiff, quint64 droppedFrames);
    void tfmImageReady(const QVector<int> &image);
    void temperatureReceived(double tempC);
    void voltageReceived(double voltage);
};
