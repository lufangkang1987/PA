#include "MainWindow.h"
#include "HomePage.h"
#include "Theme.h"
#include "ParamPage.h"
#include "MeasurePage.h"
#include "HeaderBar.h"
#include "IDriver.h"
#include "CTSPA22SDriver.h"
#include "CalibrationController.h"
#include "ConnectionManager.h"
#include "ParameterDispatcher.h"
#include "DataPacketProcessor.h"
#include "CScanIOManager.h"
#include "CScanEngine.h"
#include "CScanDataCodec.h"
#include <QWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QFrame>
#include <QPushButton>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
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
    buildHeader(root);
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

void MainWindow::buildHeader(QVBoxLayout *root)
{
    m_headerBar = new HeaderBar(this);
    root->addWidget(m_headerBar);

    m_connManager = new ConnectionManager(m_driver, this);

    // 按钮点击 → ConnectionManager
    connect(m_headerBar->connectBtn(), &QPushButton::clicked,
            m_connManager, &ConnectionManager::onConnectButtonClicked);
    connect(m_headerBar->acquireBtn(), &QPushButton::clicked,
            m_connManager, &ConnectionManager::onAcquireButtonClicked);

    // 连接模式切换 → ConnectionManager
    connect(m_headerBar->modeCombo(), QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int /*idx*/) {
        auto mode = static_cast<ConnectionMode>(m_headerBar->modeCombo()->currentData().toInt());
        if (m_driver->isConnected()) {
            m_driver->stopAcquisition();
            m_driver->disconnectDevice();
        }
        m_connManager->setConnectionMode(mode);
        if (m_driver->isConnected() == false)
            m_driver->connectDevice(mode);
    });

    // ConnectionManager 信号 → HeaderBar UI
    connect(m_connManager, &ConnectionManager::connectionStateChanged, this,
            [this](bool connected) {
        m_headerBar->deviceLabel()->setText(connected
            ? QString::fromUtf8("● 设备连接： 已连接")
            : QString::fromUtf8("● 设备连接： 未连接"));
        m_headerBar->connectBtn()->setText(connected
            ? QString::fromUtf8("断开设备") : QString::fromUtf8("连接设备"));
        m_headerBar->acquireBtn()->setEnabled(connected);
    });
    connect(m_connManager, &ConnectionManager::acquireStateChanged, this,
            [this](bool acquiring) {
        m_headerBar->acquireBtn()->setText(acquiring
            ? QString::fromUtf8("停止采集") : QString::fromUtf8("开始采集"));
    });
    connect(m_connManager, &ConnectionManager::ipAddressChanged, this,
            [this](const QString &ip) {
        m_headerBar->ipLabel()->setText(QString("IP: %1").arg(ip));
    });

    // 初始化
    m_connManager->setConnectionMode(ConnectionMode::Wireless);
    m_headerBar->acquireBtn()->setEnabled(false);
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
    auto *paramDispatcher = new ParameterDispatcher(this);
    paramDispatcher->setDriver(m_driver);
    m_paramPage->setDispatcher(paramDispatcher);
    m_calController = new CalibrationController(m_driver, m_cScanEngine, this);
    m_calController->setParams(&m_paramPage->params());
    connect(m_calController, &CalibrationController::statusMessage, this,
            [this](const QString &msg) { statusBar()->showMessage(msg); });
    m_ioManager = new CScanIOManager(m_cScanEngine, this);
    m_ioManager->setParams(&m_paramPage->params());
    connect(m_ioManager, &CScanIOManager::statusMessage, this,
            [this](const QString &msg) { statusBar()->showMessage(msg); });
    m_dataProcessor = new DataPacketProcessor(this);
    m_dataProcessor->setCalibrationController(m_calController);
    m_dataProcessor->setParams(&m_paramPage->params());
    const bool paramsLoaded = m_paramPage->initializeParams();
    m_homePage->bindParams(&m_paramPage->params());
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
    connect(m_paramPage, &ParamPage::rangeChanged, this, [this](float) {
        m_homePage->bindParams(&m_paramPage->params());
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
        if (!enterMode(AppMode::Scanning)) {
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
    // ── 保存：MainWindow 收集数据后调用纯文件 I/O ──
    connect(m_paramPage, &ParamPage::saveDataRequested, this,
            [this](const QString &path) {
        int w = 0, h = 0;
        QVector<float> data = m_homePage->getCScanData(w, h);
        if (data.isEmpty() || w <= 0 || h <= 0) {
            statusBar()->showMessage("无C扫数据可保存");
            return;
        }
        QJsonObject paramsJson = m_paramPage->serializeParams();
        auto packets = m_cScanEngine->archivedPackets();
        if (packets && m_ioManager->saveToFile(path, data, w, h, paramsJson, *packets))
            statusBar()->showMessage(QString("C扫数据已保存: %1").arg(path));
        else
            statusBar()->showMessage("保存C扫数据失败");
    });

    // ── 回放加载：MainWindow 负责 UI 分发 ──
    connect(m_paramPage, &ParamPage::replayDataRequested, this,
            [this](const QString &path) {
        if (!enterMode(AppMode::Replaying)) {
            statusBar()->showMessage("扫查或校准期间不能进入回放");
            return;
        }
        m_ioManager->onReplayDataRequested(path);
    });
    connect(m_ioManager, &CScanIOManager::replayDataReady, this,
            [this](const QVector<float> &data, int w, int h,
                   const QJsonObject &params,
                   const QVector<DataPacket> &packets,
                   const QVector<ScanRule> &rules) {
        m_homePage->setCScanReplayData(data, w, h, true);
        if (!rules.isEmpty()) m_cScanEngine->setScanRules(rules);
        m_paramPage->deserializeParams(params);
        m_paramPage->syncUiFromParams();
        m_homePage->configureCScanView(m_paramPage->params());
        emit m_paramPage->cScanViewParamsChanged();
    });

    // ── 回放导航 ──
    connect(m_homePage, &HomePage::cScanPositionSelected,
            m_ioManager, &CScanIOManager::onCScanPositionSelected);
    connect(m_homePage, &HomePage::cScanAnalysisRectChanged,
            m_paramPage, &ParamPage::setAnalysisRect);
    connect(m_paramPage, &ParamPage::cScanPageRequested,
            m_ioManager, &CScanIOManager::onCScanPageRequested);
    connect(m_paramPage, &ParamPage::exitReplayRequested, this, [this] {
        m_ioManager->onExitReplayRequested();
        leaveMode(AppMode::Replaying);
    });

    // ── 回放 UI 操作（CScanIOManager → HomePage）──
    connect(m_ioManager, &CScanIOManager::replayPacketRequested, this,
            [this](const DataPacket &packet, int line, int beam, int rectify) {
        m_homePage->showReplayPacket(packet, line, beam, rectify);
    });
    connect(m_ioManager, &CScanIOManager::replayUISetMode,
            m_homePage, &HomePage::setCScanReplayMode);
    connect(m_ioManager, &CScanIOManager::replayUISetPageStart,
            m_homePage, &HomePage::setCScanPageStart);
    connect(m_ioManager, &CScanIOManager::replayUISelectLine,
            m_homePage, &HomePage::selectCScanLine);

    // ── C扫视图配置 ──
    connect(m_paramPage, &ParamPage::cScanViewParamsChanged, this, [this] {
        m_homePage->configureCScanView(m_paramPage->params());
    });

    // ── 回放状态同步 ──
    connect(m_ioManager, &CScanIOManager::replayStateChanged,
            m_calController, &CalibrationController::setReplayActive);
}
void MainWindow::wireCalibration()
{
    // 校准触发 → 弹窗确认后转发
    connect(m_paramPage, &ParamPage::calibrationRequested, this, [this](int item) {
        if (!enterMode(AppMode::Calibrating)) {
            statusBar()->showMessage(QString::fromUtf8("请先退出扫查或回放状态"));
            return;
        }
        if ((item == 2 || item == 3) && QMessageBox::question(
                this, "校准参考线", "默认参考线为80%，是否改为50%？",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            m_calController->setCalibrationTargetPercent(50);
        else
            m_calController->setCalibrationTargetPercent(80);
        m_calController->onCalibrationRequested(item);
    });
    connect(m_paramPage, &ParamPage::encoderCalibrationRequested,
            m_calController, &CalibrationController::onEncoderCalibrationRequested);

    // 校准结果 → ParamPage + Driver
    connect(m_calController, &CalibrationController::calibrationGuideChanged, this,
            [this](bool visible, int targetPercent) {
        m_homePage->setAScanCalibrationGuide(visible, targetPercent);
        if (!visible)
            leaveMode(AppMode::Calibrating);
    });
    connect(m_calController, &CalibrationController::beamSelectRequested,
            m_paramPage, &ParamPage::setBeamNo);
    connect(m_calController, &CalibrationController::velocityCalibrated, this,
            [this](int v) {
        m_paramPage->setCalibratedVelocity(v);
        m_driver->setVelocity(m_paramPage->params().wp.lVelocity);
        m_driver->setRange(m_paramPage->params().tx.range);
    });
    connect(m_calController, &CalibrationController::probeDelayCalibrated, this,
            [this](float d) {
        m_paramPage->setCalibratedProbeDelay(d);
        const auto &p = m_paramPage->params();
        if (p.scan.scanType < 3) m_driver->setBeamDelay(); else m_driver->setCommonRDelay();
    });
    connect(m_calController, &CalibrationController::acgCalibrated, this,
            [this](const QVector<float> &values) {
        m_paramPage->setCalibratedACG(values);
        m_driver->setACG(true, m_paramPage->params());
    });
    connect(m_calController, &CalibrationController::coderDegCalibrated,
            m_paramPage, &ParamPage::setCalibratedCoderDeg);
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

    // ── 连接状态 ──
    connect(m_driver, &IDriver::connectionChanged, this, [this](bool ok) {
        if (ok) m_paramPage->applyCurrentParams();
    });
    connect(m_driver, &IDriver::statusChanged, this, [this](const QString &s) {
        statusBar()->showMessage(s);
    });
    connect(m_driver, &IDriver::errorOccurred, this, [this](const QString &e) {
        statusBar()->showMessage(QString::fromUtf8("错误：") + e);
    });

    // ── 遥测 → HeaderBar ──
    connect(m_driver, &IDriver::temperatureReceived, this, [this](double t) {
        m_headerBar->setTemperature(t);
        m_headerBar->updatePcBattery();
    });
    connect(m_driver, &IDriver::voltageReceived, this, [this](double v) {
        const int percent = qBound(0, qRound((v - 9.2) / 2.3 * 100.0), 100);
        m_headerBar->setPaBattery(percent);
        m_headerBar->updatePcBattery();
    });

    // ── A/B 扫波形 → HomePage ──
    connect(m_driver, &IDriver::waveformReady,
            m_homePage, &HomePage::setAScanWaveform);
    connect(m_driver, &IDriver::multiBeamWaveformsReady,
            m_homePage, &HomePage::setBScanWaveforms);
    connect(m_driver, &IDriver::frameStatisticsChanged,
            m_homePage, &HomePage::updateFrameStatistics);

    // ── 编码器 → 校准控制器 ──
    connect(m_driver, &IDriver::encoderPositionChanged,
            m_calController, &CalibrationController::setEncoderPosition);

    // ── 扫查法则 → CScanEngine + BScanWidget + DataPacketProcessor ──
    connect(m_driver, &IDriver::scanRulePositionsReady,
            m_cScanEngine, &CScanEngine::setRulePositions);
    connect(m_driver, &IDriver::scanRulePositionsReady,
            m_homePage, &HomePage::setBScanRulePositions);
    connect(m_driver, &IDriver::scanRulePositionsReady,
            m_dataProcessor, &DataPacketProcessor::setScanRulePositions);

    // ── 数据包 → CScanEngine + DataPacketProcessor ──
    connect(m_driver, &IDriver::dataPacketReady,
            m_cScanEngine, &CScanEngine::processPacket);
    connect(m_driver, &IDriver::dataPacketReady,
            m_dataProcessor, &DataPacketProcessor::process);

    // ── DataPacketProcessor → 测量面板 ──
    connect(m_dataProcessor, &DataPacketProcessor::gateReadingsReady, this,
            [this](const GateReadings &r) {
        if (!m_measurePage) return;
        m_measurePage->updateGateReadings('A', r.aAmplitude, r.aSoundPathMm, r.angleDegrees, r.horizontalOffsetMm);
        m_measurePage->updateGateReadings('B', r.bAmplitude, r.bSoundPathMm, r.angleDegrees, r.horizontalOffsetMm);
    });

    // ── 报警 ──
    connect(m_dataProcessor, &DataPacketProcessor::alarmTriggered, this, [] {
#ifdef Q_OS_WIN
        Beep(2000, 500);
#else
        QApplication::beep();
#endif
    });

    // ── C扫成像 → 主页 ──
    connect(m_cScanEngine, &CScanEngine::imageUpdated, this,
            [this](const QVector<float> &img, int w, int h,
                   float spanStart, float spanEnd) {
        m_homePage->setCScanData(img, w, h);
        m_homePage->setCScanImageSpan(spanStart, spanEnd);
    });
    connect(m_cScanEngine, &CScanEngine::metricsChanged,
            m_homePage, &HomePage::updateCScanMetrics);
    connect(m_cScanEngine, &CScanEngine::scanCompleted, this, [this] {
        if (m_driver) m_driver->stopAcquisition();
        leaveMode(AppMode::Scanning);
        m_ioManager->setReplayActive(true);
        m_homePage->setCScanReplayMode(true);
        m_paramPage->finishScan();
        statusBar()->showMessage("C scan completed");
    });
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

bool MainWindow::enterMode(AppMode mode)
{
    if (m_appMode != AppMode::Idle && m_appMode != mode)
        return false;
    m_appMode = mode;
    return true;
}

void MainWindow::leaveMode(AppMode mode)
{
    if (m_appMode == mode)
        m_appMode = AppMode::Idle;
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
