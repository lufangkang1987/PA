#pragma once
#include "IDriver.h"
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
class CTSPA22SDriver : public QObject, public IDriver
{
    Q_OBJECT

public:
    explicit CTSPA22SDriver(QObject *parent = nullptr);
    ~CTSPA22SDriver() override;

    // ========== IDriver 接口 ==========
    QObject* asQObject() override { return this; }
    bool connectDevice(const QString &ip,
                       quint16 cmdPort  = 51007,
                       quint16 dataPort = 51005);
    bool connectDevice(const QString &ip, quint16 port) override
    { return connectDevice(ip, 51007, port); }
    bool connectDevice(ConnectionMode mode) override;
    void disconnectDevice() override;
    bool isConnected() const override;
    void startAcquisition() override;
    void stopAcquisition() override;

    // ========== 连接模式便捷方法 ==========

    /// 无线连接：自动使用默认 WIFI IP 192.168.0.51
    bool connectViaWifi()    { return connectDevice(ConnectionMode::Wireless); }

    /// 有线连接：自动使用默认网口 IP 192.168.22.121
    bool connectViaEthernet(){ return connectDevice(ConnectionMode::Wired); }

    ConnectionMode connectionMode() const { return m_connectionMode; }
    void setConnectionMode(ConnectionMode mode) { m_connectionMode = mode; }

    // ---- 默认连接参数（来自使用说明书 V3） ----
    static constexpr const char *DefaultWifiIP       = "192.168.0.51";
    static constexpr const char *DefaultWiredIP      = "192.168.22.121";
    static constexpr const char *DefaultWifiSSID     = "PA22S-";
    static constexpr const char *DefaultWifiPassword = "12345678";
    static constexpr quint16 DefaultCmdPort  = 51007;
    static constexpr quint16 DefaultDataPort = 51005;

    // ========== 参数命令（对应 MFC 版 SendCommand） ==========

    /// 设置扫查类型：0=S扫, 1=L扫, 2=CL扫, 3=TFM
    void setScanType(int type);

    /// 设置模拟增益(dB)
    void setAnalogGain(float dB);

    /// 设置数字增益(dB)
    void setDigitalGain(float dB);

    /// 设置高压：0=110V, 1=40V, 2=20V
    void setHighVoltage(int level);

    /// 设置脉冲宽度(ns)
    void setPulseWidth(int width);

    /// 设置重复频率(Hz)
    void setPRF(int prf);

    /// 设置检测范围(mm)
    void setRange(float range);

    /// 设置检波方式：0=全波, 1=正半波, 2=负半波, 3=射频
    void setRectify(int mode);

    /// 设置滤波器档位
    void setFilter(int filter);

    /// 设置 A 波数据长度：0=128, 1=256, 2=512
    void setADataLen(int len);

    /// 设置 A 扫平滑
    void setASmooth(bool enable);

    /// 设置视频检波
    void setVideoDetect(bool enable);

    /// 设置闸门参数 (gate: 'A'/'B'/'C')
    void setGate(char gate, float start, float width, float threshold,
                 const QString &measureType = "peak");

    /// 设置声束延迟校正
    void setBeamDelay();

    /// 设置接收延迟(TFM模式)
    void setCommonRDelay();

    /// 设置 TFM 图像处理参数
    void setTFMImageProcess(double subtract, double supress, double smooth);

    /// 获取仪器温度
    void queryTemperature();

    /// 获取仪器电压
    void queryVoltage();

    /// 编码器复位（idx: 0=正向, 1=反向）
    void resetEncoder(int idx);

signals:
    // -------- 连接状态 --------
    void connectionChanged(bool connected);
    void statusChanged(const QString &status);
    void errorOccurred(const QString &error);

    // -------- A 扫描数据 --------
    /// 单声束波形（用于 A 扫描显示）
    /// @param waveform   已按检波模式归一化的采样点
    /// @param beamIndex  当前显示的声束编号 (0~127)
    /// @param frameIndex 递增帧序号（用于判定数据连续性）
    /// @param rectifyMode 检波模式: 0=全波,1=正半波,2=负半波,3=射频
    void waveformReady(const QVector<double> &waveform,
                       int beamIndex, int frameIndex, int rectifyMode);

    /// 全部 128 声束波形（用于 B 扫描 softwareImaging）
    void multiBeamWaveformsReady(const QVector<QVector<double>> &waveforms);

    // -------- 闸门读数 --------
    void gateReadingsReady(char gate, double amplitude, double path);

    // -------- 编码器位置 --------
    void encoderPositionChanged(int position);

    // -------- TFM 数据 --------
    void tfmImageReady(const QVector<int> &image);

    // -------- 遥测 --------
    void temperatureReceived(double tempC);
    void voltageReceived(double voltage);

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
    int          m_frameCount  = 0;

    // 参数缓存（用于构造全量 scan_type 命令）
    float m_analogGain   = 40.0f;
    float m_digitalGain  = 0.0f;
    int   m_highVoltage  = 1;     // 110V
    int   m_pulseWidth   = 100;
    int   m_prf          = 2000;
    float m_range        = 100.0f;
    int   m_rectify      = 3;     // 全波
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
