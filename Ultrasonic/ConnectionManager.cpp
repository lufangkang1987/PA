#include "ConnectionManager.h"
#include "CTSPA22SDriver.h"
#include "IDriver.h"

ConnectionManager::ConnectionManager(IDriver *driver, QObject *parent)
    : QObject(parent), m_driver(driver)
{
    connect(m_driver, &IDriver::connectionChanged, this, [this](bool connected) {
        emit connectionStateChanged(connected);
        if (!connected) {
            m_acquiring = false;
            emit acquireStateChanged(false);
        }
    });
}

void ConnectionManager::setConnectionMode(ConnectionMode mode)
{
    m_connectionMode = mode;
    const char *ip = (mode == ConnectionMode::Wireless)
                     ? CTSPA22SDriver::DefaultWifiIP
                     : CTSPA22SDriver::DefaultWiredIP;
    emit ipAddressChanged(QString::fromLatin1(ip));
}

void ConnectionManager::onConnectButtonClicked()
{
    if (!m_driver) return;
    if (m_driver->isConnected()) {
        m_driver->stopAcquisition();
        m_driver->disconnectDevice();
    } else {
        m_driver->connectDevice(m_connectionMode);
    }
}

void ConnectionManager::onAcquireButtonClicked()
{
    if (!m_driver || !m_driver->isConnected()) return;
    updateAcquire(!m_acquiring);
}

void ConnectionManager::updateAcquire(bool acquiring)
{
    m_acquiring = acquiring;
    if (m_acquiring) {
        m_driver->startAcquisition();
        emit acquireStarted();
    } else {
        m_driver->stopAcquisition();
        emit acquireStopped();
    }
    emit acquireStateChanged(m_acquiring);
}
