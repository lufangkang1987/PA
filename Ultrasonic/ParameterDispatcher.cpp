#include "ParameterDispatcher.h"
#include "IDriver.h"

ParameterDispatcher::ParameterDispatcher(QObject *parent)
    : QObject(parent)
{
    m_gateTimer.setSingleShot(true);
    m_gateTimer.setInterval(50);
    connect(&m_gateTimer, &QTimer::timeout, this, &ParameterDispatcher::doSetGate);
}

void ParameterDispatcher::setDriver(IDriver *driver)
{
    m_driver = driver;
}

bool ParameterDispatcher::isConnected() const
{
    return m_driver && m_driver->isConnected();
}

// ── 简单参数 — 立即透传 ──

void ParameterDispatcher::setAnalogGain(float dB)
{
    if (m_driver) m_driver->setAnalogGain(dB);
}

void ParameterDispatcher::setDigitalGain(float dB)
{
    if (m_driver) m_driver->setDigitalGain(dB);
}

void ParameterDispatcher::setTemperatureCompensation(bool on)
{
    if (m_driver) m_driver->setTemperatureCompensation(on);
}

void ParameterDispatcher::setHighVoltage(int level)
{
    if (m_driver) m_driver->setHighVoltage(level);
}

void ParameterDispatcher::setPulseWidth(int width)
{
    if (m_driver) m_driver->setPulseWidth(width);
}

void ParameterDispatcher::setPRF(int prf)
{
    if (m_driver) m_driver->setPRF(prf);
}

void ParameterDispatcher::setRange(float range)
{
    if (m_driver) m_driver->setRange(range);
}

void ParameterDispatcher::setCurrentBeam(int beam)
{
    if (m_driver) m_driver->setCurrentBeam(beam);
}

void ParameterDispatcher::setRectify(int mode)
{
    if (m_driver) m_driver->setRectify(mode);
}

void ParameterDispatcher::setFilter(int filter)
{
    if (m_driver) m_driver->setFilter(filter);
}

void ParameterDispatcher::setADataLen(int len)
{
    if (m_driver) m_driver->setADataLen(len);
}

void ParameterDispatcher::setASmooth(bool enable)
{
    if (m_driver) m_driver->setASmooth(enable);
}

void ParameterDispatcher::setVideoDetect(bool enable)
{
    if (m_driver) m_driver->setVideoDetect(enable);
}

// ── 闸门参数 — debounce ──

void ParameterDispatcher::setGate(char gate, float start, float width,
                                   float threshold, int measureType)
{
    m_pendingGate    = gate;
    m_pendingStart   = start;
    m_pendingWidth   = width;
    m_pendingThresh  = threshold;
    m_pendingMeasure = measureType;
    m_gatePending    = true;
    m_gateTimer.start();
}

void ParameterDispatcher::doSetGate()
{
    if (!m_driver || !m_gatePending) return;
    m_gatePending = false;
    const char *mode = m_pendingMeasure == 0 ? "peak" : "edge";
    m_driver->setGate(m_pendingGate, m_pendingStart, m_pendingWidth,
                      m_pendingThresh, QString::fromLatin1(mode));
}

// ── 几何参数批量下发 ──

void ParameterDispatcher::applyLaw(const PAParams &params)
{
    if (!m_driver) return;

    m_driver->setVelocity(static_cast<float>(params.wp.lVelocity));
    m_driver->setProbeGeometry(params.probe.probeCount, params.probe.probeFreq, params.probe.probePitch);
    m_driver->setElementGeometry(params.scan.eleStart, params.scan.eleEnd, params.scan.eleAperture);
    if (params.scan.scanType == 0)
        m_driver->setSscanAngles(params.scan.angleFrom, params.scan.angleTo);
    else
        m_driver->setLscanAngle(params.scan.angle);
    m_driver->setFocusMm(params.scan.focus);
    m_driver->setWedgeGeometry(params.wedge.wedgeEnable != 0, params.wedge.wedgeAngle,
                               params.wedge.wedgeVelocity, params.wedge.wedgeHeight);
    m_driver->setScanType(params.scan.scanType);

    m_driver->setAnalogGain(params.rx.aGain);
    m_driver->setDigitalGain(params.rx.dGain);
    m_driver->setTemperatureCompensation(params.tx.tempCorrect != 0);
    m_driver->setHighVoltage(params.tx.highVoltage);
    m_driver->setPulseWidth(params.tx.pulseWidth);
    m_driver->setPRF(params.tx.prf);
    // range_ratio 依赖 ADataLen，必须先设置采样长度再设置检测范围。
    m_driver->setADataLen(params.tx.aDataLen);
    m_driver->setRange(params.tx.range);
    m_driver->setCurrentBeam(params.rx.curBeam);
    m_driver->setRectify(params.rx.rectify);
    m_driver->setFilter(params.rx.filter);

    if (params.rx.video < 5) {
        m_driver->setASmooth(false);
        m_driver->setVideoDetect(true);
    } else {
        m_driver->setVideoDetect(false);
        m_driver->setASmooth(true);
    }

    static const char gateNames[] = {'A', 'B', 'C'};
    for (int g = 0; g < 3; ++g) {
        const char *mode = params.gate.gateMeasure[g] == 0 ? "peak" : "edge";
        m_driver->setGate(gateNames[g],
                          params.gate.gateStart[g], params.gate.gateWidth[g],
                          params.gate.gateThreshold[g],
                          QString::fromLatin1(mode));
    }
    if (params.scan.scanType >= 3)
        m_driver->setTFMImageProcess(params.tfm.parRestrainH16, params.tfm.parRestrainL16,
                                     params.tfm.tfmSmooth);
}
