#include "MainWindow.h"
#include "HomePage.h"
#include "ParamPage.h"
#include "IDriver.h"
#include "CTSPA22SDriver.h"
#include "AppState.h"
#include <QTabWidget>
#include <QWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QComboBox>
#include <QCloseEvent>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle("相控阵检测系统");
    resize(1500, 920);
    setMinimumSize(1180, 720);

    auto *shell = new QWidget(this);
    auto *root = new QVBoxLayout(shell);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QFrame(shell);
    header->setObjectName("TopHeader");
    header->setFixedHeight(54);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 0, 18, 0);
    headerLayout->setSpacing(14);

    auto *logo = new QLabel("◆", header);
    logo->setObjectName("LogoMark");
    auto *title = new QLabel("相控阵检测系统", header);
    title->setObjectName("AppTitle");
    auto *version = new QLabel("V1.0.0", header);
    version->setObjectName("VersionLabel");

    // 连接模式选择
    auto *modeLabel = new QLabel("连接模式:", header);
    modeLabel->setObjectName("HeaderInfo");
    m_modeCombo = new QComboBox(header);
    m_modeCombo->addItem("无线 (WIFI)", static_cast<int>(ConnectionMode::Wireless));
    m_modeCombo->addItem("有线 (网线)", static_cast<int>(ConnectionMode::Wired));
    m_modeCombo->setStyleSheet(
        "QComboBox{background:#0c2135;color:#d5e9f5;border:1px solid #1d3d58;padding:2px 8px;min-width:100px;}"
        "QComboBox::drop-down{border:0;}"
        "QComboBox::down-arrow{image:none;}"
        "QComboBox QAbstractItemView{background:#0c2135;color:#d5e9f5;selection-background:#0a72d6;}"
    );

    m_deviceLabel = new QLabel("● 设备连接： 未连接", header);
    m_deviceLabel->setObjectName("DeviceOk");
    m_ipLabel = new QLabel("IP: 192.168.0.51", header);
    m_ipLabel->setObjectName("HeaderInfo");

    headerLayout->addWidget(logo);
    headerLayout->addWidget(title);
    headerLayout->addWidget(version);
    headerLayout->addStretch();
    headerLayout->addWidget(modeLabel);
    headerLayout->addSpacing(4);
    headerLayout->addWidget(m_modeCombo);
    headerLayout->addSpacing(16);
    headerLayout->addWidget(m_deviceLabel);
    headerLayout->addSpacing(20);
    headerLayout->addWidget(m_ipLabel);

    // ────── 操作按钮 ──────
    auto makeBtn = [](const QString &text, const QString &bg, QWidget *p) {
        auto *b = new QPushButton(text, p);
        b->setFixedHeight(32);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton{background:%1;color:white;border:1px solid %2;border-radius:4px;"
            "padding:0 18px;font-size:13px;font-weight:600;}"
            "QPushButton:hover{background:%3;}"
            "QPushButton:disabled{background:#10222f;color:#4a6070;border-color:#1e3444;}")
            .arg(bg).arg(bg).arg(bg));
        return b;
    };

    m_connectBtn = makeBtn("连接设备", "#0a6e3b", header);
    m_acquireBtn = makeBtn("开始采集", "#0652a2", header);

    headerLayout->addSpacing(20);
    headerLayout->addWidget(m_connectBtn);
    headerLayout->addWidget(m_acquireBtn);

    root->addWidget(header);

    m_tabs = new QTabWidget(shell);
    m_tabs->setDocumentMode(true);
    root->addWidget(m_tabs, 1);
    setCentralWidget(shell);

    m_homePage = new HomePage;
    m_paramPage = new ParamPage;
    m_tabs->addTab(m_homePage, "主页");
    m_tabs->addTab(m_paramPage, "参数设置");
    m_tabs->addTab(new QWidget, "数据管理");
    m_tabs->addTab(new QWidget, "报告生成");
    m_tabs->addTab(new QWidget, "系统设置");

    m_driver = new CTSPA22SDriver(this);
    m_homePage->setDriver(m_driver);

    wireDriverSignals();

    // ────── 连接按钮 ──────
    connect(m_connectBtn, &QPushButton::clicked, this, [this] {
        if (!m_driver) return;

        if (m_driver->isConnected()) {
            // 断开
            m_driver->stopAcquisition();
            m_driver->disconnectDevice();
        } else {
            // 连接
            auto mode = static_cast<ConnectionMode>(
                m_modeCombo->currentData().toInt());
            m_driver->connectDevice(mode);
        }
    });

    // ────── 采集按钮 ──────
    connect(m_acquireBtn, &QPushButton::clicked, this, [this] {
        if (!m_driver || !m_driver->isConnected()) return;

        bool acquiring = m_acquireBtn->property("acquiring").toBool();
        if (acquiring) {
            m_driver->stopAcquisition();
            m_acquireBtn->setText("开始采集");
            m_acquireBtn->setStyleSheet(m_acquireBtn->styleSheet().replace("#0652a2", "#0652a2"));
        } else {
            m_driver->startAcquisition();
            m_acquireBtn->setText("停止采集");
            m_acquireBtn->setStyleSheet(
                m_acquireBtn->styleSheet().replace("#0652a2", "#c2590a"));
        }
        m_acquireBtn->setProperty("acquiring", !acquiring);
    });

    // 初始状态
    m_acquireBtn->setEnabled(false);
    m_acquireBtn->setProperty("acquiring", false);

    // 切换连接模式 → 自动断开+重连
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int /*idx*/) {
        auto mode = static_cast<ConnectionMode>(
            m_modeCombo->currentData().toInt());
        AppState::instance()->setConnectionMode(static_cast<int>(mode));

        // 更新 IP 显示
        const char *ip = (mode == ConnectionMode::Wireless)
                         ? CTSPA22SDriver::DefaultWifiIP
                         : CTSPA22SDriver::DefaultWiredIP;
        m_ipLabel->setText(QString("IP: %1").arg(ip));

        // 如果当前连接中，断开后用新模式重连
        if (m_driver && m_driver->isConnected()) {
            m_driver->stopAcquisition();
            m_driver->disconnectDevice();
            m_driver->connectDevice(mode);
        }
    });

    statusBar()->showMessage("系统就绪");

    setStyleSheet(R"(
        QMainWindow { background:#06101a; }
        #TopHeader {
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #020812, stop:.45 #06131f, stop:1 #020812);
            border-bottom:1px solid #112b40;
        }
        #LogoMark { color:#168cff; font-size:24px; font-weight:900; }
        #AppTitle { color:#f2fbff; font-size:20px; font-weight:700; }
        #VersionLabel { color:#b6c9d6; font-size:13px; padding-left:8px; }
        #DeviceOk { color:#27ff49; font-size:13px; }
        #HeaderInfo { color:#d2e0eb; font-size:13px; }
        QTabWidget::pane { border:0; top:-1px; background:#07121d; }
        QTabBar { background:#07121d; }
        QTabBar::tab {
            min-width:108px;
            min-height:44px;
            background:#0c2135;
            color:#d5e9f5;
            padding:0 20px;
            border:1px solid #1d3d58;
            border-left:0;
            font-size:15px;
        }
        QTabBar::tab:selected {
            background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #064d96, stop:1 #0a72d6);
            color:white;
            border-color:#0b73d8;
        }
        QTabBar::tab:hover { background:#123456; color:white; }
        QStatusBar { background:#06101a; color:#9dcfe8; border-top:1px solid #102a3d; }
    )");
}

void MainWindow::wireDriverSignals()
{
    if (!m_driver) return;

    QObject *qobj = m_driver->asQObject();

    // ── CTSPA22SDriver 信号全连接 ──
    if (auto *ct = qobject_cast<CTSPA22SDriver*>(qobj)) {
        connect(ct, &CTSPA22SDriver::connectionChanged, this, [this](bool ok) {
            AppState::instance()->setConnected(ok);
            m_deviceLabel->setText(ok ? QString::fromUtf8("\u25CF 设备连接： 已连接")
                                      : QString::fromUtf8("\u25CF 设备连接： 未连接"));

            if (ok) {
                m_connectBtn->setText("断开设备");
                m_connectBtn->setStyleSheet(m_connectBtn->styleSheet().replace("#0a6e3b", "#8b2020"));
                m_acquireBtn->setEnabled(true);
                m_acquireBtn->setText("开始采集");
                m_acquireBtn->setStyleSheet(m_acquireBtn->styleSheet().replace("#c2590a", "#0652a2"));
                m_acquireBtn->setProperty("acquiring", false);
            } else {
                m_connectBtn->setText("连接设备");
                m_connectBtn->setStyleSheet(m_connectBtn->styleSheet().replace("#8b2020", "#0a6e3b"));
                m_acquireBtn->setEnabled(false);
                m_acquireBtn->setText("开始采集");
                m_acquireBtn->setStyleSheet(m_acquireBtn->styleSheet().replace("#c2590a", "#0652a2"));
                m_acquireBtn->setProperty("acquiring", false);
            }
        });
        connect(ct, &CTSPA22SDriver::statusChanged, this, [this](const QString &s) {
            statusBar()->showMessage(s);
        });
        connect(ct, &CTSPA22SDriver::errorOccurred, this, [this](const QString &e) {
            AppState::instance()->setConnected(false);
            statusBar()->showMessage(QString::fromUtf8("\u9519\u8BEF\uFF1A") + e);
        });
        connect(ct, &CTSPA22SDriver::temperatureReceived, this, [](double t) {
            AppState::instance()->setTemperature(static_cast<float>(t));
        });
        connect(ct, &CTSPA22SDriver::voltageReceived, this, [](double v) {
            AppState::instance()->setInputVoltage(static_cast<float>(v));
        });
        connect(ct, &CTSPA22SDriver::encoderPositionChanged, this, [](int pos) {
            AppState::instance()->setEncoderCount(static_cast<uint32_t>(pos));
        });
        return;
    }
}

MainWindow::~MainWindow()
{
    // 在 QWidget 析构链启动之前，主动断开仪器连接
    // 否则 QTcpSocket 会在 HomePage→QTabWidget→MainWindow 析构链中被意外回收
    if (m_driver) {
        m_driver->stopAcquisition();
        m_driver->disconnectDevice();
        // 解除 HomePage 对 Driver 信号的引用，防止析构期间信号触发已销毁的 Widget
        disconnect(m_driver->asQObject(), nullptr, m_homePage, nullptr);
        delete m_driver->asQObject();
        m_driver = nullptr;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 正常关闭路径: 先断仪器，再走默认关闭流程
    if (m_driver && m_driver->isConnected()) {
        m_driver->stopAcquisition();
        m_driver->disconnectDevice();
    }
    event->accept();
}
