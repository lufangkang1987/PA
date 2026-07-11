#include "MainWindow.h"
#include "HomePage.h"
#include "ParamPage.h"
#include "MeasurePage.h"
#include "IDriver.h"
#include "CTSPA22SDriver.h"
#include "AppState.h"
#include <QWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QComboBox>
#include <QCloseEvent>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>
#include <QSplitter>

// ═══════════════════════════════════════════════════════════
// C扫数据 .dat 文件格式 (PADT)
//   Header: magic(4) version(4) width(4) height(4) jsonLen(4)
//           + json(jsonLen)  — PAParams 序列化
//   Body:   float32[width*height]  — 振幅数据 (0.0~1.0)
// ═══════════════════════════════════════════════════════════

static const quint32 kCScanMagic   = 0x54444150;  // "PADT" LE
static const quint32 kCScanVersion = 1;

static bool saveCScanFile(const QString &path, const QVector<float> &data,
                          int w, int h, const QJsonObject &params)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QByteArray jsonBytes = QJsonDocument(params).toJson(QJsonDocument::Compact);
    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << kCScanMagic << kCScanVersion
       << quint32(w) << quint32(h) << quint32(jsonBytes.size());
    file.write(jsonBytes);
    ds.writeRawData(reinterpret_cast<const char *>(data.constData()),
                    data.size() * int(sizeof(float)));
    return true;
}

static QVector<float> loadCScanFile(const QString &path, int &w, int &h,
                                    QJsonObject &params)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);
    quint32 magic = 0, version = 0, width = 0, height = 0, jsonLen = 0;
    ds >> magic >> version >> width >> height >> jsonLen;
    if (magic != kCScanMagic || version < 1 || width == 0 || height == 0)
        return {};

    QByteArray jsonBytes = file.read(jsonLen);
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (doc.isObject()) params = doc.object();

    w = int(width);
    h = int(height);
    QVector<float> data(w * h);
    ds.readRawData(reinterpret_cast<char *>(data.data()),
                   data.size() * int(sizeof(float)));
    return data;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle("相控阵检测系统");
    // 默认尺寸在 main.cpp 中根据屏幕自适应设置，此处仅设最小值
    setMinimumSize(960, 600);
    setUpdatesEnabled(false);  // 批量构建，启动时避免中间重绘

    auto *shell = new QWidget(this);
    auto *root = new QVBoxLayout(shell);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    auto *header = new QFrame(shell);
    header->setObjectName("TopHeader");
    header->setMinimumHeight(48);
    header->setMaximumHeight(60);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 14, 0);
    headerLayout->setSpacing(8);

    auto *logo = new QLabel("◆", header);
    logo->setObjectName("LogoMark");
    logo->setFixedSize(28, 28);
    auto *title = new QLabel("相控阵检测系统", header);
    title->setObjectName("AppTitle");
    title->setMinimumWidth(0);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *version = new QLabel("V1.0.0", header);
    version->setObjectName("VersionLabel");
    version->setMinimumWidth(0);
    version->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // 连接模式选择
    auto *modeLabel = new QLabel("连接模式:", header);
    modeLabel->setObjectName("HeaderInfo");
    modeLabel->setMinimumWidth(0);
    modeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_modeCombo = new QComboBox(header);
    m_modeCombo->addItem("无线 (WIFI)", static_cast<int>(ConnectionMode::Wireless));
    m_modeCombo->addItem("有线 (网线)", static_cast<int>(ConnectionMode::Wired));
    m_modeCombo->setStyleSheet(
        "QComboBox{background:#0c2135;color:#d5e9f5;border:1px solid #1d3d58;padding:2px 8px;min-width:80px;}"
        "QComboBox::drop-down{border:0;}"
        "QComboBox::down-arrow{image:none;}"
        "QComboBox QAbstractItemView{background:#0c2135;color:#d5e9f5;selection-background:#0a72d6;}"
    );

    m_deviceLabel = new QLabel("● 设备连接： 未连接", header);
    m_deviceLabel->setObjectName("DeviceOk");
    m_deviceLabel->setMinimumWidth(0);
    m_deviceLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_ipLabel = new QLabel("IP: 192.168.0.51", header);
    m_ipLabel->setObjectName("HeaderInfo");
    m_ipLabel->setMinimumWidth(0);
    m_ipLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    headerLayout->addWidget(logo);
    headerLayout->addWidget(title);
    headerLayout->addWidget(version);
    headerLayout->addStretch(2);
    headerLayout->addWidget(modeLabel);
    headerLayout->addSpacing(2);
    headerLayout->addWidget(m_modeCombo);
    headerLayout->addSpacing(6);
    headerLayout->addWidget(m_deviceLabel);
    headerLayout->addSpacing(6);
    headerLayout->addWidget(m_ipLabel);

    // ────── 操作按钮 ──────
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

    m_connectBtn = makeBtn("连接设备", "#0a6e3b", header);
    m_acquireBtn = makeBtn("开始采集", "#0652a2", header);

    headerLayout->addSpacing(8);
    headerLayout->addWidget(m_connectBtn);
    headerLayout->addWidget(m_acquireBtn);

    root->addWidget(header);

    m_homePage = new HomePage;
    m_paramPage = new ParamPage;
    m_measurePage = new MeasurePage;

    // 水平布局：左侧参数面板 | 主页 | 右侧测量面板
    // 用 QHBoxLayout 替代 QSplitter，确保各面板边框完整可见
    auto *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);
    mainLayout->addWidget(m_paramPage);
    mainLayout->addWidget(m_homePage, 1);
    mainLayout->addWidget(m_measurePage);

    root->addLayout(mainLayout, 1);
    setCentralWidget(shell);

    m_driver = new CTSPA22SDriver(this);
    m_homePage->setDriver(m_driver);

    // ── MeasurePage 信号 → MainWindow ──
    connect(m_measurePage, &MeasurePage::exitRequested, this, &MainWindow::close);
    connect(m_measurePage, &MeasurePage::powerOffAndExitRequested, this, [this] {
        if (m_driver && m_driver->isConnected()) {
            m_driver->powerOff();
        }
        close();
    });
    connect(m_measurePage, &MeasurePage::screenshotRequested, this, [this] {
        // TODO: 截屏保存
    });
    connect(m_measurePage, &MeasurePage::freezeChanged, this, [this](bool frozen) {
        statusBar()->showMessage(frozen ? "画面已冻结" : "画面已解冻");
        if (m_homePage)
            m_homePage->setFrozen(frozen);
    });
    connect(m_measurePage, &MeasurePage::loadParamsRequested, this, [this] {
        QString paramsPath = QCoreApplication::applicationDirPath() + "/params";
        QDir().mkpath(paramsPath);
        QString filePath = QFileDialog::getOpenFileName(
            this, "调用参数", paramsPath,
            "参数文件 (*.json *.ini *.param);;所有文件 (*)");
        if (!filePath.isEmpty()) {
            // TODO: 读取参数文件并更新 m_paramPage 和驱动
            statusBar()->showMessage("已加载参数: " + filePath);
        }
    });

    wireDriverSignals();

    // A扫闸门拖拽 → 更新 ParamPage 控件 + 下发硬件
    connect(m_homePage, &HomePage::gateDragged,
            m_paramPage, &ParamPage::onGateDragged);

    // A扫 Ctrl+点击 → 切换声束
    connect(m_homePage, &HomePage::beamChangeRequested, this, [this](int beam) {
        m_paramPage->setBeamNo(beam);
    });

    // 参数页闸门变化 → 主页A扫闸门显示
    connect(m_paramPage, &ParamPage::gateParamsChanged, this, [this] {
        static const QColor gateColors[3] = {
            QColor(255, 30, 30),    // A 红
            QColor(255, 200, 0),    // B 黄
            QColor(200, 50, 255),   // C 紫
        };
        for (int g = 0; g < 3; ++g) {
            bool enabled; float start, width, threshold;
            m_paramPage->getGateParams(g, enabled, start, width, threshold);
            m_homePage->setGateParams(g, enabled, start, width, threshold, gateColors[g]);
        }
        // 同步当前选中闸门（拖拽时用）
        m_homePage->setActiveGate(m_paramPage->activeGate());
    });

    // C扫扫描按钮信号（来自 ParamPage 成像子页）
    connect(m_paramPage, &ParamPage::scanStarted, this, [this] {
        if (m_driver && m_driver->isConnected())
            m_driver->startAcquisition();
    });
    connect(m_paramPage, &ParamPage::scanStopped, this, [this] {
        if (m_driver)
            m_driver->stopAcquisition();
    });

    // 声束号 / 增益变化 → 右侧测量面板读数
    connect(m_paramPage, &ParamPage::beamInfoChanged,
            m_measurePage, &MeasurePage::updateBeamInfo);

    // C扫保存数据（ParamPage 已选好路径，MainWindow 执行实际写入）
    connect(m_paramPage, &ParamPage::saveDataRequested, this, [this](const QString &path) {
        if (!m_homePage || !m_paramPage) return;
        int w = 0, h = 0;
        QVector<float> data = m_homePage->getCScanData(w, h);
        if (data.isEmpty() || w <= 0 || h <= 0) {
            statusBar()->showMessage("无C扫数据可保存");
            return;
        }
        QJsonObject paramsJson = m_paramPage->serializeParams();
        if (saveCScanFile(path, data, w, h, paramsJson)) {
            statusBar()->showMessage(QString("C扫数据已保存: %1 (%2×%3)")
                .arg(path).arg(w).arg(h));
        } else {
            statusBar()->showMessage("保存C扫数据失败");
        }
    });

    // C扫回放数据
    connect(m_paramPage, &ParamPage::replayDataRequested, this, [this](const QString &path) {
        if (!m_homePage || !m_paramPage) return;
        int w = 0, h = 0;
        QJsonObject paramsJson;
        QVector<float> data = loadCScanFile(path, w, h, paramsJson);
        if (data.isEmpty() || w <= 0 || h <= 0) {
            statusBar()->showMessage("加载C扫数据失败");
            return;
        }
        m_homePage->setCScanReplayData(data, w, h, true);
        // 恢复参数（静默加载，不触发硬件下发）
        m_paramPage->deserializeParams(paramsJson);
        m_paramPage->syncUiFromParams();
        statusBar()->showMessage(QString("C扫回放: %1 (%2×%3)")
            .arg(path).arg(w).arg(h));
    });

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
        QStatusBar { background:#06101a; color:#9dcfe8; border-top:1px solid #102a3d; }
        #MainSplitter::handle {
            background:#08131d;
            border:0;
        }
    )");

    setUpdatesEnabled(true);
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
        // 闸门读数 → 右侧测量面板
        if (m_measurePage) {
            connect(ct, &CTSPA22SDriver::gateReadingsReady,
                    m_measurePage, &MeasurePage::updateGateReadings);
        }
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
