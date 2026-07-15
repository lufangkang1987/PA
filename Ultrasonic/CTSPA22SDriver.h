#pragma once
#include "IDriver.h"
#include "PAParams.h"
#include "DataTypes.h"
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QByteArray>

// ============================================================
// Driver 内部帧类型 (QByteArray 持有数据，避免悬挂指针)
// ============================================================
struct DriverFrame
{
    QString    type;       // "Ts22w"(PA) 或 "Ts22t"(TFM)
    QByteArray payload;    // 不含 16 字节头的纯载荷
};

// ============================================================
// MTLD 二进制协议解析器
// ============================================================
class MTLDParser
{
public:
    MTLDParser();
    void reset();

    /// 喂入原始字节流，返回已解析完成的 DriverFrame 列表
    QVector<DriverFrame> feed(const QByteArray &data);

    /// 从一帧 PA 载荷中解出 DataPacket（128 声束 + 编码器）
    static DataPacket parseWaveforms(const QByteArray &payload);

    /// 从一帧 TFM 载荷中解出 256×256 int32 图像
    static QVector<int> parseTFMImage(const QByteArray &payload);

private:
    enum class State { LookingForSync, ReadingHeader, ReadingPayload };

    State       m_state       = State::LookingForSync;
    QByteArray  m_buffer;
    QString     m_packetType;
    int         m_payloadLen  = 0;

    static constexpr uint8_t SyncPattern[4] = { 0xAA, 0x55, 0xAA, 0x55 };
    static constexpr int HeaderSize = 16;
};

// ============================================================
// 汕头超声 CTSPA22S 超声仪器 Driver
// ============================================================
///
/// 通过 TCP 双通道与 CTSPA22S 仪器通信：
///   - 命令通道：JSON 文本（默认端口 51007）
///   - 数据通道：MTLD 二进制帧（默认端口 51005）
///
/// 使用方式（与 USM100Driver 完全一致）：
/// @code
///   auto *driver = new CTSPA22SDriver(this);
///   driver->connectDevice("192.168.0.51");
///   driver->startAcquisition();
///   connect(driver, &CTSPA22SDriver::waveformReady,
///           aScanWidget, &AScanWidget::setWaveform);
/// @endcode
///
class CTSPA22SDriver : public IDriver
{
    Q_OBJECT

public:
    explicit CTSPA22SDriver(QObject *parent = nullptr);
    ~CTSPA22SDriver() override;

    // ========== IDriver 接口 ==========
    bool connectDevice(const QString &ip,
                       quint16 cmdPort  = DefaultCmdPort,
                       quint16 dataPort = DefaultDataPort);
    bool connectDevice(const QString &ip, quint16 port) override
    { return connectDevice(ip, DefaultCmdPort, port); }
    bool connectDevice(ConnectionMode mode) override;
    void disconnectDevice() override;
    bool isConnected() const override;
    void startAcquisition() override;
    void stopAcquisition() override;

    // ========== 连接模式便捷方法 ==========

    // ---- 默认连接参数（来自使用说明书 V3） ----
    static constexpr const char *DefaultWifiIP       = "192.168.0.51";
    static constexpr const char *DefaultWiredIP      = "192.168.22.121";
    static constexpr quint16 DefaultCmdPort  = 51007;
    static constexpr quint16 DefaultDataPort = 51005;

    // ========== 参数命令（对应 MFC 版 SendCommand） ==========

    void setScanType(int type) override;
    void setAnalogGain(float dB) override;
    void setDigitalGain(float dB) override;
    void setTemperatureCompensation(bool enabled) override;
    void setHighVoltage(int level) override;
    void setPulseWidth(int width) override;
    void setPRF(int prf) override;
    void setRange(float range) override;
    void setRectify(int mode) override;
    void setFilter(int filter) override;
    void setADataLen(int len) override;
    void setASmooth(bool enable) override;
    void setVideoDetect(bool enable) override;
    void setGate(char gate, float start, float width, float threshold,
                 const QString &measureType = QStringLiteral("peak")) override;
    void setBeamDelay() override;
    void setCommonRDelay() override;
    void setTFMImageProcess(double subtract, double supress, double smooth) override;
    void resetEncoder(int idx) override;
    void setACG(bool enabled, const PAParams &params) override;
    void setTCG(bool enabled, const PAParams &params) override;

    // -------- 扫查配置 setter（供"应用法则"批量下发）--------
    void setVelocity(float mps) override;
    void setProbeGeometry(int count, float freqMHz, float pitchMm) override;
    void setElementGeometry(int start, int end, int aperture) override;
    void setSscanAngles(float fromDeg, float toDeg) override;
    void setLscanAngle(float angleDeg) override;
    void setFocusMm(float mm) override;
    void setWedgeGeometry(bool enable, float angleDeg, int velocityMps, float heightMm) override;

    /// 远程关机
    void powerOff() override;

    /// 获取仪器温度
    void queryTemperature() override;

    /// 获取仪器电压
    void queryVoltage() override;

private slots:
    void onCmdConnected();
    void onCmdDisconnected();
    void onCmdError(QAbstractSocket::SocketError err);
    void onDataConnected();
    void onDataDisconnected();
    void onDataError(QAbstractSocket::SocketError err);
    void onDataReadyRead();
    void processFrame(const DriverFrame &frame);

private:
    // ===== 发送工具 =====
    QJsonObject sendJsonCommand(const QJsonObject &obj, bool waitForResp = true);
    QJsonObject waitForResponse(int timeoutMs = 3000);

    // ===== 命令构造 =====
    QJsonObject buildScanTypeCommand(int type);
    QJsonObject buildGainCommand(float analogGain);
    QJsonObject buildDGainCommand(float digitalGain);
    QJsonObject buildVoltageCommand(int level);
    QJsonObject buildGateCommand(char gate, float start, float width,
                                 float threshold, const QString &measureType);
    QJsonObject buildRangeCommand(float range);

    // ===== 内部状态 =====
    QTcpSocket  *m_cmdSocket  = nullptr;
    QTcpSocket  *m_dataSocket = nullptr;
    MTLDParser   m_parser;
    ConnectionMode m_connectionMode = ConnectionMode::Wireless;
    bool         m_connected   = false;
    bool         m_acquiring   = false;
    bool         m_cmdReady    = false;
    bool         m_dataReady   = false;
    int          m_scanType    = 0;
    int          m_beamCount   = 128;
    bool         m_hasLastWaveFrame = false;
    quint16      m_lastWaveFrame = 0;
    int          m_lastReportedFrameDiff = -1;
    quint64      m_droppedFrames = 0;

    // 参数缓存（用于构造全量 scan_type 命令）
    float m_analogGain   = 40.0f;
    float m_digitalGain  = 0.0f;
    bool  m_tempCorrect  = true;
    int   m_xadcTemp     = 30;
    int   m_lastCompTemp = -100;
    QTimer *m_monitorTimer = nullptr;
    int   m_highVoltage  = 1;     // 110V
    int   m_pulseWidth   = 100;
    int   m_prf          = 2000;
    float m_range        = 100.0f;
    int   m_rectify      = 0;     // 全波
    int   m_filter       = 0;
    int   m_aDataLen     = 0;     // 400点

    // 探头参数
    int   m_probeCount   = 64;
    float m_probeFreq    = 5.0f;
    float m_probePitch   = 0.6f;
    int   m_eleStart     = 1;
    int   m_eleEnd       = 64;
    int   m_eleAperture  = 16;
    float m_angleFrom    = 30.0f;
    float m_angleTo      = 70.0f;
    float m_focus        = 50.0f;
    float m_velocity     = 5900.0f;

    // 楔块参数
    bool  m_wedgeEnable  = false;
    float m_wedgeAngle   = 0.0f;
    int   m_wedgeVelocity = 2337;
    float m_wedgeHeight  = 0.0f;
};
