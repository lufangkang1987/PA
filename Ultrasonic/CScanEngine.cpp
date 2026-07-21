#include "CScanEngine.h"
#include <QMetaObject>
#include <QThread>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace {
constexpr int CScanLineCount = MaxCScanFrames - 1; // Original limit: 925 * 3 = 2775.
}

CScanEngine::CScanEngine(QObject *parent)
    : QObject(parent), m_image(MaxCScanFrames * CScanWidth, 0.0f)
{
}

void CScanEngine::configure(const PAParams &params)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, params] { configure(params); },
                                  Qt::BlockingQueuedConnection);
        return;
    }
    m_params = params;
}

void CScanEngine::start()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this] { start(); }, Qt::BlockingQueuedConnection);
        return;
    }
    clear();
    m_scanTimer.start();
    m_lastMetricMs = 0;
    m_lastImageMs = 0;
    m_lastMetricLines = 0;
    m_scanning = true;
}

void CScanEngine::stop()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this] { stop(); }, Qt::BlockingQueuedConnection);
        return;
    }
    if (m_scanning && m_capturedLines > 0) {
        auto plain = std::make_shared<QVector<DataPacket>>();
        plain->reserve(m_archivedPackets.size());
        for (const auto &sp : m_archivedPackets)
            plain->append(sp ? *sp : DataPacket{});
        m_archivedSnapshot = plain;
        emit imageUpdated(m_image.mid(0, m_capturedLines * CScanWidth),
                          CScanWidth, m_capturedLines,
                          m_imgSpanStart, m_imgSpanEnd);
    }
    m_scanning = false;
}

void CScanEngine::clear()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this] { clear(); }, Qt::BlockingQueuedConnection);
        return;
    }
    std::fill(m_image.begin(), m_image.end(), 0.0f);
    m_capturedLines = 0;
    m_lastLine = -1;
    m_archivedPackets.clear();
    m_archivedSnapshot.reset();
    m_traceBaseB = 0;
    m_traceBaseC = 0;
    m_traceBaseInitialized = false;
    std::fill(std::begin(m_shiftA1), std::end(m_shiftA1), 0);
    std::fill(std::begin(m_shiftA2), std::end(m_shiftA2), 0);
    emit imageUpdated({}, CScanWidth, 0, m_imgSpanStart, m_imgSpanEnd);
    emit progressChanged(0, CScanLineCount);
}

void CScanEngine::setArchivedPackets(const QVector<DataPacket> &packets)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, packets] { setArchivedPackets(packets); },
                                  Qt::BlockingQueuedConnection);
        return;
    }
    m_archivedPackets.clear();
    m_archivedPackets.reserve(packets.size());
    for (const auto &p : packets)
        m_archivedPackets.append(std::make_shared<const DataPacket>(p));
    auto plain = std::make_shared<QVector<DataPacket>>(packets);
    m_archivedSnapshot = plain;
}

void CScanEngine::setRulePositions(const QVector<double> &positions)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, positions] { setRulePositions(positions); },
                                  Qt::QueuedConnection);
        return;
    }
    m_rulePositions = positions.mid(0, MaxBeams);
    m_explicitRules.clear();
}

void CScanEngine::setScanRules(const QVector<ScanRule> &rules)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, rules] { setScanRules(rules); },
                                  Qt::BlockingQueuedConnection);
        return;
    }
    m_explicitRules = rules.mid(0, MaxBeams);
}

bool CScanEngine::isScanning() const
{
    if (QThread::currentThread() == thread()) return m_scanning;
    bool result = false;
    QMetaObject::invokeMethod(const_cast<CScanEngine *>(this),
                              [this, &result] { result = m_scanning; },
                              Qt::BlockingQueuedConnection);
    return result;
}

int CScanEngine::capturedLines() const
{
    if (QThread::currentThread() == thread()) return m_capturedLines;
    int result = 0;
    QMetaObject::invokeMethod(const_cast<CScanEngine *>(this),
                              [this, &result] { result = m_capturedLines; },
                              Qt::BlockingQueuedConnection);
    return result;
}

QVector<float> CScanEngine::image() const
{
    if (QThread::currentThread() == thread()) return m_image;
    QVector<float> result;
    QMetaObject::invokeMethod(const_cast<CScanEngine *>(this),
                              [this, &result] { result = m_image; },
                              Qt::BlockingQueuedConnection);
    return result;
}

std::shared_ptr<const QVector<DataPacket>> CScanEngine::archivedPackets() const
{
    auto build = [](const QVector<std::shared_ptr<const DataPacket>> &src) {
        auto v = std::make_shared<QVector<DataPacket>>();
        v->reserve(src.size());
        for (const auto &sp : src)
            v->append(sp ? *sp : DataPacket{});
        return std::shared_ptr<const QVector<DataPacket>>(v);
    };
    if (QThread::currentThread() == thread())
        return m_archivedSnapshot ? m_archivedSnapshot : build(m_archivedPackets);
    std::shared_ptr<const QVector<DataPacket>> result;
    QMetaObject::invokeMethod(const_cast<CScanEngine *>(this),
                              [this, &result, &build] {
        result = m_archivedSnapshot ? m_archivedSnapshot : build(m_archivedPackets);
    }, Qt::BlockingQueuedConnection);
    return result;
}

QVector<ScanRule> CScanEngine::currentScanRules(int beamCount)
{
    if (QThread::currentThread() != thread()) {
        QVector<ScanRule> result;
        QMetaObject::invokeMethod(const_cast<CScanEngine *>(this),
                                  [this, beamCount, &result] {
                                      result = currentScanRules(beamCount);
                                  }, Qt::BlockingQueuedConnection);
        return result;
    }
    QVector<ScanRule> rules(qBound(0, beamCount, MaxBeams));
    if (!rules.isEmpty()) computeScanRules(rules.size(), rules.data());
    return rules;
}

int CScanEngine::encoderLine(const BeamWaveform &beam) const
{
    // Keep the original MFC MakeCRecord / MakeCRecord_trace semantics:
    // normal: direction 0=fwd, 1=rvs, >1=time base;
    // trace : direction 0/1=absolute encoder difference, >1=time base.
    quint64 counter = 0;
    const int direction = m_params.enc.direction;
    if (direction > 1) {
        const qint64 elapsedMs = m_scanTimer.isValid() ? m_scanTimer.elapsed() : 0;
        counter = quint64(qMax<qint64>(0, elapsedMs)) * quint64(direction - 1);
    } else if (m_params.wp.traceEnable) {
        counter = beam.encFwd >= beam.encRvs
            ? quint64(beam.encFwd - beam.encRvs)
            : quint64(beam.encRvs - beam.encFwd);
    } else {
        counter = direction == 1 ? quint64(beam.encRvs) : quint64(beam.encFwd);
    }

    const double stepMm = std::max(0.1, static_cast<double>(m_params.img.degPerPoint));
    return static_cast<int>(counter * m_params.enc.coderDeg / stepMm);
}

int CScanEngine::gateStartSample(int gate) const
{
    return qBound(0, qRound(m_params.gate.gateStart[gate] * WaveSampleCount
                            / std::max(0.001f, m_params.tx.range)), WaveSampleCount - 1);
}

int CScanEngine::gateWidthSamples(int gate) const
{
    return qBound(1, qRound(m_params.gate.gateWidth[gate] * WaveSampleCount
                            / std::max(0.001f, m_params.tx.range)), WaveSampleCount);
}

void CScanEngine::initializeTrace(const DataPacket &packet)
{
    if (!m_params.wp.traceEnable || packet.beamCount <= 0 || m_traceBaseInitialized)
        return;
    // MFC InitTrace samples the current beam once when trace mode is enabled.
    // A missing B/C threshold crossing leaves that base at zero; it is not
    // silently moved to a later frame.
    m_traceBaseInitialized = true;
    const int beam = qBound(0, m_params.rx.curBeam, packet.beamCount - 1);
    const BeamWaveform &wave = packet.beams[beam];
    const int starts[2] = {gateStartSample(2), gateStartSample(1)};
    const int widths[2] = {gateWidthSamples(2), gateWidthSamples(1)};
    int *bases[2] = {&m_traceBaseC, &m_traceBaseB};
    const float thresholds[2] = {m_params.gate.gateThreshold[2], m_params.gate.gateThreshold[1]};
    for (int gate = 0; gate < 2; ++gate) {
        if (*bases[gate] != 0) continue;
        const int end = std::min(starts[gate] + widths[gate], WaveSampleCount);
        for (int i = starts[gate]; i < end; ++i) {
            if (wave.waveP[i] >= int(thresholds[gate] * 2.5f)) {
                *bases[gate] = i;
                break;
            }
        }
    }
}

void CScanEngine::applyTrace(DataPacket &packet)
{
    initializeTrace(packet);
    if (m_traceBaseB == 0 || m_traceBaseC == 0) return;

    const int currentBeam = qBound(0, m_params.rx.curBeam, packet.beamCount - 1);
    const int gateBStart = gateStartSample(1);
    const int gateBWidth = gateWidthSamples(1);
    int bStart = gateBStart;
    if (m_shiftA2[currentBeam] < -gateBWidth || m_shiftA2[currentBeam] > gateBWidth)
        bStart += m_shiftA2[currentBeam];

    const int gateCStart = gateStartSample(2);
    const int gateCWidth = gateWidthSamples(2);
    const int gateAStart = gateStartSample(0);
    const int gateAWidth = gateWidthSamples(0);
    for (int beam = 0; beam < packet.beamCount; ++beam) {
        BeamWaveform &wave = packet.beams[beam];
        for (int i = gateCStart; i < std::min(gateCStart + gateCWidth, WaveSampleCount); ++i) {
            if (wave.waveP[i] >= int(m_params.gate.gateThreshold[2] * 2.5f)) {
                m_shiftA1[beam] = i - m_traceBaseC;
                break;
            }
        }
        for (int i = qMax(0, bStart); i < std::min(bStart + gateBWidth, WaveSampleCount); ++i) {
            if (wave.waveP[i] >= int(m_params.gate.gateThreshold[1] * 2.5f)) {
                m_shiftA2[beam] = i - m_traceBaseB;
                break;
            }
        }
        wave.amp0 = 0;
        wave.path0 = 0;
        for (int i = gateAStart; i < std::min(gateAStart + gateAWidth, WaveSampleCount); ++i) {
            if (wave.amp0 < wave.waveP[i]) {
                wave.amp0 = wave.waveP[i];
                wave.path0 = static_cast<uint16_t>(i);
            }
        }
    }
}

QVector<float> CScanEngine::buildTraceRow(const DataPacket &packet) const
{
    QVector<float> row(CScanWidth, 0.0f);
    if (packet.beamCount < 2) return row;
    QVector<int> maxima(packet.beamCount, 0);
    const int aStart = gateStartSample(0);
    const int aWidth = gateWidthSamples(0);
    for (int beam = 0; beam < packet.beamCount; ++beam) {
        const int start = qBound(0, aStart + m_shiftA1[beam], WaveSampleCount - 1);
        const int end = qBound(start + 1, aStart + aWidth + m_shiftA2[beam], WaveSampleCount);
        for (int i = start; i < end; ++i)
            maxima[beam] = std::max(maxima[beam], int(packet.beams[beam].waveP[i]));
    }
    const double step = 511.0 / (packet.beamCount - 1);
    for (int x = 0; x < CScanWidth; ++x) {
        const int beam = int(x / step);
        if (beam >= packet.beamCount - 1) continue;
        const double fraction = (x - beam * step) / step;
        row[x] = float((maxima[beam] + (maxima[beam + 1] - maxima[beam]) * fraction) / 255.0);
    }
    return row;
}

void CScanEngine::computeScanRules(int beamCount, ScanRule *rules)
{
    const int count = qBound(1, beamCount, MaxBeams);
    ::computeScanRules(m_params.scan, m_params.probe, m_params.tx.range,
                       m_params.global.beamCount,
                       &m_rulePositions, &m_explicitRules, rules,
                       &m_imgSpanStart, &m_imgSpanEnd);
    Q_UNUSED(count);
}

QVector<uint8_t> CScanEngine::softwareImaging(const DataPacket &packet)
{
    QVector<uint8_t> image(SImageSize, 0xFF);
    const int count = qBound(0, packet.beamCount, MaxBeams);
    if (count < 2) return image;
    ScanRule rules[MaxBeams] = {};
    computeScanRules(count, rules);
    const uint8_t *waveP[MaxBeams];
    for (int b = 0; b < count; ++b)
        waveP[b] = packet.beams[b].waveP;
    ::softwareImaging(waveP, rules, count, image.data());
    return image;
}

QVector<float> CScanEngine::buildCScanRow(const QVector<uint8_t> &sImage)
{
    QVector<float> row(CScanWidth, 0.0f);
    const int x1 = qBound(0, std::min(m_params.img.imgLineX1, m_params.img.imgLineX2), CScanWidth);
    const int x2 = qBound(0, std::max(m_params.img.imgLineX1, m_params.img.imgLineX2), CScanWidth);
    const int y1 = qBound(0, std::min(m_params.img.imgLineY1, m_params.img.imgLineY2), WaveSampleCount);
    const int y2 = qBound(0, std::max(m_params.img.imgLineY1, m_params.img.imgLineY2), WaveSampleCount);

    for (int x = x1; x < x2; ++x) {
        uint8_t maximum = 0;
        for (int y = y1; y < y2; ++y) {
            const uint8_t value = sImage[(399 - y) * CScanWidth + x];
            if (value < 255) maximum = std::max(maximum, value);
        }
        row[x] = maximum / 255.0f;
    }
    return row;
}

void CScanEngine::processPacket(std::shared_ptr<DataPacket> packet)
{
    if (!m_scanning || !packet || packet->beamCount <= 0)
        return;

    // 仅在 trace 模式下拷贝 DataPacket（~53KB），非 trace 模式直接读原始数据
    DataPacket workingPacket;
    const DataPacket *wp = packet.get();
    if (m_params.wp.traceEnable) {
        workingPacket = *packet;
        applyTrace(workingPacket);
        wp = &workingPacket;
    }

    int lineIndex = encoderLine(wp->beams[0]);
    if (lineIndex < 0)
        return;
    if (lineIndex >= CScanLineCount) {
        if (m_capturedLines < CScanLineCount)
            lineIndex = CScanLineCount - 1;
        else {
            stop();
            emit scanCompleted();
            return;
        }
    }

    const QVector<float> row = m_params.wp.traceEnable
        ? buildTraceRow(*wp)
        : buildCScanRow(softwareImaging(*wp));
    const int writeX1 = m_params.wp.traceEnable
        ? 0 : qBound(0, std::min(m_params.img.imgLineX1, m_params.img.imgLineX2), CScanWidth);
    const int writeX2 = m_params.wp.traceEnable
        ? CScanWidth - 1
        : qBound(0, std::max(m_params.img.imgLineX1, m_params.img.imgLineX2), CScanWidth);
    const auto writeCImageRow = [this, &row, writeX1, writeX2](int targetLine,
                                                               bool clearFirst) {
        const int base = targetLine * CScanWidth;
        if (clearFirst) {
            std::fill(m_image.begin() + base + writeX1,
                      m_image.begin() + base + writeX2, 0.0f);
        }
        std::copy(row.cbegin() + writeX1, row.cbegin() + writeX2,
                  m_image.begin() + base + writeX1);
    };
    if (m_capturedLines <= lineIndex) {
        const int first = m_capturedLines;
        if (m_archivedPackets.size() <= lineIndex)
            m_archivedPackets.resize(lineIndex + 1);
        for (int y = first; y <= lineIndex; ++y) {
            writeCImageRow(y, false);
            m_archivedPackets[y] = packet;  // shared_ptr：连续行共享同一包
        }
    } else {
        writeCImageRow(lineIndex, true);
        if (m_archivedPackets.size() <= lineIndex)
            m_archivedPackets.resize(lineIndex + 1);
        m_archivedPackets[lineIndex] = packet;
    }

    m_lastLine = lineIndex;
    m_capturedLines = std::max(m_capturedLines, lineIndex + 1);
    const qint64 elapsedMs = m_scanTimer.isValid() ? m_scanTimer.elapsed() : 0;
    if (m_lastImageMs == 0 || elapsedMs - m_lastImageMs >= 50
            || m_capturedLines >= CScanLineCount) {
        emit imageUpdated(m_image.mid(0, m_capturedLines * CScanWidth),
                          CScanWidth, m_capturedLines,
                          m_imgSpanStart, m_imgSpanEnd);
        m_lastImageMs = elapsedMs;
    }
    emit progressChanged(m_capturedLines, CScanLineCount);
    if (elapsedMs - m_lastMetricMs >= 100 || m_capturedLines >= CScanLineCount) {
        const int deltaLines = m_capturedLines - m_lastMetricLines;
        const qint64 deltaMs = elapsedMs - m_lastMetricMs;
        const double speed = deltaMs > 0
            ? deltaLines * m_params.img.degPerPoint * 1000.0 / deltaMs : 0.0;
        const double average = elapsedMs > 0
            ? m_capturedLines * m_params.img.degPerPoint * 1000.0 / elapsedMs : 0.0;
        emit metricsChanged(m_capturedLines, CScanLineCount,
                            m_capturedLines * m_params.img.degPerPoint, speed, average);
        m_lastMetricMs = elapsedMs;
        m_lastMetricLines = m_capturedLines;
    }

    if (m_capturedLines >= CScanLineCount) {
        stop();
        emit scanCompleted();
    }
}
