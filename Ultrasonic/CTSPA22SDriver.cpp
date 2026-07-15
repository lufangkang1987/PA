#include "CTSPA22SDriver.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostAddress>
#include <QThread>
#include <QtMath>
#include <cstdint>
#include <cstring>

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

DataPacket MTLDParser::parseWaveforms(const QByteArray &payload)
{
    DataPacket pkt;
    const int beamSize = sizeof(Waveform);  // 1024 字节

    int count = payload.size() / beamSize;
    if (count > MaxBeams)
        count = MaxBeams;

    pkt.beamCount = count;

    for (int b = 0; b < count; ++b) {
        const uint8_t *src = reinterpret_cast<const uint8_t *>(
            payload.constData() + b * beamSize);

        BeamWaveform &wf = pkt.beams[b];

        // waveP[400] 位于字节 0-399
        std::memcpy(wf.waveP, src, 400);

        // frame（字节 400-401, uint16 LE）
        wf.frame = src[400] | (src[401] << 8);

        // channel（字节 402）
        wf.channel = src[402];

        // path0（字节 403-404）
        wf.path0 = src[403] | (src[404] << 8);
        wf.amp0  = src[405];

        // path1（字节 406-407）
        wf.path1 = src[406] | (src[407] << 8);
        wf.amp1  = src[408];

        // path2（字节 409-410）
        wf.path2 = src[409] | (src[410] << 8);
        wf.amp2  = src[411];

        // enc[2]（字节 412-427，每 8 字节一个 ENCODER）
        wf.encFwd = src[412] | (src[413] << 8) |
                    (src[414] << 16) | (src[415] << 24);
        wf.encRvs = src[420] | (src[421] << 8) |
                    (src[422] << 16) | (src[423] << 24);

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
    if (m_connected) {
        emit statusChanged(QString::fromUtf8("已经连接"));
        return true;
    }

    emit statusChanged(QString::fromUtf8("正在连接 %1 ...").arg(ip));

    m_cmdSocket->connectToHost(QHostAddress(ip), cmdPort);
    if (!m_cmdSocket->waitForConnected(5000)) {
        emit errorOccurred(QString::fromUtf8("命令通道连接失败: ") + m_cmdSocket->errorString());
        return false;
    }

    m_dataSocket->connectToHost(QHostAddress(ip), dataPort);
    if (!m_dataSocket->waitForConnected(5000)) {
        m_cmdSocket->disconnectFromHost();
        emit errorOccurred(QString::fromUtf8("数据通道连接失败: ") + m_dataSocket->errorString());
        return false;
    }

    m_cmdReady  = true;
    m_dataReady = true;
    m_connected = true;
    m_parser.reset();
    m_lastCompTemp = -100;
    m_monitorTimer->start();
    QTimer::singleShot(0, this, [this] {
        if (!m_connected) return;
        queryTemperature();
        queryVoltage();
    });

    emit connectionChanged(true);
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
    m_monitorTimer->stop();
    stopAcquisition();

    if (m_dataSocket->state() != QAbstractSocket::UnconnectedState)
        m_dataSocket->disconnectFromHost();
    if (m_cmdSocket->state() != QAbstractSocket::UnconnectedState)
        m_cmdSocket->disconnectFromHost();

    m_cmdReady  = false;
    m_dataReady = false;
    m_connected = false;

    emit connectionChanged(false);
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
    sendJsonCommand(cmd);

    m_acquiring = true;
    emit statusChanged(QString::fromUtf8("开始采集"));
}

void CTSPA22SDriver::stopAcquisition()
{
    if (!m_acquiring) return;

    QJsonObject cmd;
    cmd["scan"] = 0;
    sendJsonCommand(cmd);

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
        DataPacket pkt = MTLDParser::parseWaveforms(frame.payload);

        if (pkt.beamCount < 1) return;

        const quint16 currentFrame = pkt.beams[0].frame;
        int frameDiff = 0;
        if (m_hasLastWaveFrame) {
            frameDiff = static_cast<quint16>(currentFrame - m_lastWaveFrame);
            if (frameDiff > 32768)
                frameDiff = 0;
            if (frameDiff > 1)
                m_droppedFrames += static_cast<quint64>(frameDiff - 1);
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
            int curBeam = qBound(0, m_beamCount / 2, pkt.beamCount - 1);
            const auto &wf = pkt.beams[curBeam];
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
            QVector<QVector<double>> allWaves(pkt.beamCount);
            const bool isRF = (m_rectify == 3);
            for (int b = 0; b < pkt.beamCount; ++b) {
                allWaves[b].resize(WaveSampleCount);
                for (int i = 0; i < WaveSampleCount; ++i) {
                    if (isRF)
                        allWaves[b][i] = (static_cast<int>(pkt.beams[b].waveP[i]) - 128) / 128.0;
                    else
                        allWaves[b][i] = pkt.beams[b].waveP[i] / 255.0;
                }
            }
            emit multiBeamWaveformsReady(allWaves);
        }

        // --- 闸门读数 ---
        {
            int curBeam = qBound(0, m_beamCount / 2, pkt.beamCount - 1);
            const auto &wf = pkt.beams[curBeam];
            emit gateReadingsReady('A', wf.amp0, wf.path0);
            emit gateReadingsReady('B', wf.amp1, wf.path1);
            emit gateReadingsReady('C', wf.amp2, wf.path2);
        }

        // --- 编码器 ---
        {
            int curBeam = qBound(0, m_beamCount / 2, pkt.beamCount - 1);
            const auto &wf = pkt.beams[curBeam];
            // 正向为主，同时考虑反向
            int pos = static_cast<int>(wf.encFwd) - static_cast<int>(wf.encRvs);
            emit encoderPositionChanged(pos);
        }

    } else if (frame.type == "Ts22t") {
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
    m_cmdReady = true;
    if (m_cmdReady && m_dataReady && !m_connected) {
        m_connected = true;
        emit connectionChanged(true);
        emit statusChanged(QString::fromUtf8("双通道已连接"));
    }
}

void CTSPA22SDriver::onCmdDisconnected()
{
    m_cmdReady = false;
    if (m_connected) {
        m_connected = false;
        emit connectionChanged(false);
        emit statusChanged(QString::fromUtf8("命令通道断开"));
    }
}

void CTSPA22SDriver::onCmdError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err);
    emit errorOccurred(QString::fromUtf8("命令通道错误: ") + m_cmdSocket->errorString());
}

void CTSPA22SDriver::onDataConnected()
{
    m_dataReady = true;
    if (m_cmdReady && m_dataReady && !m_connected) {
        m_connected = true;
        emit connectionChanged(true);
        emit statusChanged(QString::fromUtf8("双通道已连接"));
    }
}

void CTSPA22SDriver::onDataDisconnected()
{
    m_dataReady = false;
    if (m_connected) {
        m_connected = false;
        emit connectionChanged(false);
        emit statusChanged(QString::fromUtf8("数据通道断开"));
    }
}

void CTSPA22SDriver::onDataError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err);
    emit errorOccurred(QString::fromUtf8("数据通道错误: ") + m_dataSocket->errorString());
}

// ================================================================
// 命令发送工具
// ================================================================

QJsonObject CTSPA22SDriver::sendJsonCommand(const QJsonObject &obj, bool waitForResp)
{
    if (!m_cmdReady) return {};

    // 发前清空命令通道残留（防止读到上一次的响应）
    if (m_cmdSocket->bytesAvailable() > 0)
        m_cmdSocket->readAll();

    QJsonDocument doc(obj);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    json.append('\x1E');  // MTLD 命令结束符 (RS, 同 MFC 版 \36 八进制)

    m_cmdSocket->write(json);
    m_cmdSocket->flush();

    if (waitForResp)
        return waitForResponse(3000);
    return {};
}

QJsonObject CTSPA22SDriver::waitForResponse(int timeoutMs)
{
    if (!m_cmdSocket->waitForReadyRead(timeoutMs))
        return {};

    QByteArray respData = m_cmdSocket->readAll();
    // 去除结束符 RS(0x1E)
    if (respData.endsWith('\x1E'))
        respData.chop(1);

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(respData, &err);
    if (err.error != QJsonParseError::NoError)
        return {};

    return doc.object();
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
        QThread::msleep(200);
    }
    const QJsonObject response = sendJsonCommand(buildScanTypeCommand(type));
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

    QJsonObject resp = sendJsonCommand(obj);  // 阻塞等响应
    if (resp.contains("result") && resp["result"].toObject().contains("temp")) {
        const double temp = qBound(-50.0,
            resp["result"].toObject()["temp"].toDouble(), 100.0);
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

    QJsonObject resp = sendJsonCommand(obj);  // 阻塞等响应
    if (resp.contains("result") && resp["result"].toObject().contains("vin")) {
        double vin = resp["result"].toObject()["vin"].toDouble();
        emit voltageReceived(vin);
    }
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
        const int count = qBound(1, params.beamCount, MaxBeams);
        for (int beam = 0; beam < count; ++beam)
            values.append(double(params.acgValue[beam]));
        obj["bcg"] = values;
    }
    sendJsonCommand(obj);
}

void CTSPA22SDriver::setTCG(bool enabled, const PAParams &params)
{
    const int count = qBound(1, params.beamCount, MaxBeams);
    for (int beam = 0; beam < count; ++beam) {
        QJsonArray data;
        if (enabled) {
            for (int i = 0; i < 50; ++i) {
                const int segment = i / 10;
                const double fraction = (i % 10) / 10.0;
                const double distance = params.tcgX[segment]
                    + (params.tcgX[segment + 1] - params.tcgX[segment]) * fraction;
                const int x = qRound(distance * 2.0 / params.lVelocity
                                     * 1000000.0 / S22_SP);
                const double ratio = params.tcgRatio[segment]
                    + (params.tcgRatio[segment + 1] - params.tcgRatio[segment]) * fraction;
                QJsonArray point; point.append(x); point.append(ratio);
                data.append(point);
            }
        } else {
            for (int i = 0; i < 10; ++i) {
                const int x = qRound(i * 40.0 * params.range / WaveSampleCount * 2.0
                                     / params.lVelocity * 1000000.0 / S22_SP);
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
