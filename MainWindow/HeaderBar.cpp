#include "HeaderBar.h"
#include "CTSPA22SDriver.h"
#include "DataTypes.h"
#include "Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

HeaderBar::HeaderBar(QWidget *parent)
    : QFrame(parent)
{
    setupUi();
}

void HeaderBar::setupUi()
{
    setObjectName("TopHeader");
    setMinimumHeight(48);
    setMaximumHeight(60);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 14, 0);
    layout->setSpacing(8);

    // ── Logo + 标题 + 版本 ──
    auto *logo = new QLabel(QString::fromUtf8("◆"), this);
    logo->setObjectName("LogoMark");
    logo->setFixedSize(28, 28);

    auto *title = new QLabel(QString::fromUtf8("相控阵检测系统"), this);
    title->setObjectName("AppTitle");
    title->setMinimumWidth(0);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto *version = new QLabel("V1.0.0", this);
    version->setObjectName("VersionLabel");
    version->setMinimumWidth(0);
    version->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // ── 连接模式选择 ──
    auto *modeLabel = new QLabel(QString::fromUtf8("连接模式:"), this);
    modeLabel->setObjectName("HeaderInfo");
    modeLabel->setMinimumWidth(0);
    modeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QString::fromUtf8("无线 (WIFI)"), static_cast<int>(ConnectionMode::Wireless));
    m_modeCombo->addItem(QString::fromUtf8("有线 (网线)"), static_cast<int>(ConnectionMode::Wired));
    m_modeCombo->setStyleSheet(
        "QComboBox{background:#0c2135;color:#d5e9f5;border:1px solid #1d3d58;padding:2px 8px;min-width:80px;}"
        "QComboBox::drop-down{border:0;}"
        "QComboBox::down-arrow{image:none;}"
        "QComboBox QAbstractItemView{background:#0c2135;color:#d5e9f5;selection-background:#0a72d6;}"
    );

    // ── 设备状态 ──
    m_deviceLabel = new QLabel(QString::fromUtf8("● 设备连接： 未连接"), this);
    m_deviceLabel->setObjectName("DeviceOk");
    m_deviceLabel->setMinimumWidth(0);
    m_deviceLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_ipLabel = new QLabel(QString("IP: %1").arg(CTSPA22SDriver::DefaultWifiIP), this);
    m_ipLabel->setObjectName("HeaderInfo");
    m_ipLabel->setMinimumWidth(0);
    m_ipLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // ── 遥测 ──
    m_temperatureLabel = new QLabel(QString::fromUtf8("温度: --"), this);
    m_pcBatteryLabel   = new QLabel(QString::fromUtf8("PC电量: --"), this);
    m_paBatteryLabel   = new QLabel(QString::fromUtf8("PA电量: --"), this);
    for (QLabel *label : {m_temperatureLabel, m_pcBatteryLabel, m_paBatteryLabel}) {
        label->setObjectName("HeaderInfo");
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    // ── 操作按钮 ──
    auto makeBtn = [](const QString &text, const QString &bg, QWidget *p) {
        auto *b = new QPushButton(text, p);
        b->setFixedHeight(30);
        b->setMinimumWidth(72);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        b->setStyleSheet(QString(
            "QPushButton{background:%1;color:white;border:1px solid %2;border-radius:4px;"
            "padding:0 12px;font-size:13px;font-weight:600;}"
            "QPushButton:hover{background:%3;}"
            "QPushButton:disabled{background:#10222f;color:#4a6070;border-color:#1e3444;}")
            .arg(bg).arg(bg).arg(bg));
        return b;
    };

    m_connectBtn = makeBtn(QString::fromUtf8("连接设备"), "#0a6e3b", this);
    m_acquireBtn = makeBtn(QString::fromUtf8("开始采集"), "#0652a2", this);

    // ── 组装布局 ──
    layout->addWidget(logo);
    layout->addWidget(title);
    layout->addWidget(version);
    layout->addStretch(2);
    layout->addWidget(modeLabel);
    layout->addSpacing(2);
    layout->addWidget(m_modeCombo);
    layout->addSpacing(6);
    layout->addWidget(m_deviceLabel);
    layout->addSpacing(6);
    layout->addWidget(m_ipLabel);
    layout->addSpacing(6);
    layout->addWidget(m_temperatureLabel);
    layout->addWidget(m_pcBatteryLabel);
    layout->addWidget(m_paBatteryLabel);
    updatePcBattery();

    layout->addSpacing(8);
    layout->addWidget(m_connectBtn);
    layout->addWidget(m_acquireBtn);
}

void HeaderBar::setTemperature(double celsius)
{
    m_temperatureLabel->setText(QString::fromUtf8("温度: %1 °C").arg(celsius, 0, 'f', 1));
}

void HeaderBar::setPaBattery(int percent)
{
    m_paBatteryLabel->setText(QString::fromUtf8("PA电量: %1%").arg(percent));
}

void HeaderBar::updatePcBattery()
{
#ifdef Q_OS_WIN
    SYSTEM_POWER_STATUS status = {};
    if (GetSystemPowerStatus(&status)
            && status.BatteryLifePercent != 255
            && !(status.BatteryFlag & 128)) {
        m_pcBatteryLabel->setText(
            QString::fromUtf8("PC电量: %1%").arg(qBound(0, int(status.BatteryLifePercent), 100)));
        return;
    }
#endif
    m_pcBatteryLabel->setText(QString::fromUtf8("PC电量: --"));
}
