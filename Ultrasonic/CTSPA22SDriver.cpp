#include "CTSPA22SDriver.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostAddress>
#include <QEventLoop>
#include <QTimer>
#include <QtEndian>
#include <QtMath>
#include <memory>
#include <cstdint>
#include <cstring>
#include "Logging/Logger.h"

// ================================================================
// MTLDParser 实现
// ================================================================

MTLDParser::MTLDParser()
{
    m_buffer.reserve(600000);  // 预分配足够大
}

void MTLDParser::reset()
{
    m_state    = State::LookingForSync;
    m_buffer.clear();
    m_packetType.clear();
    m_payloadLen = 0;
}

QVector<DriverFrame> MTLDParser::feed(const QByteArray &data)
{
    QVector<DriverFrame> frames;
    m_buffer.append(data);

    while (true) {
        switch (m_state) {
        // ---- 查找同步头 0xAA 0x55 0xAA 0x55 ----
        case State::LookingForSync: {
            if (m_buffer.size() < 4) return frames;

            int idx = m_buffer.indexOf(
                QByteArray(reinterpret_cast<const char *>(SyncPattern), 4));
            if (idx < 0) {
                // 保留尾部最多 3 字节，防止同步头横跨两次 TCP 分片
                // 例：上一次结尾 ...AA 55 AA，本次以 55 开头
                const int keep = qMin(3, m_buffer.size());
                if (keep > 0)
                    m_buffer = m_buffer.right(keep);
                else
                    m_buffer.clear();
                return frames;
            }
            if (idx > 0)
                m_buffer.remove(0, idx);
            m_state = State::ReadingHeader;
            break;
        }

        // ---- 读取 16 字节包头 ----
        case State::ReadingHeader: {
            if (m_buffer.size() < HeaderSize) return frames;

            // 跳过 4 字节同步头，类型标识在偏移 4（如 "Ts22w"）
            m_packetType = QString::fromLatin1(
                m_buffer.mid(4, 5).constData(), 5).trimmed();

            // 字节 12-14: 载荷长度（little-endian 3字节）
            const uint8_t *p = reinterpret_cast<const uint8_t *>(
                m_buffer.constData());
            m_payloadLen = p[12] + (p[13] << 8) + (p[14] << 16) - HeaderSize;

            if (m_payloadLen < 0 || m_payloadLen > 550000) {
                // 非法长度，丢弃同步头重新找
                m_buffer.remove(0, 1);
                m_state = State::LookingForSync;
                break;
            }

            m_state = State::ReadingPayload;
            break;
        }

        // ---- 等待载荷接收完整 ----
        case State::ReadingPayload: {
            if (m_buffer.size() < HeaderSize + m_payloadLen) return frames;

            DriverFrame frame;
            frame.type    = m_packetType;
            frame.payload = m_buffer.mid(HeaderSize, m_payloadLen);
            frames.append(frame);

            m_buffer.remove(0, HeaderSize + m_payloadLen);
            m_state = State::LookingForSync;
            break;
        }
        } // switch
    }
}

DataPacket MTLDParser::parseWaveforms(const QByteArray &payload, int aDataLen,
                                       float range)
{
    DataPacket pkt;

    // 紧凑格式参数（与 MFC CTSPA22SView.cpp:973-987 一致）
    const int rawWaveLen = (aDataLen == 0 ? 100 : (aDataLen == 1 ? 200 : 400));
    const int beamDataLen = (aDataLen == 0 ? 128 : (aDataLen == 1 ? 256 : 512));
    const int count = qMin(payload.size() / beamDataLen, MaxBeams);
    pkt.beamCount = count;

    for (int b = 0; b < count; ++b) {
        const uint8_t *src = reinterpret_cast<const uint8_t *>(
            payload.constData() + b * beamDataLen);
        const uint8_t *meta = src + rawWaveLen;  // 元数据紧接波形采样点之后

        BeamWaveform &wf = pkt.beams[b];

        // 波形采样点插值到 400 点（精确匹配 MFC CTSPA22SView.cpp:1011-1055）
        if (aDataLen >= 2) {
            std::memcpy(wf.waveP, src, 400);
            // MFC: 400 点模式下若量程 < 10mm，前 200 有效点插值为 400
            if (range < 10.0f) {
                for (int i = 199; i >= 0; --i)
                    wf.waveP[i * 2] = wf.waveP[i];
                for (int i = 0; i < 199; ++i)
                    wf.waveP[i * 2 + 1] = uint8_t((wf.waveP[i * 2] + wf.waveP[i * 2 + 2]) / 2);
            }
        } else if (aDataLen == 1) {
            // 200→400：偶数位置放原始点，奇数位置取相邻均值
            wf.waveP[0] = src[0];
            for (int i = rawWaveLen - 1; i > 0; --i)
                wf.waveP[i * 2] = src[i];
            for (int i = 0; i < rawWaveLen - 1; ++i)
                wf.waveP[i * 2 + 1] = uint8_t((wf.waveP[i * 2] + wf.waveP[i * 2 + 2]) / 2);
            wf.waveP[399] = wf.waveP[398];
        } else {
            // 100→400：4倍扩展 → 中点 → 四分点 → 尾部填充（与 MFC 一致）
            wf.waveP[0] = src[0];
            for (int i = rawWaveLen - 1; i > 0; --i)
                wf.waveP[i * 4] = src[i];
            for (int i = 0; i < rawWaveLen - 1; ++i)
                wf.waveP[i * 4 + 2] = uint8_t((wf.waveP[i * 4] + wf.waveP[i * 4 + 4]) / 2);
            for (int i = 0; i < rawWaveLen - 1; ++i) {
                wf.waveP[i * 4 + 1] = uint8_t((wf.waveP[i * 4] + wf.waveP[i * 4 + 2]) / 2);
                wf.waveP[i * 4 + 3] = uint8_t((wf.waveP[i * 4 + 2] + wf.waveP[i * 4 + 4]) / 2);
            }
            wf.waveP[397] = wf.waveP[396];
            wf.waveP[398] = wf.waveP[396];
            wf.waveP[399] = wf.waveP[396];
        }

        // 元数据：显式小端解析（字段偏移对应 MFC WAVEFORM 结构 400-427 范围）
        wf.frame   = qFromLittleEndian<quint16>(meta);
        wf.channel = meta[2];
        wf.path0   = qFromLittleEndian<quint16>(meta + 3);
        wf.amp0    = meta[5];
        wf.path1   = qFromLittleEndian<quint16>(meta + 6);
        wf.amp1    = meta[8];
        wf.path2   = qFromLittleEndian<quint16>(meta + 9);
        wf.amp2    = meta[11];
        wf.encFwd  = qFromLittleEndian<quint32>(meta + 12);
        wf.encRvs  = qFromLittleEndian<quint32>(meta + 20);

        pkt.frameIndex = wf.frame;
    }

    return pkt;
}

QVector<int> MTLDParser::parseTFMImage(const QByteArray &payload)
{
    QVector<int> image(256 * 256);
    if (payload.size() >= 256 * 256 * 4) {
        const int32_t *src = reinterpret_cast<const int32_t *>(
            payload.constData());
        for (int i = 0; i < 256 * 256; ++i)
            image[i] = qAbs(src[i]);
    }
    return image;
}

// ================================================================
// CTSPA22SDriver 实现
// ================================================================

CTSPA22SDriver::CTSPA22SDriver(QObject *parent)
    : IDriver(parent)
{
    m_cmdSocket  = new QTcpSocket(this);
    m_dataSocket = new QTcpSocket(this);
    m_monitorTimer = new QTimer(this);
    m_monitorTimer->setInterval(30000);
    connect(m_monitorTimer, &QTimer::timeout, this, [this] {
        if (!m_connected) return;
        queryTemperature();
        queryVoltage();
    });

    // 命令通道信号
    connect(m_cmdSocket, &QTcpSocket::connected,
            this, &CTSPA22SDriver::onCmdConnected);
    connect(m_cmdSocket, &QTcpSocket::disconnected,
            this, &CTSPA22SDriver::onCmdDisconnected);
    connect(m_cmdSocket, &QAbstractSocket::errorOccurred,
            this, &CTSPA22SDriver::onCmdError);

    // 数据通道信号
    connect(m_dataSocket, &QTcpSocket::connected,
            this, &CTSPA22SDriver::onDataConnected);
    connect(m_dataSocket, &QTcpSocket::disconnected,
            this, &CTSPA22SDriver::onDataDisconnected);
    connect(m_dataSocket, &QAbstractSocket::errorOccurred,
            this, &CTSPA22SDriver::onDataError);
    connect(m_dataSocket, &QTcpSocket::readyRead,
            this, &CTSPA22SDriver::onDataReadyRead);
}

CTSPA22SDriver::~CTSPA22SDriver()
{
    disconnectDevice();
}

// ================================================================
// 连接管理
// ================================================================

bool CTSPA22SDriver::connectDevice(const QString &ip,
                                     quint16 cmdPort,
                                     quint16 dataPort)
{
    PA_LOG_INFO("NETWORK", QStringLiteral("Connect requested ip=%1 cmdPort=%2 dataPort=%3")
                .arg(ip).arg(cmdPort).arg(dataPort));
    if (m_connected) {
        emit statusChanged(QString::fromUtf8("已经连接"));
        return true;
    }

    if (m_cmdSocket->state() != QAbstractSocket::UnconnectedState)
        m_cmdSocket->abort();
    if (m_dataSocket->state() != QAbstractSocket::UnconnectedState)
        m_dataSocket->abort();

    emit statusChanged(QString::fromUtf8("正在连接 %1 ...").arg(ip));

    // 异步连接：QEventLoop 允许事件循环继续运行（不冻结 UI）
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool cmdOk = false, dataOk = false;
    QString cmdErr, dataErr;

    // 以 &loop 为 context：loop 销毁时所有临时连接自动断开，避免悬空引用
    connect(m_cmdSocket, &QTcpSocket::connected, &loop, [&] {
        cmdOk = true;
        m_dataSocket->connectToHost(QHostAddress(ip), dataPort);
    });
    connect(m_dataSocket, &QTcpSocket::connected, &loop, [&] {
        dataOk = true;
        loop.quit();
    });
    auto onError = [&](const QString &err) {
        if (!cmdOk) { cmdErr = err; m_cmdSocket->abort(); }
        else        { dataErr = err; m_dataSocket->abort(); }
        loop.quit();
    };
    connect(m_cmdSocket, &QAbstractSocket::errorOccurred, &loop,
            [&] { onError(m_cmdSocket->errorString()); });
    connect(m_dataSocket, &QAbstractSocket::errorOccurred, &loop,
            [&] { onError(m_dataSocket->errorString()); });
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    m_cmdSocket->connectToHost(QHostAddress(ip), cmdPort);
    timeout.start(10000);
    loop.exec();

    if (!cmdOk || !dataOk) {
        if (!cmdOk) m_cmdSocket->abort();
        if (!dataOk) { m_dataSocket->abort(); m_cmdSocket->abort(); }
        emit errorOccurred(QString::fromUtf8("连接失败 (%1): %2")
                           .arg(ip)
                           .arg(cmdOk ? dataErr : cmdErr));
        return false;
    }

    // 状态已由 onCmdConnected / onDataConnected 设置并 emit 过 connectionChanged
    m_parser.reset();
    m_lastCompTemp = -100;
    m_monitorTimer->start();
    QTimer::singleShot(0, this, [this] {
        if (!m_connected) return;
        queryTemperature();
        queryVoltage();
    });

    emit statusChanged(QString::fromUtf8("已连接 %1 (命令:%2 数据:%3)")
                       .arg(ip).arg(cmdPort).arg(dataPort));
    return true;
}

bool CTSPA22SDriver::connectDevice(ConnectionMode mode)
{
    m_connectionMode = mode;
    const char *ip = (mode == ConnectionMode::Wireless)
                     ? DefaultWifiIP : DefaultWiredIP;
    return connectDevice(QString::fromLatin1(ip),
                         DefaultCmdPort, DefaultDataPort);
}

void CTSPA22SDriver::disconnectDevice()
{
    PA_LOG_INFO("NETWORK", QStringLiteral("Disconnect requested connected=%1 acquiring=%2")
                .arg(m_connected).arg(m_acquiring));
    // 停止采集（仅尝试发送 stop 命令，不等响应）
    if (m_acquiring) {
        QJsonObject cmd;
        cmd["scan"] = 0;
        sendJsonCommand(cmd, false);
        m_acquiring = false;
    }

    m_monitorTimer->stop();

    // abort() 触发 disconnected 信号 → onDataDisconnected/onCmdDisconnected 已处理状态
    if (m_dataSocket->state() != QAbstractSocket::UnconnectedState)
        m_dataSocket->abort();
    if (m_cmdSocket->state() != QAbstractSocket::UnconnectedState)
        m_cmdSocket->abort();

    // 若 socket 已处于断开状态（abort 未触发信号），在此统一处理
    if (m_connected) {
        m_connected = false;
        m_acquiring = false;
        m_hasLastWaveFrame = false;
        emit connectionChanged(false);
    }
    m_cmdReady  = false;
    m_dataReady = false;

    emit statusChanged(QString::fromUtf8("已断开"));
}

bool CTSPA22SDriver::isConnected() const
{
    return m_connected;
}

// ================================================================
// 采集控制
// ================================================================

void CTSPA22SDriver::startAcquisition()
{
    PA_LOG_INFO("ACQUISITION", QStringLiteral("Start requested scanType=%1 aDataLen=%2 range=%3")
                .arg(m_scanType).arg(m_aDataLen).arg(m_range));
    if (!m_connected) {
        emit statusChanged(QString::fromUtf8("未连接，无法启动采集"));
        return;
    }

    // 下发扫查类型（触发硬件开始组帧），不等待命令响应
    // 因为 {"scan": 1} 后数据通道立即开始灌帧，
    // 命令通道可能不返回 JSON，等待会导致 3 秒 UI 冻结
    m_hasLastWaveFrame = false;
    m_lastReportedFrameDiff = -1;
    m_droppedFrames = 0;
    emit frameStatisticsChanged(0, 0);

    sendJsonCommand(buildScanTypeCommand(m_scanType), false);

    // 启动数据采集
    QJsonObject cmd;
    cmd["scan"] = 1;
    sendJsonCommand(cmd, false);  // scan=1 无 JSON 响应

    m_acquiring = true;
    emit statusChanged(QString::fromUtf8("开始采集"));
}

void CTSPA22SDriver::stopAcquisition()
{
    PA_LOG_INFO("ACQUISITION", QStringLiteral("Stop requested droppedFrames=%1")
                .arg(m_droppedFrames));
    if (!m_acquiring) return;

    QJsonObject cmd;
    cmd["scan"] = 0;
    sendJsonCommand(cmd, false);  // scan=0 无 JSON 响应

    m_acquiring = false;
    emit statusChanged(QString::fromUtf8("停止采集"));
}

// ================================================================
// 数据接收处理
// ================================================================

void CTSPA22SDriver::onDataReadyRead()
{
    QByteArray data = m_dataSocket->readAll();

    QVector<DriverFrame> frames = m_parser.feed(data);

    for (const auto &frame : frames)
        processFrame(frame);
}

void CTSPA22SDriver::processFrame(const DriverFrame &frame)
{

    if (frame.type == "Ts22w") {
        // === 常规 PA 扫描 ===
        auto pkt = std::make_shared<DataPacket>(MTLDParser::parseWaveforms(frame.payload, m_aDataLen, m_range));

        if (pkt->beamCount < 1) {
            PA_LOG_WARNING("DATA", QStringLiteral("Empty waveform packet payloadBytes=%1")
                           .arg(frame.payload.size()));
            return;
        }

        if ((pkt->frameIndex % 100) == 0) {
            PA_LOG_DEBUG("DATA", QStringLiteral("Wave frame=%1 beams=%2 payloadBytes=%3 aDataLen=%4 range=%5")
                         .arg(pkt->frameIndex).arg(pkt->beamCount).arg(frame.payload.size())
                         .arg(m_aDataLen).arg(m_range));
        }

        const quint16 currentFrame = pkt->beams[0].frame;
        int frameDiff = 0;
        if (m_hasLastWaveFrame) {
            frameDiff = static_cast<quint16>(currentFrame - m_lastWaveFrame);
            if (frameDiff > 32768)
                frameDiff = 0;
            if (frameDiff > 1) {
                m_droppedFrames += static_cast<quint64>(frameDiff - 1);
                PA_LOG_WARNING("DATA", QStringLiteral("Dropped frames current=%1 previous=%2 diff=%3 total=%4")
                               .arg(currentFrame).arg(m_lastWaveFrame).arg(frameDiff)
                               .arg(m_droppedFrames));
            }
        }
        m_lastWaveFrame = currentFrame;
        m_hasLastWaveFrame = true;
        if (frameDiff != m_lastReportedFrameDiff || frameDiff > 1) {
            emit frameStatisticsChanged(frameDiff, m_droppedFrames);
            m_lastReportedFrameDiff = frameDiff;
        }

        emit dataPacketReady(pkt);

        // --- A 扫描：发送当前声束波形 ---
        {
            int curBeam = qBound(0, m_beamCount / 2, pkt->beamCount - 1);
            const auto &wf = pkt->beams[curBeam];
            QVector<double> wave(WaveSampleCount);

            // 按检波模式归一化：
            //   RF(3):   uint8->int8 偏移 → [-1.0, +1.0] 双极性
            //   其他:    uint8 → [0.0, 1.0] 单极性
            const bool isRF = (m_rectify == 3);
            for (int i = 0; i < WaveSampleCount; ++i) {
                if (isRF)
                    wave[i] = (static_cast<int>(wf.waveP[i]) - 128) / 128.0;
                else
                    wave[i] = wf.waveP[i] / 255.0;
            }
            emit waveformReady(wave, curBeam, wf.frame, m_rectify);
        }

        // --- 全部声束波形（供 B 扫 softwareImaging 使用） ---
        {
            QVector<QVector<double>> allWaves(pkt->beamCount);
            const bool isRF = (m_rectify == 3);
            for (int b = 0; b < pkt->beamCount; ++b) {
                allWaves[b].resize(WaveSampleCount);
                for (int i = 0; i < WaveSampleCount; ++i) {
                    if (isRF)
                        allWaves[b][i] = (static_cast<int>(pkt->beams[b].waveP[i]) - 128) / 128.0;
                    else
                        allWaves[b][i] = pkt->beams[b].waveP[i] / 255.0;
                }
            }
            emit multiBeamWaveformsReady(allWaves);
        }

        // --- 闸门读数 ---
        {
            int curBeam = qBound(0, m_beamCount / 2, pkt->beamCount - 1);
            const auto &wf = pkt->beams[curBeam];
            emit gateReadingsReady('A', wf.amp0, wf.path0);
            emit gateReadingsReady('B', wf.amp1, wf.path1);
            emit gateReadingsReady('C', wf.amp2, wf.path2);
        }

        // --- 编码器 ---
        {
            int curBeam = qBound(0, m_beamCount / 2, pkt->beamCount - 1);
            const auto &wf = pkt->beams[curBeam];
            // 正向为主，同时考虑反向
            int pos = static_cast<int>(wf.encFwd) - static_cast<int>(wf.encRvs);
            emit encoderPositionChanged(pos);
        }

    } else if (frame.type == "Ts22t") {
        static quint64 tfmFrameCount = 0;
        ++tfmFrameCount;
        if ((tfmFrameCount % 100) == 0)
            PA_LOG_DEBUG("DATA", QStringLiteral("TFM frameCount=%1 payloadBytes=%2")
                         .arg(tfmFrameCount).arg(frame.payload.size()));
        // === TFM 扫描 ===
        QVector<int> image = MTLDParser::parseTFMImage(frame.payload);

        // 应用 TFM 增益（如果有的话）
        emit tfmImageReady(image);
    }
}

// ================================================================
// 通道事件处理
// ================================================================

void CTSPA22SDriver::onCmdConnected()
{
    PA_LOG_INFO("NETWORK", QStringLiteral("Command channel connected"));
    m_cmdReady = true;
    if (m_cmdReady && m_dataReady && !m_connected) {
        m_connected = true;
        emit connectionChanged(true);
        emit statusChanged(QString::fromUtf8("双通道已连接"));
    }
}

void CTSPA22SDriver::onCmdDisconnected()
{
    PA_LOG_WARNING("NETWORK", QStringLiteral("Command channel disconnected"));
    m_cmdReady = false;
    if (m_connected) {
        // 命令通道断开意味着整个连接不可用，做完整清理
        m_connected = false;
        m_acquiring = false;
        m_monitorTimer->stop();
        m_dataSocket->abort();
        m_dataReady = false;
        emit connectionChanged(false);
        emit statusChanged(QString::fromUtf8("命令通道断开，连接已断开"));
    }
}

void CTSPA22SDriver::onCmdError(QAbstractSocket::SocketError err)
{
    PA_LOG_ERROR("NETWORK", QStringLiteral("Command channel error code=%1 text=%2")
                 .arg(static_cast<int>(err)).arg(m_cmdSocket->errorString()));
    const QString errStr = m_cmdSocket->errorString();
    // 严重错误：连接被拒绝 / 远端关闭 / 网络不可达 — 自动断开
    switch (err) {
    case QAbstractSocket::ConnectionRefusedError:
    case QAbstractSocket::RemoteHostClosedError:
    case QAbstractSocket::NetworkError:
    case QAbstractSocket::SocketTimeoutError:
        if (m_connected || m_cmdReady) {
            m_cmdReady = false;
            m_connected = false;
            m_acquiring = false;
            m_monitorTimer->stop();
            emit connectionChanged(false);
        }
        break;
    default:
        break;
    }
    emit errorOccurred(QString::fromUtf8("命令通道错误: %1").arg(errStr));
}

void CTSPA22SDriver::onDataConnected()
{
    PA_LOG_INFO("NETWORK", QStringLiteral("Data channel connected"));
    m_dataReady = true;
    if (m_cmdReady && m_dataReady && !m_connected) {
        m_connected = true;
        emit connectionChanged(true);
        emit statusChanged(QString::fromUtf8("双通道已连接"));
    }
}

void CTSPA22SDriver::onDataDisconnected()
{
    PA_LOG_WARNING("NETWORK", QStringLiteral("Data channel disconnected"));
    m_dataReady = false;
    if (m_connected) {
        // 数据通道断开意味着无法接收波形，做完整清理
        m_connected = false;
        m_acquiring = false;
        m_monitorTimer->stop();
        m_hasLastWaveFrame = false;
        emit connectionChanged(false);
        emit statusChanged(QString::fromUtf8("数据通道断开，连接已断开"));
    }
}

void CTSPA22SDriver::onDataError(QAbstractSocket::SocketError err)
{
    PA_LOG_ERROR("NETWORK", QStringLiteral("Data channel error code=%1 text=%2")
                 .arg(static_cast<int>(err)).arg(m_dataSocket->errorString()));
    const QString errStr = m_dataSocket->errorString();
    // 严重错误：自动断开
    switch (err) {
    case QAbstractSocket::ConnectionRefusedError:
    case QAbstractSocket::RemoteHostClosedError:
    case QAbstractSocket::NetworkError:
    case QAbstractSocket::SocketTimeoutError:
        if (m_connected || m_dataReady) {
            m_dataReady = false;
            m_connected = false;
            m_acquiring = false;
            m_monitorTimer->stop();
            m_hasLastWaveFrame = false;
            emit connectionChanged(false);
        }
        break;
    default:
        break;
    }
    emit errorOccurred(QString::fromUtf8("数据通道错误: %1").arg(errStr));
}

// ================================================================
// 命令发送工具
// ================================================================

QJsonObject CTSPA22SDriver::sendJsonCommand(const QJsonObject &obj, bool waitForResp)
{
    if (!m_cmdReady || m_cmdSocket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QString::fromUtf8("命令通道未就绪，无法发送命令"));
        return {};
    }

    // RPC 重入保护：等待响应期间禁止发送任何命令
    // QEventLoop::exec() 可能分发嵌套事件 → 用户操作 → 再次进入本函数
    if (m_rpcBusy) {
        PA_LOG_WARNING("COMMAND", QStringLiteral("Rejected while RPC busy request=%1")
                       .arg(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))));
        emit errorOccurred(QString::fromUtf8("RPC 正忙，上一命令尚未完成"));
        return {};
    }

    // 构造并发送 JSON
    if (m_cmdSocket->bytesAvailable() > 0) {
        QByteArray stale = m_cmdSocket->readAll();
        int lastSep = stale.lastIndexOf('\x1E');
        if (lastSep >= 0 && lastSep < stale.size() - 1)
            m_cmdReadBuffer = stale.mid(lastSep + 1);
        else if (lastSep < 0 && !stale.isEmpty())
            m_cmdReadBuffer = stale;
    }

    QJsonDocument doc(obj);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    PA_LOG_INFO("COMMAND", QStringLiteral("TX waitForResponse=%1 bytes=%2 json=%3")
                .arg(waitForResp).arg(json.size()).arg(QString::fromUtf8(json)));
    json.append('\x1E');

    const qint64 written = m_cmdSocket->write(json);
    if (written != json.size()) {
        PA_LOG_ERROR("COMMAND", QStringLiteral("TX failed written=%1 expected=%2")
                     .arg(written).arg(json.size()));
        emit errorOccurred(QString::fromUtf8("命令发送失败: write 返回 %1 (预期 %2)")
                           .arg(written).arg(json.size()));
        return {};
    }
    m_cmdSocket->flush();

    if (!waitForResp)
        return {};

    // 等待响应期间持有 busy 标记，防止重入
    m_rpcBusy = true;
    QJsonObject result = waitForResponse(3000);
    m_rpcBusy = false;
    PA_LOG_INFO("COMMAND", QStringLiteral("RX completed success=%1 response=%2")
                .arg(!result.isEmpty())
                .arg(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))));
    return result;
}

QJsonObject CTSPA22SDriver::waitForResponse(int timeoutMs)
{
    if (m_cmdSocket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QString::fromUtf8("等待响应时命令通道已断开"));
        return {};
    }

    // 先尝试从持久缓冲提取完整帧
    if (!m_cmdReadBuffer.isEmpty()) {
        int sep = m_cmdReadBuffer.indexOf('\x1E');
        if (sep >= 0) {
            QByteArray frame = m_cmdReadBuffer.left(sep);
            m_cmdReadBuffer = m_cmdReadBuffer.mid(sep + 1);
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(frame, &err);
            if (err.error == QJsonParseError::NoError)
                return doc.object();
        }
    }

    // QEventLoop 替代 waitForReadyRead：协议层等待响应，但不冻结 UI 事件循环
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QJsonObject result;
    bool ok = false, timedOut = false;

    QMetaObject::Connection conn = connect(m_cmdSocket, &QTcpSocket::readyRead, this, [&] {
        m_cmdReadBuffer.append(m_cmdSocket->readAll());
        while (!m_cmdReadBuffer.isEmpty()) {
            int sep = m_cmdReadBuffer.indexOf('\x1E');
            if (sep < 0) break;
            QByteArray frame = m_cmdReadBuffer.left(sep);
            m_cmdReadBuffer = m_cmdReadBuffer.mid(sep + 1);
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(frame, &err);
            if (err.error == QJsonParseError::NoError) {
                result = doc.object();
                ok = true;
                loop.quit();
                return;
            }
        }
    });
    connect(&timer, &QTimer::timeout, &loop, [&] { timedOut = true; loop.quit(); });
    connect(m_cmdSocket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();
    disconnect(conn);

    if (ok)
        return result;
    if (timedOut)
        PA_LOG_ERROR("COMMAND", QStringLiteral("RPC timeout timeoutMs=%1").arg(timeoutMs));
    if (timedOut)
        emit errorOccurred(QString::fromUtf8("命令响应超时 (%1ms)").arg(timeoutMs));
    else if (m_cmdSocket->state() != QAbstractSocket::ConnectedState)
        emit errorOccurred(QString::fromUtf8("等待响应时连接断开"));
    return {};
}

// ================================================================
// 命令构造
// ================================================================

QJsonObject CTSPA22SDriver::buildScanTypeCommand(int type)
{
    QJsonObject cmd;

    switch (type) {
    case 0: {  // S-Scan
        QJsonObject sscan;
        sscan["probe_count"]    = m_probeCount;
        sscan["probe_freq"]     = m_probeFreq;
        sscan["probe_pitch"]    = m_probePitch;
        sscan["ele_start"]      = m_eleStart;
        sscan["ele_end"]        = m_eleEnd;
        sscan["ele_aperture"]   = m_eleAperture;
        sscan["angle_from"]     = m_angleFrom;
        sscan["angle_to"]       = m_angleTo;
        sscan["focus"]          = m_focus;
        sscan["velocity"]       = m_velocity;
        sscan["wedge_enable"]   = m_wedgeEnable ? 1 : 0;
        sscan["wedge_angle"]    = m_wedgeAngle;
        sscan["wedge_velocity"] = m_wedgeVelocity;
        sscan["wedge_height"]   = m_wedgeHeight;
        cmd["pa_sscan"] = sscan;
        break;
    }
    case 1: {  // L-Scan
        QJsonObject lscan;
        lscan["probe_count"]    = m_probeCount;
        lscan["probe_freq"]     = m_probeFreq;
        lscan["probe_pitch"]    = m_probePitch;
        lscan["ele_start"]      = m_eleStart;
        lscan["ele_end"]        = m_eleEnd;
        lscan["ele_aperture"]   = m_eleAperture;
        lscan["angle"]          = m_angleFrom;
        lscan["focus"]          = m_focus;
        lscan["velocity"]       = m_velocity;
        lscan["wedge_enable"]   = m_wedgeEnable ? 1 : 0;
        lscan["wedge_angle"]    = m_wedgeAngle;
        lscan["wedge_velocity"] = m_wedgeVelocity;
        lscan["wedge_height"]   = m_wedgeHeight;
        cmd["pa_lscan"] = lscan;
        break;
    }
    case 2: {  // CL-Scan
        QJsonObject clscan;
        clscan["probe_count"]  = m_probeCount;
        clscan["probe_pitch"]  = m_probePitch;
        clscan["ele_aperture"] = m_eleAperture;
        clscan["inner_r"]      = m_focus;
        clscan["outer_r"]      = m_focus + 50.0;
        clscan["focus"]        = m_focus;
        clscan["velocity"]     = m_velocity;
        cmd["pa_clscan"] = clscan;
        break;
    }
    case 3: {  // TFM
        QJsonObject tfm;
        tfm["probe_count"]     = m_probeCount;
        tfm["probe_pitch"]     = m_probePitch;
        tfm["ele_start"]       = m_eleStart;
        tfm["ele_end"]         = m_eleEnd;
        tfm["pixel_size"]      = 0.5;
        tfm["dimension_x"]     = 128.0;
        tfm["dimension_y"]     = 128.0;
        tfm["offset_x"]        = 0.0;
        tfm["offset_y"]        = 0.0;
        tfm["piece_thickness"] = 50.0;
        tfm["velocity"]        = m_velocity;
        cmd["tfm_llscan"] = tfm;
        break;
    }
    default:
        break;
    }

    return cmd;
}

QJsonObject CTSPA22SDriver::buildGainCommand(float analogGain)
{
    // 根据 CTSPA22S 增益表将 dB 值拆分为 vga1/vga2 两级
    float vga1, vga2;
    if (analogGain <= 20.0f) {
        vga1 = analogGain;
        vga2 = 0.0f;
    } else if (analogGain <= 40.0f) {
        vga1 = 20.0f;
        vga2 = analogGain - 20.0f;
    } else {
        vga1 = qMin(analogGain * 0.5f, 30.0f);
        vga2 = analogGain - vga1;
    }

    QJsonObject gainObj;
    gainObj["vga1"] = vga1;
    gainObj["vga2"] = vga2;
    QJsonObject gain;
    gain["gain"] = gainObj;
    return gain;
}

QJsonObject CTSPA22SDriver::buildDGainCommand(float digitalGain)
{
    // MFC 版: DGain + 36.0 + 温度补偿
    float dgain = digitalGain + 36.0f;
    if (m_tempCorrect)
        dgain += (m_xadcTemp - 30) * 0.03f;
    QJsonObject obj;
    obj["dgain"] = dgain;
    return obj;
}

QJsonObject CTSPA22SDriver::buildVoltageCommand(int level)
{
    // 110V=1, 40V=7, 20V=17（MFC 版查表值）
    int volt;
    switch (level) {
    case 0: volt = 1;  break;  // 110V
    case 1: volt = 7;  break;  // 40V
    case 2: volt = 17; break;  // 20V
    default: volt = 7; break;
    }
    QJsonObject obj;
    obj["volt"] = volt;
    return obj;
}

QJsonObject CTSPA22SDriver::buildGateCommand(char gate, float start,
                                              float width, float threshold,
                                              const QString &measureType)
{
    QJsonObject gateObj;
    gateObj["name"] = QString(gate);

    QJsonObject ctrl;
    ctrl["mode"] = measureType;

    QJsonArray params;
    params.append(start);
    params.append(width);
    params.append(threshold);
    ctrl["param"] = params;

    gateObj["ctrl"] = ctrl;

    QJsonObject obj;
    obj["gate"] = gateObj;
    return obj;
}

QJsonObject CTSPA22SDriver::buildRangeCommand(float range)
{
    // range_ratio = Range * S22_SP(=10) / 声速 * 1000000 / 2
    float ratio = range * 10.0f / m_velocity * 1000000.0f / 2.0f;
    QJsonObject obj;
    obj["range_ratio"] = ratio;
    return obj;
}

// ================================================================
// 参数设置方法
// ================================================================

void CTSPA22SDriver::setScanType(int type)
{
    m_scanType = type;
    if (m_acquiring) {
        stopAcquisition();
    }
    const QJsonObject response = sendJsonCommand(buildScanTypeCommand(type), true);
    QJsonArray result = response.value("result").toArray();
    if (!result.isEmpty()) {
        QVector<double> positions;
        positions.reserve(result.size());
        for (const QJsonValue &value : result)
            positions.append(value.toDouble());
        emit scanRulePositionsReady(positions);
    }
    if (m_acquiring)
        startAcquisition();
    emit statusChanged(QString::fromUtf8("扫查类型: %1").arg(
        type == 0 ? QString::fromUtf8("S扫") : type == 1 ? QString::fromUtf8("L扫") : type == 2 ? QString::fromUtf8("CL扫") : "TFM"));
}

void CTSPA22SDriver::setAnalogGain(float dB)
{
    m_analogGain = dB;
    sendJsonCommand(buildGainCommand(dB));
}

void CTSPA22SDriver::setDigitalGain(float dB)
{
    m_digitalGain = dB;
    sendJsonCommand(buildDGainCommand(dB));
}

void CTSPA22SDriver::setTemperatureCompensation(bool enabled)
{
    m_tempCorrect = enabled;
    m_lastCompTemp = -100;
    if (m_connected)
        sendJsonCommand(buildDGainCommand(m_digitalGain));
}

void CTSPA22SDriver::setHighVoltage(int level)
{
    m_highVoltage = level;
    sendJsonCommand(buildVoltageCommand(level));
}

void CTSPA22SDriver::setPulseWidth(int width)
{
    m_pulseWidth = width;
    QJsonObject obj;
    obj["pulse_width"] = width;
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setPRF(int prf)
{
    m_prf = prf;
    QJsonObject obj;
    obj["prf"] = prf;
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setRange(float range)
{
    m_range = range;
    sendJsonCommand(buildRangeCommand(range));
}

void CTSPA22SDriver::setRectify(int mode)
{
    m_rectify = mode;
    QJsonObject obj;
    static const char *modes[] = {"full_wave", "pos_wave", "neg_wave", "rf"};
    obj["rectify"] = (mode >= 0 && mode < 4) ? modes[mode] : "full_wave";
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setFilter(int filter)
{
    m_filter = filter;
    QJsonObject obj;
    obj["dfilter"] = filter;
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setADataLen(int len)
{
    m_aDataLen = len;
    QJsonObject obj;
    int sampleCount = (len == 0 ? 128 : (len == 1 ? 256 : 512));
    obj["a_data_len"] = sampleCount;
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setASmooth(bool enable)
{
    QJsonObject obj;
    obj["a_smooth"] = enable ? 1 : 0;
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setVideoDetect(bool enable)
{
    QJsonObject obj;
    obj["video_detect"] = enable ? 1 : 0;
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setGate(char gate, float start, float width,
                              float threshold, const QString &measureType)
{
    sendJsonCommand(buildGateCommand(gate, start, width, threshold, measureType));
}

void CTSPA22SDriver::setBeamDelay()
{
    QJsonObject obj;
    obj["beam_delay_correct"] = 0;
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setCommonRDelay()
{
    QJsonObject obj;
    obj["common_rdelay"] = 0;
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setTFMImageProcess(double subtract, double supress,
                                         double smooth)
{
    QJsonObject process;
    process["subtract"] = subtract;
    process["supress"]  = supress;
    process["smooth"]   = smooth;
    QJsonObject tfm;
    tfm["tfm_img_process"] = process;
    sendJsonCommand(tfm);
}

void CTSPA22SDriver::queryTemperature()
{
    QJsonObject obj;
    obj["get_xadc"] = "temp";

    QJsonObject resp = sendJsonCommand(obj, true);  // 需要温度响应
    // 设备返回 {"result":48.83}（数值直接在 result 上，
    // 见 MFC CTSPA22SView.cpp:4643 注释 R:{"result":12.19...}）
    const QJsonValue result = resp.value("result");
    if (result.isDouble()) {
        const double temp = qBound(-50.0, result.toDouble(), 100.0);
        emit temperatureReceived(temp);
        m_xadcTemp = static_cast<int>(temp);
        if (m_tempCorrect
                && (m_lastCompTemp == -100 || qAbs(m_xadcTemp - m_lastCompTemp) >= 1)) {
            sendJsonCommand(buildDGainCommand(m_digitalGain));
            m_lastCompTemp = m_xadcTemp;
        }
    }
}

void CTSPA22SDriver::queryVoltage()
{
    QJsonObject obj;
    obj["get_xadc"] = "vin";

    QJsonObject resp = sendJsonCommand(obj, true);  // 需要电压响应
    // 同温度查询：设备返回 {"result":11.19}，数值直接在 result 上
    const QJsonValue result = resp.value("result");
    if (result.isDouble())
        emit voltageReceived(result.toDouble());
}

void CTSPA22SDriver::resetEncoder(int idx)
{
    QJsonObject enc;
    enc["idx"] = idx;
    QJsonObject obj;
    obj["encoder_reset"] = enc;
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setACG(bool enabled, const PAParams &params)
{
    QJsonObject obj;
    if (!enabled) {
        obj["bcg"] = 1.0;
    } else {
        QJsonArray values;
        const int count = qBound(1, params.global.beamCount, MaxBeams);
        for (int beam = 0; beam < count; ++beam)
            values.append(double(params.tcg.acgValue[beam]));
        obj["bcg"] = values;
    }
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setTCG(bool enabled, const PAParams &params)
{
    const int count = qBound(1, params.global.beamCount, MaxBeams);
    for (int beam = 0; beam < count; ++beam) {
        QJsonArray data;
        if (enabled) {
            for (int i = 0; i < 50; ++i) {
                const int segment = i / 10;
                const double fraction = (i % 10) / 10.0;
                const double distance = params.tcg.tcgX[segment]
                    + (params.tcg.tcgX[segment + 1] - params.tcg.tcgX[segment]) * fraction;
                const int x = qRound(distance * 2.0 / params.wp.lVelocity
                                     * 1000000.0 / S22_SP);
                const double ratio = params.tcg.tcgRatio[segment]
                    + (params.tcg.tcgRatio[segment + 1] - params.tcg.tcgRatio[segment]) * fraction;
                QJsonArray point; point.append(x); point.append(ratio);
                data.append(point);
            }
        } else {
            for (int i = 0; i < 10; ++i) {
                const int x = qRound(i * 40.0 * params.tx.range / WaveSampleCount * 2.0
                                     / params.wp.lVelocity * 1000000.0 / S22_SP);
                QJsonArray point; point.append(x); point.append(1.0);
                data.append(point);
            }
        }
        QJsonObject tcg;
        tcg["beam_index"] = beam;
        tcg["data"] = data;
        QJsonObject obj;
        obj["tcg"] = tcg;
        sendJsonCommand(obj);
    }
}

// ================================================================
// 扫查配置 setter（仅更新内部状态，不单独下发命令；由 setScanType 统一构造 JSON）
// ================================================================

void CTSPA22SDriver::setVelocity(float mps)
{
    m_velocity = mps;
}

void CTSPA22SDriver::setProbeGeometry(int count, float freqMHz, float pitchMm)
{
    m_probeCount = count;
    m_probeFreq  = freqMHz;
    m_probePitch = pitchMm;
}

void CTSPA22SDriver::setElementGeometry(int start, int end, int aperture)
{
    m_eleStart    = start;
    m_eleEnd      = end;
    m_eleAperture = aperture;
}

void CTSPA22SDriver::setSscanAngles(float fromDeg, float toDeg)
{
    m_angleFrom = fromDeg;
    m_angleTo   = toDeg;
}

void CTSPA22SDriver::setLscanAngle(float angleDeg)
{
    // L扫只使用单一角度；angle 也存于 m_angleFrom 供 buildScanTypeCommand 读取
    m_angleFrom = angleDeg;
}

void CTSPA22SDriver::setFocusMm(float mm)
{
    m_focus = mm;
}

void CTSPA22SDriver::setWedgeGeometry(bool enable, float angleDeg, int velocityMps, float heightMm)
{
    m_wedgeEnable   = enable;
    m_wedgeAngle    = angleDeg;
    m_wedgeVelocity = velocityMps;
    m_wedgeHeight   = heightMm;
}

void CTSPA22SDriver::powerOff()
{
    // 对应 MFC 版: {"power": "off"}\x1E
    // 硬件还支持 "on" 和 "stayby"，这里只实现关机
    QJsonObject obj;
    obj["power"] = QString("off");
    sendJsonCommand(obj, false);  // 不等待响应，硬件可能直接断电
    emit statusChanged(QString::fromUtf8("已发送远程关机指令"));
}
