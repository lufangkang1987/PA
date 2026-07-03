#pragma once
// ============================================================
// AppState — 运行时状态管理器（替代 MFC 版 g_TempPara 全局变量）
// ============================================================
// 单例 + QObject 信号槽。任何状态变更都通过 setter → emit 信号
// → 感兴趣的 Widget update() 实现 UI 自动刷新，不再手动调 OnPaint()。

#include <QObject>
#include <QString>
#include <QVector>
#include <cstdint>
#include "DataTypes.h"

class AppState : public QObject
{
    Q_OBJECT

public:
    static AppState* instance();

    // ============================================================
    //  连接状态 —— 对应原 ConnectState / ReadThreadState 等
    // ============================================================

    bool isConnected() const              { return m_connected; }
    void setConnected(bool v);

    bool isAcquiring() const              { return m_acquiring; }
    void setAcquiring(bool v);

    int  connectionMode() const           { return m_connectionMode; }
    void setConnectionMode(int mode);     // casts from ConnectionMode enum

    // ============================================================
    //  工作模式 —— 对应原 ParaPage / ParaType / ScanType
    // ============================================================

    int  paraPage() const                 { return m_paraPage; }
    void setParaPage(int page);           // 0=参数页, 1=数据页

    int  paraType() const                 { return m_paraType; }
    void setParaType(int type);           // 0=发射, 1=接收, ...

    int  scanType() const                 { return m_scanType; }
    void setScanType(int type);           // 0=S扫, 1=L扫, 2=CL扫, 3=TFM

    // ============================================================
    //  冻结 / 回放 / 采集 — 对应原 FreezeState / ReplayState / StartState
    // ============================================================

    bool freezeState() const              { return m_freezeState; }
    void setFreezeState(bool v);

    bool replayState() const              { return m_replayState; }
    void setReplayState(bool v);

    bool startState() const               { return m_startState; }
    void setStartState(bool v);

    bool replayDrawChangeState() const    { return m_replayDrawChangeState; }
    void setReplayDrawChangeState(bool v);

    // ============================================================
    //  当前声束 / 声束数 — 对应原 CurBeam / BeamCount
    // ============================================================

    int  currentBeam() const              { return m_currentBeam; }
    void setCurrentBeam(int beam);

    int  beamCount() const                { return m_beamCount; }
    void setBeamCount(int count);

    // ============================================================
    //  声学参数 — 声速 / 声程 / 采样率（影响 A 扫横轴标尺）
    // ============================================================
    float velocity() const                { return m_velocity; }       // m/s
    void  setVelocity(float v);

    float range() const                   { return m_range; }          // mm
    void  setRange(float r);

    int   sampleRate() const              { return m_sampleRate; }     // MHz
    void  setSampleRate(int rate);

    int  tempBeamCount() const            { return m_tempBeamCount; }
    void setTempBeamCount(int count);

    // ============================================================
    //  校准 / TCG / ACG
    // ============================================================

    bool calibrateState() const           { return m_calibrateState; }
    void setCalibrateState(bool v);

    int  calibrateStep() const            { return m_calibrateStep; }
    void setCalibrateStep(int step);

    bool calLineEnable() const            { return m_calLineEnable; }
    void setCalLineEnable(bool v);

    int  tcgCurPoint() const              { return m_tcgCurPoint; }
    void setTcgCurPoint(int pt);

    // ============================================================
    //  编码器 — 对应原 CoderCount / CoderDeg / EncCheckState
    // ============================================================

    uint32_t  encoderCount() const        { return m_encoderCount; }
    void      setEncoderCount(uint32_t v);

    float encoderDegPerPulse() const      { return m_encoderDegPerPulse; }
    void  setEncoderDegPerPulse(float v);

    float degPerPoint() const             { return m_degPerPoint; }
    void  setDegPerPoint(float v);

    // ============================================================
    //  闸门 UI 位置 — 对应原 GateADrawStart / GateADrawWidth 等
    // ============================================================

    int  gateDrawStart(int gateIdx) const;   // 0=A, 1=B, 2=C
    void setGateDrawStart(int gateIdx, int v);

    int  gateDrawWidth(int gateIdx) const;
    void setGateDrawWidth(int gateIdx, int v);

    // ============================================================
    //  C 扫回放 — 对应原 CScanShift / LineX1/2 / LineY1/2 / CurLine
    // ============================================================

    int  cScanShift() const               { return m_cScanShift; }
    void setCScanShift(int shift);

    int  lineX1() const                   { return m_lineX1; }
    void setLineX1(int v);
    int  lineX2() const                   { return m_lineX2; }
    void setLineX2(int v);
    int  lineY1() const                   { return m_lineY1; }
    void setLineY1(int v);
    int  lineY2() const                   { return m_lineY2; }
    void setLineY2(int v);

    int  curLine() const                  { return m_curLine; }
    void setCurLine(int v);

    int  replayCurPos() const             { return m_replayCurPos; }
    void setReplayCurPos(int pos);

    // ============================================================
    //  C 扫归档帧数 — 对应原 ReadNum
    // ============================================================

    int  readNum() const                  { return m_readNum; }
    void setReadNum(int n);

    // ============================================================
    //  遥测
    // ============================================================

    float temperature() const             { return m_temperature; }
    void  setTemperature(float v);

    float inputVoltage() const            { return m_inputVoltage; }
    void  setInputVoltage(float v);

    // ============================================================
    //  文件路径 — 对应原 PathStr / ReplayFileName
    // ============================================================

    QString pathStr() const               { return m_pathStr; }
    void    setPathStr(const QString &path);

    QString replayFileName() const        { return m_replayFileName; }
    void    setReplayFileName(const QString &name);

    // ============================================================
    //  状态重置
    // ============================================================

    void reset();   // 回到上电初始状态

signals:
    // -------- 连接 --------
    void connectedChanged(bool connected);
    void acquiringChanged(bool acquiring);
    void connectionModeChanged(int mode);

    // -------- 工作模式 --------
    void paraPageChanged(int page);
    void paraTypeChanged(int type);
    void scanTypeChanged(int type);

    // -------- 冻结 / 回放 / 采集 --------
    void freezeStateChanged(bool frozen);
    void replayStateChanged(bool replay);
    void startStateChanged(bool started);

    // -------- 声束 --------
    void currentBeamChanged(int beam);
    void beamCountChanged(int count);

    // -------- 声学参数 --------
    void velocityChanged(float velocity);
    void rangeChanged(float range);
    void sampleRateChanged(int rate);

    // -------- 校准 --------
    void calibrateStateChanged(bool cal);
    void calibrateStepChanged(int step);

    // -------- 编码器 --------
    void encoderCountChanged(uint32_t count);

    // -------- C 扫回放 --------
    void cScanShiftChanged(int shift);
    void lineX1Changed(int x);
    void lineX2Changed(int x);
    void lineY1Changed(int y);
    void lineY2Changed(int y);
    void replayCurPosChanged(int pos);

    // -------- 归档 --------
    void readNumChanged(int n);
    void replayDrawChangeStateChanged(bool changed);

    // -------- 遥测 --------
    void temperatureChanged(float tempC);
    void inputVoltageChanged(float voltage);

private:
    explicit AppState(QObject *parent = nullptr);
    static AppState *s_instance;

    // ===== 连接 =====
    bool m_connected   = false;
    bool m_acquiring   = false;
    int  m_connectionMode = static_cast<int>(ConnectionMode::Wireless);  // 默认无线

    // ===== 工作模式 =====
    int  m_paraPage    = 0;      // 0=参数页, 1=数据页
    int  m_paraType    = 0;      // 0=发射, ...
    int  m_scanType    = 0;      // 0=S扫

    // ===== 冻结/回放/采集 =====
    bool m_freezeState           = false;
    bool m_replayState           = false;
    bool m_startState            = false;
    bool m_replayDrawChangeState = false;

    // ===== 声束 =====
    int  m_currentBeam   = 0;
    int  m_beamCount     = 128;
    int  m_tempBeamCount = 128;

    // ===== 声学参数（默认值：钢纵波 5920 m/s，100 MHz 采样）=====
    float m_velocity    = 5920.0f;
    float m_range       = 0.0f;     // 0=自动（samples / rate * velocity / 2）
    int   m_sampleRate  = 100;      // MHz

    // ===== 校准 =====
    bool m_calibrateState   = false;
    int  m_calibrateStep    = 0;
    bool m_calLineEnable    = false;
    int  m_tcgCurPoint      = 0;

    // ===== 编码器 =====
    uint32_t m_encoderCount      = 0;
    float    m_encoderDegPerPulse = 0.09f;
    float    m_degPerPoint       = 0.36f;

    // ===== 闸门绘制位置 =====
    int  m_gateDrawStart[3] = {0, 0, 0};   // A/B/C
    int  m_gateDrawWidth[3] = {100, 100, 100};

    // ===== C 扫回放 =====
    int  m_cScanShift    = 0;
    int  m_lineX1        = 0;
    int  m_lineX2        = 924;
    int  m_lineY1        = 0;
    int  m_lineY2        = 400;
    int  m_curLine       = 0;
    int  m_replayCurPos  = 0;

    // ===== 归档 =====
    int  m_readNum       = 0;

    // ===== 遥测 =====
    float m_temperature  = 25.0f;
    float m_inputVoltage = 12.0f;

    // ===== 文件路径 =====
    QString m_pathStr;
    QString m_replayFileName;
};
