#pragma once
#include "DataTypes.h"
#include <QObject>
#include <QString>

class QComboBox;
class QLabel;
class QPushButton;
class IDriver;

/// 连接管理器 — 所有网络连接/断开/模式切换逻辑从 MainWindow 分离
class ConnectionManager : public QObject
{
    Q_OBJECT
public:
    ConnectionManager(IDriver *driver, QComboBox *modeCombo, QLabel *deviceLabel,
                      QLabel *ipLabel, QPushButton *connectBtn, QPushButton *acquireBtn,
                      QObject *parent = nullptr);

    void initialize();

public slots:
    void onConnectButtonClicked();
    void onAcquireButtonClicked();
    void onModeChanged(int index);
    void onDriverConnectionChanged(bool connected);

signals:
    void statusMessage(const QString &msg);
    void acquireStarted();
    void acquireStopped();

private:
    void updateConnectionUi(bool connected);
    void updateAcquireUi(bool connected, bool acquiring);

    IDriver     *m_driver;
    QComboBox   *m_modeCombo;
    QLabel      *m_deviceLabel;
    QLabel      *m_ipLabel;
    QPushButton *m_connectBtn;
    QPushButton *m_acquireBtn;
    bool m_acquiring = false;
};
