#include "ConnectionManager.h"
#include "CTSPA22SDriver.h"
#include "IDriver.h"
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QString>

static const char *const kStyleConnect  = "#0a6e3b";
static const char *const kStyleDisconn  = "#8b2020";
static const char *const kStyleAcquire  = "#0652a2";
static const char *const kStyleAcquiring = "#c2590a";

ConnectionManager::ConnectionManager(IDriver *driver, QComboBox *modeCombo,
        QLabel *deviceLabel, QLabel *ipLabel, QPushButton *connectBtn,
        QPushButton *acquireBtn, QObject *parent)
    : QObject(parent), m_driver(driver), m_modeCombo(modeCombo),
      m_deviceLabel(deviceLabel), m_ipLabel(ipLabel),
      m_connectBtn(connectBtn), m_acquireBtn(acquireBtn)
{
    connect(m_connectBtn, &QPushButton::clicked,
            this, &ConnectionManager::onConnectButtonClicked);
    connect(m_acquireBtn, &QPushButton::clicked,
            this, &ConnectionManager::onAcquireButtonClicked);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionManager::onModeChanged);
    connect(m_driver, &IDriver::connectionChanged,
            this, &ConnectionManager::onDriverConnectionChanged);
    m_acquireBtn->setEnabled(false);
}

void ConnectionManager::initialize()
{
    auto mode = static_cast<ConnectionMode>(m_modeCombo->currentData().toInt());
    const char *ip = (mode == ConnectionMode::Wireless)
                     ? CTSPA22SDriver::DefaultWifiIP
                     : CTSPA22SDriver::DefaultWiredIP;
    m_ipLabel->setText(QString("IP: %1").arg(ip));
}

void ConnectionManager::onConnectButtonClicked()
{
    if (!m_driver) return;
    if (m_driver->isConnected()) {
        m_driver->stopAcquisition();
        m_driver->disconnectDevice();
    } else {
        auto mode = static_cast<ConnectionMode>(m_modeCombo->currentData().toInt());
        m_driver->connectDevice(mode);
    }
}

void ConnectionManager::onAcquireButtonClicked()
{
    if (!m_driver || !m_driver->isConnected()) return;
    m_acquiring = !m_acquiring;
    if (m_acquiring) {
        m_driver->startAcquisition();
        emit acquireStarted();
    } else {
        m_driver->stopAcquisition();
        emit acquireStopped();
    }
    updateAcquireUi(true, m_acquiring);
}

void ConnectionManager::onModeChanged(int /*idx*/)
{
    if (!m_driver) return;
    auto mode = static_cast<ConnectionMode>(m_modeCombo->currentData().toInt());
    const char *ip = (mode == ConnectionMode::Wireless)
                     ? CTSPA22SDriver::DefaultWifiIP
                     : CTSPA22SDriver::DefaultWiredIP;
    m_ipLabel->setText(QString("IP: %1").arg(ip));
    if (m_driver->isConnected()) {
        m_driver->stopAcquisition();
        m_driver->disconnectDevice();
        m_driver->connectDevice(mode);
    }
}

void ConnectionManager::updateConnectionUi(bool connected)
{
    m_deviceLabel->setText(connected ? "● 设备连接： 已连接" : "● 设备连接： 未连接");
    if (connected) {
        m_connectBtn->setText("断开设备");
        m_connectBtn->setStyleSheet(
            m_connectBtn->styleSheet().replace(kStyleConnect, kStyleDisconn));
        m_acquireBtn->setEnabled(true);
    } else {
        m_connectBtn->setText("连接设备");
        m_connectBtn->setStyleSheet(
            m_connectBtn->styleSheet().replace(kStyleDisconn, kStyleConnect));
        m_acquireBtn->setEnabled(false);
        m_acquiring = false;
    }
}

void ConnectionManager::updateAcquireUi(bool /*connected*/, bool acquiring)
{
    m_acquireBtn->setText(acquiring ? "停止采集" : "开始采集");
    m_acquireBtn->setStyleSheet(
        m_acquireBtn->styleSheet().replace(
            acquiring ? kStyleAcquire : kStyleAcquiring,
            acquiring ? kStyleAcquiring : kStyleAcquire));
}

void ConnectionManager::onDriverConnectionChanged(bool connected)
{
    updateConnectionUi(connected);
    if (!connected) {
        m_acquiring = false;
        updateAcquireUi(false, false);
    }
}
