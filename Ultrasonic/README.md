# CTSPA22S Driver 架构说明

## 1. 设计模式

```
┌──────────────────────────────────────────────────┐
│                   IDriver （接口）                 │
│  connect / disconnect / start / stop              │
├────────────────────┬─────────────────────────────┤
│   USM100Driver     │   CTSPA22SDriver             │
│   (模拟数据)        │   (真实TCP + MTLD协议)       │
│                    │                              │
│   QTimer 50ms      │   QTcpSocket x2             │
│   → 生成高斯回波    │   → MTLDParser 拆包          │
│                    │   → parseWaveforms 解波束     │
│                    │   → signals 发送到UI          │
└────────────────────┴─────────────────────────────┘
```

**核心原则**：两个 Driver 对外暴露相同的核心信号（`waveformReady`），UI 层不关心底层是模拟还是真实硬件。

## 2. 文件说明

| 文件 | 作用 |
|------|------|
| `IDriver.h` | 纯虚接口（connect/disconnect/start/stop） |
| `CTSPA22SDriver.h` | CTSPA22S 仪器 Driver 声明 + 数据结构 + MTLDParser |
| `CTSPA22SDriver.cpp` | 完整实现：TCP 双通道、MTLD 协议解析、JSON 命令构造 |
| `USM100Driver.h/.cpp` | GE USM100 模拟 Driver（不改动） |

## 3. CTSPA22SDriver 信号

| 信号 | 触发时机 | 数据类型 | 对应原MFC |
|------|---------|---------|-----------|
| `waveformReady` | 每帧 Ts22w 到达 | `QVector<double>` (400点) | A 扫描波形显示 |
| `multiBeamWaveformsReady` | 每帧 Ts22w 到达 | `QVector<QVector<double>>` (128×400) | softwareImaging 输入 |
| `gateReadingsReady` | 每帧 Ts22w 到达 | gate, amplitude, path | 闸门读数 |
| `encoderPositionChanged` | 每帧 Ts22w 到达 | int | 编码器位置 |
| `tfmImageReady` | 每帧 Ts22t 到达 | `QVector<int>` (256×256) | TFM 图像 |
| `connectionChanged` | 连接/断开 | bool | 连接状态 |
| `statusChanged` | 状态变化 | QString | 日志消息 |
| `temperatureReceived` | queryTemperature 响应 | double | 仪器温度 |
| `voltageReceived` | queryVoltage 响应 | double | 仪器电压 |
| `errorOccurred` | 网络错误 | QString | 错误信息 |

## 4. CTSPA22SDriver 参数命令

对应原 MFC 版 `SendCommand("set X")` 的全部 17 条命令：

```cpp
driver->setScanType(0);          // S扫 (0=S, 1=L, 2=CL, 3=TFM)
driver->setAnalogGain(42.5);     // 模拟增益 dB
driver->setDigitalGain(6.0);     // 数字增益 dB
driver->setHighVoltage(0);       // 高压 110V (0=110V,1=40V,2=20V)
driver->setPulseWidth(100);      // 脉冲宽度 ns
driver->setPRF(2000);            // 重复频率 Hz
driver->setRange(100.0);         // 检测范围 mm
driver->setRectify(3);           // 检波 (0=全波,1=正半波,2=负半波,3=RF)
driver->setFilter(0);            // 滤波器
driver->setADataLen(0);          // A波长度 (0=128,1=256,2=512)
driver->setASmooth(true);        // A扫平滑
driver->setVideoDetect(false);   // 视频检波
driver->setGate('A', 10, 30, 50); // 闸门(起始mm,宽度mm,阈值%)
driver->setBeamDelay();          // 声束延迟校正
driver->setCommonRDelay();       // TFM接收延迟
driver->setTFMImageProcess(0.5,0.3,0.2); // TFM后处理
driver->queryTemperature();      // 查询温度
driver->queryVoltage();          // 查询电压
driver->resetEncoder(0);         // 编码器复位
```

## 5. 在 MainWindow 中切换仪器

```cpp
// -------- 方案 A：用 CTSPA22S （真实仪器）--------
#include "CTSPA22SDriver.h"

m_ultrasonic = new CTSPA22SDriver(this);
m_ultrasonic->connectDevice("192.168.0.51");
m_ultrasonic->startAcquisition();

// -------- 方案 B：用 USM100Driver （模拟调试）--------
#include "USM100Driver.h"

m_ultrasonic = new USM100Driver(this);
m_ultrasonic->connectDevice("192.168.0.100", 5000);
m_ultrasonic->startAcquisition();

// -------- 统一绑定 UI（两种 Driver 都支持）--------
connect(m_ultrasonic, &CTSPA22SDriver::waveformReady,
        m_aScanWidget, &AScanWidget::setWaveform);
connect(m_ultrasonic, &CTSPA22SDriver::statusChanged,
        this, [this](const QString &s) { statusBar()->showMessage(s); });
```

## 6. MTLDParser 单元测试思路

`MTLDParser` 是纯 C++ 类（无 Qt 依赖），可以直接用离线 .raw 数据包做回归测试：

```cpp
// 加载仪器录制的 .raw 文件
QFile file("test_data_20250101.raw");
file.open(QIODevice::ReadOnly);
QByteArray rawData = file.readAll();

MTLDParser parser;
QVector<DriverFrame> frames = parser.feed(rawData);

QCOMPARE(frames.size(), expectedFrameCount);
QCOMPARE(frames[0].type, "Ts22w");

DataPacket pkt = MTLDParser::parseWaveforms(frames[0].payload);
QCOMPARE(pkt.beamCount, 128);
QVERIFY(pkt.beams[0].waveP[0] >= 0);  // 有效数据
```

## 7. 与原 MFC 版 SendCommand 的对照

| MFC (CString) | Qt (QJsonObject) |
|---------------|------------------|
| `SendCommand("set data_start")` | `sendJsonCommand({"scan":1})` |
| `SendCommand("set data_stop")` | `sendJsonCommand({"scan":0})` |
| `SendCommand("set scan_type sscan")` | `buildScanTypeCommand(0)` → 完整 JSON |
| `SendCommand("set again")` | `buildGainCommand(dB)` → 两级 vga1/vga2 |
| `SendCommand("set gate_a")` | `buildGateCommand('A',...)` → name + ctrl + param |
| `SendCommand("get xadc_temp")` | `queryTemperature()` → signals temperatureReceived |
| `send + recv + JSON检查` | `QTcpSocket::write + waitForReadyRead` |

## 8. 与原 MFC 版 ReadDataThread 的对照

| MFC | Qt |
|-----|-----|
| `recv(dataSocket, buf, 550000)` | `QTcpSocket::readyRead → readAll()` |
| `FindHeader(0xAA55AA55)` 字符串搜索 | `MTLDParser::feed()` 状态机 |
| 手动 `memmove` 粘包处理 | `QByteArray::remove(0, n)` 自动管理 |
| `SetEvent / WaitForSingleObject` | `QObject::signal / slot` 跨线程安全 |
| 全局 `g_TempPara.AWave` | 局部变量 → emit signal 传递 |
