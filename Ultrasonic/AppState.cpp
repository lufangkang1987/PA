#include "AppState.h"

AppState *AppState::s_instance = nullptr;

AppState::AppState(QObject *parent)
    : QObject(parent)
{
}

AppState* AppState::instance()
{
    if (!s_instance)
        s_instance = new AppState();
    return s_instance;
}

// ============================================================
//  连接状态
// ============================================================

void AppState::setConnected(bool v)
{
    if (m_connected != v) {
        m_connected = v;
        emit connectedChanged(v);
    }
}

void AppState::setAcquiring(bool v)
{
    if (m_acquiring != v) {
        m_acquiring = v;
        emit acquiringChanged(v);
    }
}

void AppState::setConnectionMode(int mode)
{
    if (m_connectionMode != mode) {
        m_connectionMode = mode;
        emit connectionModeChanged(mode);
    }
}

// ============================================================
//  工作模式
// ============================================================

void AppState::setParaPage(int page)
{
    if (m_paraPage != page) {
        m_paraPage = page;
        emit paraPageChanged(page);
    }
}

void AppState::setParaType(int type)
{
    if (m_paraType != type) {
        m_paraType = type;
        emit paraTypeChanged(type);
    }
}

void AppState::setScanType(int type)
{
    if (m_scanType != type) {
        m_scanType = type;
        emit scanTypeChanged(type);
    }
}

// ============================================================
//  冻结 / 回放 / 采集
// ============================================================

void AppState::setFreezeState(bool v)
{
    if (m_freezeState != v) {
        m_freezeState = v;
        emit freezeStateChanged(v);
    }
}

void AppState::setReplayState(bool v)
{
    if (m_replayState != v) {
        m_replayState = v;
        emit replayStateChanged(v);
    }
}

void AppState::setStartState(bool v)
{
    if (m_startState != v) {
        m_startState = v;
        emit startStateChanged(v);
    }
}

void AppState::setReplayDrawChangeState(bool v)
{
    if (m_replayDrawChangeState != v) {
        m_replayDrawChangeState = v;
        emit replayDrawChangeStateChanged(v);
    }
}

// ============================================================
//  声束
// ============================================================

void AppState::setCurrentBeam(int beam)
{
    if (m_currentBeam != beam) {
        m_currentBeam = beam;
        emit currentBeamChanged(beam);
    }
}

void AppState::setBeamCount(int count)
{
    if (m_beamCount != count) {
        m_beamCount = count;
        emit beamCountChanged(count);
    }
}

void AppState::setTempBeamCount(int count)
{
    m_tempBeamCount = count;
    // 不发信号，这是内部计算用的临时值
}

// ============================================================
//  声学参数
// ============================================================

void AppState::setVelocity(float v)
{
    if (m_velocity != v) {
        m_velocity = v;
        emit velocityChanged(v);
    }
}

void AppState::setRange(float r)
{
    if (m_range != r) {
        m_range = r;
        emit rangeChanged(r);
    }
}

void AppState::setSampleRate(int rate)
{
    if (m_sampleRate != rate) {
        m_sampleRate = rate;
        emit sampleRateChanged(rate);
    }
}

// ============================================================
//  校准
// ============================================================

void AppState::setCalibrateState(bool v)
{
    if (m_calibrateState != v) {
        m_calibrateState = v;
        emit calibrateStateChanged(v);
    }
}

void AppState::setCalibrateStep(int step)
{
    if (m_calibrateStep != step) {
        m_calibrateStep = step;
        emit calibrateStepChanged(step);
    }
}

void AppState::setCalLineEnable(bool v)
{
    m_calLineEnable = v;
}

void AppState::setTcgCurPoint(int pt)
{
    m_tcgCurPoint = pt;
}

// ============================================================
//  编码器
// ============================================================

void AppState::setEncoderCount(uint32_t v)
{
    if (m_encoderCount != v) {
        m_encoderCount = v;
        emit encoderCountChanged(v);
    }
}

void AppState::setEncoderDegPerPulse(float v)
{
    m_encoderDegPerPulse = v;
}

void AppState::setDegPerPoint(float v)
{
    m_degPerPoint = v;
}

// ============================================================
//  闸门位置
// ============================================================

int AppState::gateDrawStart(int gateIdx) const
{
    return (gateIdx >= 0 && gateIdx < 3) ? m_gateDrawStart[gateIdx] : 0;
}

void AppState::setGateDrawStart(int gateIdx, int v)
{
    if (gateIdx >= 0 && gateIdx < 3)
        m_gateDrawStart[gateIdx] = v;
}

int AppState::gateDrawWidth(int gateIdx) const
{
    return (gateIdx >= 0 && gateIdx < 3) ? m_gateDrawWidth[gateIdx] : 100;
}

void AppState::setGateDrawWidth(int gateIdx, int v)
{
    if (gateIdx >= 0 && gateIdx < 3)
        m_gateDrawWidth[gateIdx] = v;
}

// ============================================================
//  C 扫回放
// ============================================================

void AppState::setCScanShift(int shift)
{
    if (m_cScanShift != shift) {
        m_cScanShift = shift;
        emit cScanShiftChanged(shift);
    }
}

void AppState::setLineX1(int v)
{
    if (m_lineX1 != v) { m_lineX1 = v; emit lineX1Changed(v); }
}

void AppState::setLineX2(int v)
{
    if (m_lineX2 != v) { m_lineX2 = v; emit lineX2Changed(v); }
}

void AppState::setLineY1(int v)
{
    if (m_lineY1 != v) { m_lineY1 = v; emit lineY1Changed(v); }
}

void AppState::setLineY2(int v)
{
    if (m_lineY2 != v) { m_lineY2 = v; emit lineY2Changed(v); }
}

void AppState::setCurLine(int v)
{
    m_curLine = v;
}

void AppState::setReplayCurPos(int pos)
{
    if (m_replayCurPos != pos) {
        m_replayCurPos = pos;
        emit replayCurPosChanged(pos);
    }
}

// ============================================================
//  归档
// ============================================================

void AppState::setReadNum(int n)
{
    if (m_readNum != n) {
        m_readNum = n;
        emit readNumChanged(n);
    }
}

// ============================================================
//  遥测
// ============================================================

void AppState::setTemperature(float v)
{
    if (m_temperature != v) {
        m_temperature = v;
        emit temperatureChanged(v);
    }
}

void AppState::setInputVoltage(float v)
{
    if (m_inputVoltage != v) {
        m_inputVoltage = v;
        emit inputVoltageChanged(v);
    }
}

// ============================================================
//  路径
// ============================================================

void AppState::setPathStr(const QString &path)
{
    m_pathStr = path;
}

void AppState::setReplayFileName(const QString &name)
{
    m_replayFileName = name;
}

// ============================================================
//  重置
// ============================================================

void AppState::reset()
{
    setConnected(false);
    setAcquiring(false);
    setConnectionMode(static_cast<int>(ConnectionMode::Wireless));
    setParaPage(0);
    setParaType(0);
    setScanType(0);
    setFreezeState(false);
    setReplayState(false);
    setStartState(false);
    setReplayDrawChangeState(false);
    setCurrentBeam(0);
    setBeamCount(128);
    setTempBeamCount(128);
    setCalibrateState(false);
    setCalibrateStep(0);
    setCalLineEnable(false);
    setTcgCurPoint(0);
    setEncoderCount(0);
    setEncoderDegPerPulse(0.09f);
    setDegPerPoint(0.36f);
    setGateDrawStart(0, 0); setGateDrawStart(1, 0); setGateDrawStart(2, 0);
    setGateDrawWidth(0, 100); setGateDrawWidth(1, 100); setGateDrawWidth(2, 100);
    setCScanShift(0);
    setLineX1(0); setLineX2(924);
    setLineY1(0); setLineY2(400);
    setCurLine(0);
    setReplayCurPos(0);
    setReadNum(0);
    setTemperature(25.0f);
    setInputVoltage(12.0f);
    setPathStr(QString());
    setReplayFileName(QString());
    setVelocity(5920.0f);
    setRange(0.0f);
    setSampleRate(100);
}
