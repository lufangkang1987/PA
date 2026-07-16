#include "MainWindow.h"
#include "HomePage.h"
#include "Theme.h"
#include "ParamPage.h"
#include "MeasurePage.h"
#include "IDriver.h"
#include "CTSPA22SDriver.h"
#include "CalibrationController.h"
#include "ConnectionManager.h"
#include "ParameterDispatcher.h"
#include "CScanIOManager.h"
#include "ReadingCalculator.h"
#include "CScanEngine.h"
#include "CScanDataCodec.h"
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QThread>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle(QString::fromUtf8("相控阵检测系统"));
    // 默认尺寸在 main.cpp 中根据屏幕自适应设置，此处仅设最小值
    setMinimumSize(960, 600);
    setUpdatesEnabled(false);  // 批量构建，启动时避免中间重绘

    auto *shell = new QWidget(this);
    auto *root = new QVBoxLayout(shell);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    m_driver = new CTSPA22SDriver(this);  // 提前创建，供 buildHeader 中 ConnectionManager 使用
    buildHeader(shell, root);
    buildCentral(root);
    setCentralWidget(shell);
    buildDriverAndEngine();
    wirePageSignals();
    wireCScanIO();
    wireCalibration();
    wireDriverSignals();
    applyGlobalStyleSheet();

    statusBar()->showMessage(QString::fromUtf8("系统就绪"));
    setUpdatesEnabled(true);
}

void MainWindow::buildHeader(QWidget *shell, QVBoxLayout *root)
{
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
    auto *title = new QLabel(QString::fromUtf8("相控阵检测系统"), header);
    title->setObjectName("AppTitle");
    title->setMinimumWidth(0);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *version = new QLabel("V1.0.0", header);
    version->setObjectName("VersionLabel");
    version->setMinimumWidth(0);
    version->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // 连接模式选择
    auto *modeLabel = new QLabel(QString::fromUtf8("连接模式:"), header);
    modeLabel->setObjectName("HeaderInfo");
    modeLabel->setMinimumWidth(0);
    modeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_modeCombo = new QComboBox(header);
    m_modeCombo->addItem(QString::fromUtf8("无线 (WIFI)"), static_cast<int>(ConnectionMode::Wireless));
    m_modeCombo->addItem(QString::fromUtf8("有线 (网线)"), static_cast<int>(ConnectionMode::Wired));
    m_modeCombo->setStyleSheet(
        "QComboBox{background:#0c2135;color:#d5e9f5;border:1px solid #1d3d58;padding:2px 8px;min-width:80px;}"
        "QComboBox::drop-down{border:0;}"
        "QComboBox::down-arrow{image:none;}"
        "QComboBox QAbstractItemView{background:#0c2135;color:#d5e9f5;selection-background:#0a72d6;}"
    );

    m_deviceLabel = new QLabel(QString::fromUtf8("● 设备连接： 未连接"), header);
    m_deviceLabel->setObjectName("DeviceOk");
    m_deviceLabel->setMinimumWidth(0);
    m_deviceLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_ipLabel = new QLabel(QString("IP: %1").arg(CTSPA22SDriver::DefaultWifiIP), header);
    m_ipLabel->setObjectName("HeaderInfo");
    m_ipLabel->setMinimumWidth(0);
    m_ipLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_temperatureLabel = new QLabel(QString::fromUtf8("温度: --"), header);
    m_pcBatteryLabel = new QLabel(QString::fromUtf8("PC电量: --"), header);
    m_paBatteryLabel = new QLabel(QString::fromUtf8("PA电量: --"), header);
    for (QLabel *label : {m_temperatureLabel, m_pcBatteryLabel, m_paBatteryLabel}) {
        label->setObjectName("HeaderInfo");
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

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
    headerLayout->addSpacing(6);
    headerLayout->addWidget(m_temperatureLabel);
    headerLayout->addWidget(m_pcBatteryLabel);
    headerLayout->addWidget(m_paBatteryLabel);
    updatePcBattery();

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

    auto *connectBtn = makeBtn(QString::fromUtf8("连接设备"), "#0a6e3b", header);
    auto *acquireBtn = makeBtn(QString::fromUtf8("开始采集"), "#0652a2", header);

    headerLayout->addSpacing(8);
    headerLayout->addWidget(connectBtn);
    headerLayout->addWidget(acquireBtn);

    root->addWidget(header);

    m_connManager = new ConnectionManager(m_driver, m_modeCombo, m_deviceLabel,
        m_ipLabel, connectBtn, acquireBtn, this);
    m_connManager->initialize();
}

void MainWindow::buildCentral(QVBoxLayout *root)
{
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
}

void MainWindow::buildDriverAndEngine()
{
    m_cScanThread = new QThread(this);
    m_cScanThread->setObjectName("CScanImagingThread");
    m_cScanEngine = new CScanEngine;
    m_cScanEngine->moveToThread(m_cScanThread);
    connect(m_cScanThread, &QThread::finished, m_cScanEngine, &QObject::deleteLater);
    m_cScanThread->start();
    m_homePage->setDriver(m_driver);
    auto *paramDispatcher = new ParameterDispatcher(this);
    paramDispatcher->setDriver(m_driver);
    m_paramPage->setDispatcher(paramDispatcher);
    m_calController = new CalibrationController(m_driver, m_paramPage,
        m_cScanEngine, m_homePage, this);
    connect(m_calController, &CalibrationController::statusMessage, this,
            [this](const QString &msg) { statusBar()->showMessage(msg); });
    m_ioManager = new CScanIOManager(m_homePage, m_paramPage,
        m_cScanEngine, m_calController, this);
    connect(m_ioManager, &CScanIOManager::statusMessage, this,
            [this](const QString &msg) { statusBar()->showMessage(msg); });
    const bool paramsLoaded = m_paramPage->initializeParams();
    m_homePage->configureCScanView(m_paramPage->params());
    if (!paramsLoaded)
        statusBar()->showMessage(QString::fromUtf8("默认参数文件加载失败，已使用程序内置参数"));
}

void MainWindow::wirePageSignals()
{
    // ── MeasurePage 信号 → MainWindow ──
    connect(m_measurePage, &MeasurePage::exitRequested, this, &MainWindow::close);
    connect(m_measurePage, &MeasurePage::powerOffAndExitRequested, this, [this] {
        if (m_driver && m_driver->isConnected()) {
            m_driver->powerOff();
        }
        close();
    });
    connect(m_measurePage, &MeasurePage::screenshotRequested, this, [this] {
        const QString dir = QCoreApplication::applicationDirPath() + "/screenshots";
        QDir().mkpath(dir);
        const QString suggested = dir + "/PA_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
        const QString path = QFileDialog::getSaveFileName(
            this, QString::fromUtf8("保存截图"), suggested, QString::fromUtf8("PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg)"));
        if (path.isEmpty()) return;
        statusBar()->showMessage(grab().save(path) ? QString::fromUtf8("截图已保存: ") + path : QString::fromUtf8("截图保存失败"));
    });
    connect(m_measurePage, &MeasurePage::freezeChanged, this, [this](bool frozen) {
        statusBar()->showMessage(frozen ? QString::fromUtf8("画面已冻结") : QString::fromUtf8("画面已解冻"));
        if (m_homePage)
            m_homePage->setFrozen(frozen);
    });
    connect(m_measurePage, &MeasurePage::loadParamsRequested, this, [this] {
        QString paramsPath = QCoreApplication::applicationDirPath() + "/params";
        QDir().mkpath(paramsPath);
        QString filePath = QFileDialog::getOpenFileName(
            this, QString::fromUtf8("调用参数"), paramsPath,
            QString::fromUtf8("参数文件 (*.json *.ini *.param *.par);;所有文件 (*)"));
        if (!filePath.isEmpty()) loadParamsFile(filePath);
    });
    connect(m_measurePage, &MeasurePage::applyLawRequested,
            m_paramPage, &ParamPage::onApplyLaw);

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
            ThemeColor::GateA, ThemeColor::GateB, ThemeColor::GateC
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
        if (m_calController->isCalibrating() || m_calController->isEncoderCalibrating() || m_ioManager->isReplayActive()) {
            statusBar()->showMessage(QString::fromUtf8("请先退出校准或回放状态"));
            m_paramPage->finishScan();
            return;
        }
        m_homePage->configureCScanView(m_paramPage->params());
        m_cScanEngine->configure(m_paramPage->params());
        m_cScanEngine->start();
        if (m_driver && m_driver->isConnected()) {
            m_driver->resetEncoder(0);
            m_driver->startAcquisition();
        }
    });
    connect(m_paramPage, &ParamPage::scanStopped, this, [this] {
        m_cScanEngine->stop();
        if (m_driver)
            m_driver->stopAcquisition();
    });

    // 声束号 / 增益变化 → 右侧测量面板读数
    connect(m_paramPage, &ParamPage::beamInfoChanged,
            m_measurePage, &MeasurePage::updateBeamInfo);
}

void MainWindow::wireCScanIO()
{
    connect(m_paramPage, &ParamPage::saveDataRequested,
            m_ioManager, &CScanIOManager::onSaveDataRequested);
    connect(m_paramPage, &ParamPage::replayDataRequested,
            m_ioManager, &CScanIOManager::onReplayDataRequested);
    connect(m_homePage, &HomePage::cScanPositionSelected,
            m_ioManager, &CScanIOManager::onCScanPositionSelected);
    connect(m_homePage, &HomePage::cScanAnalysisRectChanged,
            m_paramPage, &ParamPage::setAnalysisRect);
    connect(m_paramPage, &ParamPage::cScanViewParamsChanged,
            m_ioManager, &CScanIOManager::onCScanViewParamsChanged);
    connect(m_paramPage, &ParamPage::cScanPageRequested,
            m_ioManager, &CScanIOManager::onCScanPageRequested);
    connect(m_paramPage, &ParamPage::exitReplayRequested,
            m_ioManager, &CScanIOManager::onExitReplayRequested);
    connect(m_ioManager, &CScanIOManager::replayStateChanged,
            m_calController, &CalibrationController::setReplayActive);
}
void MainWindow::wireCalibration()
{
    connect(m_paramPage, &ParamPage::calibrationRequested,
            m_calController, &CalibrationController::onCalibrationRequested);
    connect(m_paramPage, &ParamPage::encoderCalibrationRequested,
            m_calController, &CalibrationController::onEncoderCalibrationRequested);
}

void MainWindow::applyGlobalStyleSheet()
{
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
}

void MainWindow::wireDriverSignals()
{
    if (!m_driver) return;


    // ── CTSPA22SDriver 信号全连接 ──
    {
        connect(m_driver, &IDriver::connectionChanged, this, [this](bool ok) {
            if (ok) m_paramPage->applyCurrentParams();
        });
        connect(m_driver, &IDriver::statusChanged, this, [this](const QString &s) {
            statusBar()->showMessage(s);
        });
        connect(m_driver, &IDriver::errorOccurred, this, [this](const QString &e) {
            statusBar()->showMessage(QString::fromUtf8("错误：") + e);
        });
        connect(m_driver, &IDriver::temperatureReceived, this, [this](double t) {
            m_temperatureLabel->setText(QString::fromUtf8("温度: %1 °C").arg(t, 0, 'f', 1));
            updatePcBattery();
        });
        connect(m_driver, &IDriver::voltageReceived, this, [this](double v) {
            const int percent = qBound(0, qRound((v - 9.2) / 2.3 * 100.0), 100);
            m_paBatteryLabel->setText(QString::fromUtf8("PA电量: %1%").arg(percent));
            updatePcBattery();
        });
        connect(m_driver, &IDriver::encoderPositionChanged,
                m_calController, &CalibrationController::setEncoderPosition);
        connect(m_driver, &IDriver::dataPacketReady,
                m_cScanEngine, &CScanEngine::processPacket);
        connect(m_driver, &IDriver::scanRulePositionsReady,
                m_cScanEngine, &CScanEngine::setRulePositions);
        connect(m_driver, &IDriver::scanRulePositionsReady, this,
                [this](const QVector<double> &positions) {
            m_scanRulePositions = positions.mid(0, MaxBeams);
        });
        connect(m_driver, &IDriver::dataPacketReady, this, [this](std::shared_ptr<DataPacket> packet) {
            m_latestPacket = packet;
            m_calController->setLatestPacket(packet);
            m_hasLatestPacket = packet && packet->beamCount > 0;
            if (!m_hasLatestPacket || !m_measurePage || !m_paramPage) return;

            const GateReadings r = calculateReadings(m_paramPage->params(), *packet, m_scanRulePositions);
            m_measurePage->updateGateReadings('A', r.aAmplitude, r.aSoundPathMm, r.angleDegrees, r.horizontalOffsetMm);
            m_measurePage->updateGateReadings('B', r.bAmplitude, r.bSoundPathMm, r.angleDegrees, r.horizontalOffsetMm);

            if (r.alarmTriggered) {
#ifdef Q_OS_WIN
                Beep(2000, 500);
#else
                QApplication::beep();
#endif
            }
        });
        connect(m_cScanEngine, &CScanEngine::imageUpdated, this,
                [this](const QVector<float> &img, int w, int h) {
            m_homePage->setCScanData(img, w, h);
            m_homePage->setCScanImageSpan(m_cScanEngine->imgSpanStart(),
                                          m_cScanEngine->imgSpanEnd());
        });
        connect(m_cScanEngine, &CScanEngine::metricsChanged,
                m_homePage, &HomePage::updateCScanMetrics);
        connect(m_cScanEngine, &CScanEngine::scanCompleted, this, [this] {
            if (m_driver) m_driver->stopAcquisition();
            m_ioManager->setReplayActive(true);
            m_homePage->setCScanReplayMode(true);
            m_paramPage->finishScan();
            statusBar()->showMessage("C scan completed");
        });
        // 闸门读数 → 右侧测量面板
        return;
    }
}

void MainWindow::updatePcBattery()
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

MainWindow::~MainWindow()
{
    // 在 QWidget 析构链启动之前，主动断开仪器连接
    // 否则 QTcpSocket 会在 HomePage→QTabWidget→MainWindow 析构链中被意外回收
    if (m_driver) {
        m_driver->stopAcquisition();
        m_driver->disconnectDevice();
        // 解除 HomePage 对 Driver 信号的引用，防止析构期间信号触发已销毁的 Widget
        disconnect(m_driver, nullptr, m_homePage, nullptr);
        delete m_driver;
        m_driver = nullptr;
    }
    if (m_cScanThread) {
        m_cScanThread->quit();
        m_cScanThread->wait();
        m_cScanEngine = nullptr;
    }
}

bool MainWindow::loadParamsFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        statusBar()->showMessage(QString::fromUtf8("参数文件格式无效"));
        return false;
    }
    QJsonObject params = document.object();
    m_paramPage->deserializeParams(params);
    m_paramPage->syncUiFromParams();
    m_homePage->configureCScanView(m_paramPage->params());
    if (m_driver && m_driver->isConnected()) m_paramPage->applyCurrentParams();
    statusBar()->showMessage(QString::fromUtf8("已加载参数: ") + path);
    return true;
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
