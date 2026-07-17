#pragma once
#include "DataTypes.h"
#include <QObject>
#include <QString>

class IDriver;

/// 连接管理器 — 纯业务逻辑，通过信号与 UI 通信
///
/// 依赖：IDriver（同层）
/// 不依赖任何 Pages/ 或 Widgets/ 层，不持有任何 QWidget 指针
class ConnectionManager : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionManager(IDriver *driver, QObject *parent = nullptr);

public slots:
    void onConnectButtonClicked();
    void onAcquireButtonClicked();
    void setConnectionMode(ConnectionMode mode);

signals:
    void statusMessage(const QString &msg);
    void connectionStateChanged(bool connected);
    void acquireStateChanged(bool acquiring);
    void ipAddressChanged(const QString &ip);
    void acquireStarted();
    void acquireStopped();

private:
    void updateAcquire(bool acquiring);

    IDriver       *m_driver = nullptr;
    ConnectionMode m_connectionMode = ConnectionMode::Wireless;
    bool           m_acquiring = false;
};
