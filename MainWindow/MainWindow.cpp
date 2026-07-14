#include "MainWindow.h"
#include "HomePage.h"
#include "ParamPage.h"
#include "MeasurePage.h"
#include "IDriver.h"
#include "CTSPA22SDriver.h"
#include "AppState.h"
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
#include <QMessageBox>
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
    m_ipLabel = new QLabel("IP: 192.168.0.51", header);
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

    m_connectBtn = makeBtn(QString::fromUtf8("连接设备"), "#0a6e3b", header);
    m_acquireBtn = makeBtn(QString::fromUtf8("开始采集"), "#0652a2", header);

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
    m_cScanThread = new QThread(this);
    m_cScanThread->setObjectName("CScanImagingThread");
    m_cScanEngine = new CScanEngine;
    m_cScanEngine->moveToThread(m_cScanThread);
    connect(m_cScanThread, &QThread::finished, m_cScanEngine, &QObject::deleteLater);
    m_cScanThread->start();
    m_homePage->setDriver(m_driver);
    m_paramPage->setDriver(m_driver);
    const bool paramsLoaded = m_paramPage->initializeParams();
    m_homePage->configureCScanView(m_paramPage->params());
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
        if (m_calibrating || m_encoderCalibrating || AppState::instance()->replayState()) {
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

    // C扫保存数据（ParamPage 已选好路径，MainWindow 执行实际写入）
    connect(m_paramPage, &ParamPage::saveDataRequested, this, [this](const QString &path) {
        if (!m_homePage || !m_paramPage) return;
        int w = 0, h = 0;
        QVector<float> data = m_homePage->getCScanData(w, h);
        if (data.isEmpty() || w <= 0 || h <= 0) {
            statusBar()->showMessage(QString::fromUtf8("无C扫数据可保存"));
            return;
        }
        QJsonObject paramsJson = m_paramPage->serializeParams();
        if (saveCScanFile(path, data, w, h, paramsJson,
                          m_cScanEngine->archivedPackets())) {
            statusBar()->showMessage(QString::fromUtf8("C扫数据已保存: %1 (%2×%3)")
                .arg(path).arg(w).arg(h));
        } else {
            statusBar()->showMessage(QString::fromUtf8("保存C扫数据失败"));
        }
    });

    // C扫回放数据
    connect(m_paramPage, &ParamPage::replayDataRequested, this, [this](const QString &path) {
        if (!m_homePage || !m_paramPage) return;
        if (m_calibrating || m_encoderCalibrating || m_cScanEngine->isScanning()) {
            statusBar()->showMessage(QString::fromUtf8("扫查或校准期间不能进入回放"));
            return;
        }
        int w = 0, h = 0;
        QJsonObject paramsJson;
        QVector<DataPacket> packets;
        QVector<ScanRule> loadedRules;
        QVector<float> data = loadCScanFile(path, w, h, paramsJson, packets, &loadedRules);
        if (data.isEmpty() || w <= 0 || h <= 0) {
            statusBar()->showMessage(QString::fromUtf8("加载C扫数据失败"));
            return;
        }
        m_homePage->setCScanReplayData(data, w, h, true);
        m_cScanEngine->setArchivedPackets(packets);
        if (!loadedRules.isEmpty()) m_cScanEngine->setScanRules(loadedRules);
        AppState::instance()->setReplayState(true);
        AppState::instance()->setReplayCurPos(0);
        // 恢复参数（静默加载，不触发硬件下发）
        m_paramPage->deserializeParams(paramsJson);
        m_paramPage->syncUiFromParams();
        m_homePage->configureCScanView(m_paramPage->params());
        statusBar()->showMessage(QString::fromUtf8("C扫回放: %1 (%2×%3)")
            .arg(path).arg(w).arg(h));
    });
    connect(m_homePage, &HomePage::cScanPositionSelected, this,
            [this](int line, int) {
        const auto &packets = m_cScanEngine->archivedPackets();
        if (line < 0 || line >= packets.size()) return;
        AppState::instance()->setReplayCurPos(line);
        m_homePage->showReplayPacket(packets[line], line,
            m_paramPage->params().curBeam, m_paramPage->params().rectify);
    });
    connect(m_homePage, &HomePage::cScanAnalysisRectChanged,
            m_paramPage, &ParamPage::setAnalysisRect);
    connect(m_paramPage, &ParamPage::cScanViewParamsChanged, this, [this] {
        m_homePage->configureCScanView(m_paramPage->params());
    });
    connect(m_paramPage, &ParamPage::calibrationRequested, this, [this](int item) {
        auto *ct = static_cast<CTSPA22SDriver*>(static_cast<IDriver*>(m_driver));
        if (!ct || !m_driver->isConnected()) {
            statusBar()->showMessage(QString::fromUtf8("校准需要先连接设备"));
            return;
        }
        if (m_cScanEngine->isScanning() || AppState::instance()->replayState()) {
            statusBar()->showMessage(QString::fromUtf8("请先退出扫查或回放状态"));
            return;
        }
        if (!m_calibrating) {
            m_calibrating = true;
            m_calibrationItem = item;
            const PAParams &p = m_paramPage->params();
            if (item == 2 || item == 3) {
                const bool useFifty = QMessageBox::question(
                    this, QString::fromUtf8("校准参考线"), QString::fromUtf8("默认参考线为80%，是否改为50%？"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
                m_calibrationTargetPercent = useFifty ? 50 : 80;
                m_homePage->setAScanCalibrationGuide(true, m_calibrationTargetPercent);
            } else {
                m_homePage->setAScanCalibrationGuide(false);
            }
            const int centerBeam = p.scanType == 0 ? 63
                : (p.scanType == 1 ? (p.probeCount - p.eleAperture + 1) / 2
                                   : p.probeCount / 2);
            if (item < 3) m_paramPage->setBeamNo(qBound(0, centerBeam, MaxBeams - 1));
            if (item == 2) ct->setACG(false, p);
            if (item == 3) ct->setTCG(false, p);
            statusBar()->showMessage(QString::fromUtf8("校准已开始，取得稳定回波后再次点击完成"));
            return;
        }
        if (!m_hasLatestPacket || m_latestPacket.beamCount <= 0) {
            statusBar()->showMessage(QString::fromUtf8("尚未收到有效采集数据"));
            return;
        }

        const PAParams &p = m_paramPage->params();
        const int beam = qBound(0, p.curBeam, m_latestPacket.beamCount - 1);
        const BeamWaveform &wave = m_latestPacket.beams[beam];
        auto pathMm = [&p](quint16 path) {
            return p.gateTrace[2] ? path * p.range / WaveSampleCount
                                  : path * S22_SP * p.lVelocity / 2000000.0;
        };
        if (m_calibrationItem == 0) {
            const double difference = pathMm(wave.path1) - pathMm(wave.path0);
            if (difference > 1.0)
                m_paramPage->setCalibratedVelocity(qRound(p.realDistance * p.lVelocity / difference));
            ct->setVelocity(m_paramPage->params().lVelocity);
            ct->setRange(m_paramPage->params().range);
        } else if (m_calibrationItem == 1) {
            const float delay = float((pathMm(wave.path0) - p.realDistance)
                                * 2000.0 / p.lVelocity + p.probeDelay);
            m_paramPage->setCalibratedProbeDelay(delay);
            if (p.scanType < 3) ct->setBeamDelay(); else ct->setCommonRDelay();
        } else if (m_calibrationItem == 2) {
            QVector<float> values(m_latestPacket.beamCount, 1.0f);
            for (int i = 0; i < m_latestPacket.beamCount; ++i) {
                const int amplitude = m_latestPacket.beams[i].amp0;
                values[i] = amplitude > 0
                    ? qBound(0.0f, m_calibrationTargetPercent * 2.5f / amplitude, 256.0f)
                    : 256.0f;
            }
            m_paramPage->setCalibratedACG(values);
            ct->setACG(true, m_paramPage->params());
        } else if (m_calibrationItem == 3) {
            ct->setTCG(true, p);
        }
        m_calibrating = false;
        m_calibrationItem = -1;
        m_homePage->setAScanCalibrationGuide(false);
        statusBar()->showMessage(QString::fromUtf8("校准完成并已应用"));
    });
    connect(m_paramPage, &ParamPage::encoderCalibrationRequested, this, [this] {
        if (!m_driver || !m_driver->isConnected()) {
            statusBar()->showMessage(QString::fromUtf8("编码器校准需要先连接设备"));
            return;
        }
        if (m_cScanEngine->isScanning() || AppState::instance()->replayState() || m_calibrating) {
            statusBar()->showMessage(QString::fromUtf8("请先退出扫查、回放或其他校准状态"));
            return;
        }
        const int position = int(AppState::instance()->encoderCount());
        if (!m_encoderCalibrating) {
            m_driver->resetEncoder(0);
            m_encoderCalibrationStart = 0;
            m_encoderCalibrating = true;
            statusBar()->showMessage(QString::fromUtf8("编码器校准已开始，请移动指定距离后再次点击"));
        } else {
            const int pulses = qAbs(position - m_encoderCalibrationStart);
            if (pulses > 0)
                m_paramPage->setCalibratedCoderDeg(m_paramPage->params().checkDistance / pulses);
            m_encoderCalibrating = false;
            statusBar()->showMessage(QString::fromUtf8("编码器精度：%1 mm/p")
                .arg(m_paramPage->params().coderDeg, 0, 'f', 4));
        }
    });
    connect(m_paramPage, &ParamPage::cScanPageRequested, this, [this] {
        const int count = m_cScanEngine->archivedPackets().size();
        if (count <= 0) return;
        const int maximumStart = qMax(0, count - 925);
        int pageStart = m_homePage->cScanPageStart() + 925;
        if (pageStart > maximumStart) pageStart = 0;
        m_homePage->setCScanPageStart(pageStart);
        const int line = qBound(0, pageStart + m_paramPage->params().anaLineX1, count - 1);
        AppState::instance()->setReplayCurPos(line);
        m_homePage->showReplayPacket(m_cScanEngine->archivedPackets()[line], line,
            m_paramPage->params().curBeam, m_paramPage->params().rectify);
    });
    connect(m_paramPage, &ParamPage::exitReplayRequested, this, [this] {
        AppState::instance()->setReplayState(false);
        AppState::instance()->setReplayCurPos(0);
        m_homePage->setCScanReplayMode(false);
        m_homePage->setCScanPageStart(0);
        m_homePage->selectCScanLine(-1);
        statusBar()->showMessage(QString::fromUtf8("已退出 C 扫回放"));
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
            m_acquireBtn->setText(QString::fromUtf8("开始采集"));
            m_acquireBtn->setStyleSheet(m_acquireBtn->styleSheet().replace("#0652a2", "#0652a2"));
        } else {
            m_driver->startAcquisition();
            m_acquireBtn->setText(QString::fromUtf8("停止采集"));
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

    statusBar()->showMessage(QString::fromUtf8("系统就绪"));
    if (!paramsLoaded)
        statusBar()->showMessage(QString::fromUtf8("默认参数文件加载失败，已使用程序内置参数"));

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


    // ── CTSPA22SDriver 信号全连接 ──
    {
        connect(m_driver, &IDriver::connectionChanged, this, [this](bool ok) {
            AppState::instance()->setConnected(ok);
            m_deviceLabel->setText(ok ? QString::fromUtf8("\u25CF 设备连接： 已连接")
                                      : QString::fromUtf8("\u25CF 设备连接： 未连接"));

            if (ok) {
                m_connectBtn->setText(QString::fromUtf8("断开设备"));
                m_connectBtn->setStyleSheet(m_connectBtn->styleSheet().replace("#0a6e3b", "#8b2020"));
                m_acquireBtn->setEnabled(true);
                m_acquireBtn->setText(QString::fromUtf8("开始采集"));
                m_acquireBtn->setStyleSheet(m_acquireBtn->styleSheet().replace("#c2590a", "#0652a2"));
                m_acquireBtn->setProperty("acquiring", false);
                m_paramPage->applyCurrentParams();
            } else {
                m_connectBtn->setText(QString::fromUtf8("连接设备"));
                m_connectBtn->setStyleSheet(m_connectBtn->styleSheet().replace("#8b2020", "#0a6e3b"));
                m_acquireBtn->setEnabled(false);
                m_acquireBtn->setText(QString::fromUtf8("开始采集"));
                m_acquireBtn->setStyleSheet(m_acquireBtn->styleSheet().replace("#c2590a", "#0652a2"));
                m_acquireBtn->setProperty("acquiring", false);
            }
        });
        connect(m_driver, &IDriver::statusChanged, this, [this](const QString &s) {
            statusBar()->showMessage(s);
        });
        connect(m_driver, &IDriver::errorOccurred, this, [this](const QString &e) {
            AppState::instance()->setConnected(false);
            statusBar()->showMessage(QString::fromUtf8("\u9519\u8BEF\uFF1A") + e);
        });
        connect(m_driver, &IDriver::temperatureReceived, this, [this](double t) {
            AppState::instance()->setTemperature(static_cast<float>(t));
            m_temperatureLabel->setText(QString::fromUtf8("温度: %1 °C").arg(t, 0, 'f', 1));
            updatePcBattery();
        });
        connect(m_driver, &IDriver::voltageReceived, this, [this](double v) {
            AppState::instance()->setInputVoltage(static_cast<float>(v));
            const int percent = qBound(0, qRound((v - 9.2) / 2.3 * 100.0), 100);
            m_paBatteryLabel->setText(QString::fromUtf8("PA电量: %1%").arg(percent));
            updatePcBattery();
        });
        connect(m_driver, &IDriver::encoderPositionChanged, this, [](int pos) {
            AppState::instance()->setEncoderCount(static_cast<uint32_t>(pos));
        });
        connect(m_driver, &IDriver::dataPacketReady,
                m_cScanEngine, &CScanEngine::processPacket);
        connect(m_driver, &IDriver::scanRulePositionsReady,
                m_cScanEngine, &CScanEngine::setRulePositions);
        connect(m_driver, &IDriver::scanRulePositionsReady, this,
                [this](const QVector<double> &positions) {
            m_scanRulePositions = positions.mid(0, MaxBeams);
        });
        connect(m_driver, &IDriver::dataPacketReady, this, [this](const DataPacket &packet) {
            m_latestPacket = packet;
            m_hasLatestPacket = packet.beamCount > 0;
            if (!m_hasLatestPacket || !m_measurePage || !m_paramPage) return;

            const PAParams &params = m_paramPage->params();
            const int beam = qBound(0, params.curBeam, packet.beamCount - 1);
            const BeamWaveform &wave = packet.beams[beam];
            double angle = params.angle;
            if (params.scanType == 0) {
                const double t = packet.beamCount > 1
                    ? static_cast<double>(beam) / (packet.beamCount - 1) : 0.0;
                angle = params.angleFrom + (params.angleTo - params.angleFrom) * t;
            }

            double horizontalOffset = 0.0;
            if (params.scanType == 0 && params.wedgeEnable != 0
                    && m_scanRulePositions.size() >= packet.beamCount) {
                const int centerBeam = qBound(0, 63, packet.beamCount - 1);
                horizontalOffset = m_scanRulePositions[beam]
                    - m_scanRulePositions[centerBeam];
            }
            auto soundPathMm = [&params](quint16 path) {
                return params.gateTrace[2]
                    ? path * params.range / WaveSampleCount
                    : path * S22_SP * params.lVelocity / 2000000.0;
            };
            m_measurePage->updateGateReadings(
                'A', qMin(100.0, wave.amp0 / 2.5), soundPathMm(wave.path0),
                angle, horizontalOffset);
            m_measurePage->updateGateReadings(
                'B', qMin(100.0, wave.amp1 / 2.5), soundPathMm(wave.path1),
                angle, horizontalOffset);

            // ── 蜂鸣报警 ──
            if (params.alarmSound != 0) {
                bool triggered = false;
                const int sound = params.alarmSound;  // 1=A, 2=B, 3=AB
                const int threshA = qRound(params.gateThreshold[0] * 2.5);
                const int threshB = qRound(params.gateThreshold[1] * 2.5);
                for (int b = 0; b < packet.beamCount && !triggered; ++b) {
                    if ((sound == 1 || sound == 3) && packet.beams[b].amp0 > threshA)
                        triggered = true;
                    if ((sound == 2 || sound == 3) && packet.beams[b].amp1 > threshB)
                        triggered = true;
                }
                if (triggered) {
#ifdef Q_OS_WIN
                    Beep(2000, 500);
#else
                    QApplication::beep();
#endif
                }
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
            AppState::instance()->setStartState(false);
            AppState::instance()->setReplayState(true);
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
