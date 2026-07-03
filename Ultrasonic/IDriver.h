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
};
