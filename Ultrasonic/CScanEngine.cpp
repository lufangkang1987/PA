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
        emit imageUpdated(m_image.mid(0, m_capturedLines * CScanWidth),
                          CScanWidth, m_capturedLines);
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
    m_traceBaseB = 0;
    m_traceBaseC = 0;
    std::fill(std::begin(m_shiftA1), std::end(m_shiftA1), 0);
    std::fill(std::begin(m_shiftA2), std::end(m_shiftA2), 0);
    emit imageUpdated({}, CScanWidth, 0);
    emit progressChanged(0, CScanLineCount);
}

void CScanEngine::setArchivedPackets(const QVector<DataPacket> &packets)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, packets] { setArchivedPackets(packets); },
                                  Qt::BlockingQueuedConnection);
        return;
    }
    m_archivedPackets = packets;
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

QVector<DataPacket> CScanEngine::archivedPackets() const
{
    if (QThread::currentThread() == thread()) return m_archivedPackets;
    QVector<DataPacket> result;
    QMetaObject::invokeMethod(const_cast<CScanEngine *>(this),
                              [this, &result] { result = m_archivedPackets; },
                              Qt::BlockingQueuedConnection);
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
    const uint32_t pulses = m_params.traceEnable
        ? (beam.encFwd > beam.encRvs ? beam.encFwd - beam.encRvs : beam.encRvs - beam.encFwd)
        : (m_params.direction == 1 ? beam.encRvs : beam.encFwd);
    const double stepMm = std::max(0.1, static_cast<double>(m_params.degPerPoint));
    return static_cast<int>(pulses * m_params.coderDeg / stepMm);
}

int CScanEngine::gateStartSample(int gate) const
{
    return qBound(0, qRound(m_params.gateStart[gate] * WaveSampleCount
                            / std::max(0.001f, m_params.range)), WaveSampleCount - 1);
}

int CScanEngine::gateWidthSamples(int gate) const
{
    return qBound(1, qRound(m_params.gateWidth[gate] * WaveSampleCount
                            / std::max(0.001f, m_params.range)), WaveSampleCount);
}

void CScanEngine::initializeTrace(const DataPacket &packet)
{
    if (!m_params.traceEnable || packet.beamCount <= 0) return;
    const int beam = qBound(0, m_params.curBeam, packet.beamCount - 1);
    const BeamWaveform &wave = packet.beams[beam];
    const int starts[2] = {gateStartSample(2), gateStartSample(1)};
    const int widths[2] = {gateWidthSamples(2), gateWidthSamples(1)};
    int *bases[2] = {&m_traceBaseC, &m_traceBaseB};
    const float thresholds[2] = {m_params.gateThreshold[2], m_params.gateThreshold[1]};
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

    const int currentBeam = qBound(0, m_params.curBeam, packet.beamCount - 1);
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
            if (wave.waveP[i] >= int(m_params.gateThreshold[2] * 2.5f)) {
                m_shiftA1[beam] = i - m_traceBaseC;
                break;
            }
        }
        for (int i = qMax(0, bStart); i < std::min(bStart + gateBWidth, WaveSampleCount); ++i) {
            if (wave.waveP[i] >= int(m_params.gateThreshold[1] * 2.5f)) {
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
    if (m_explicitRules.size() >= count) {
        std::copy_n(m_explicitRules.cbegin(), count, rules);
        return;
    }
    const bool hasPositions = m_rulePositions.size() >= count;
    if (m_params.scanType == 0) {
        const double angleRange = m_params.angleTo - m_params.angleFrom;
        for (int b = 0; b < count; ++b) {
            const double t = count > 1 ? static_cast<double>(b) / (count - 1) : 0.0;
            rules[b].ang = m_params.angleFrom + angleRange * t;
            if (!hasPositions) {
                rules[b].x = qBound(0.0, 255.0 + (t - 0.5) * 400.0, 511.0);
            } else if (m_params.angleFrom > -0.01f) {
                rules[b].x = 55.0 + (m_rulePositions[b] - m_rulePositions[0])
                    / std::max(0.001f, m_params.range) * 400.0;
            } else if (m_params.angleTo < 0.01f) {
                rules[b].x = 455.0 + (m_rulePositions[b] - m_rulePositions[0])
                    / std::max(0.001f, m_params.range) * 400.0;
            } else {
                rules[b].x = 255.0
                    + (m_rulePositions[b] - (m_rulePositions[0] + m_rulePositions[count - 1]) / 2.0)
                        / std::max(0.001f, m_params.range) * 400.0
                    + (std::abs(m_params.angleFrom) - std::abs(m_params.angleTo)) * 200.0 / 90.0;
                rules[b].x = std::min(rules[b].x, 455.0
                    + (m_rulePositions[b] - m_rulePositions[0])
                        / std::max(0.001f, m_params.range) * 400.0);
            }
        }
    } else if (m_params.scanType == 1) {
        const double angle = m_params.angle;
        const double usable = std::max(1.0, 512.0 - 400.0 * std::sin(angle * M_PI / 180.0));
        for (int b = 0; b < count; ++b) {
            const double t = count > 1 ? static_cast<double>(b) / (count - 1) : 0.0;
            rules[b].x = usable * t;
            if (hasPositions && angle > 0.01)
                rules[b].x = (m_rulePositions[b] - m_rulePositions[0])
                    / std::max(0.001f, m_params.range) * 400.0;
            rules[b].ang = angle;
        }
    } else {
        for (int b = 0; b < count; ++b) {
            const double t = count > 1 ? static_cast<double>(b) / (count - 1) : 0.0;
            rules[b].x = 511.0 * t;
            rules[b].ang = m_params.angle;
        }
    }

    // ── 计算成像跨度 (MFC GetScanRules 中 ImgSpanStart/End) ──
    if (m_params.scanType == 0) {
        const double dotMm = m_params.range / 400.0;
        m_imgSpanStart = static_cast<float>(dotMm * (0.0 - rules[0].x));
        m_imgSpanEnd   = static_cast<float>(dotMm * (510.0 - rules[0].x));
    } else if (m_params.scanType == 1) {
        const float halfMm = m_params.probePitch * (m_params.probeCount - m_params.eleAperture) / 2.0f;
        m_imgSpanStart = -halfMm;
        m_imgSpanEnd   =  halfMm;
    } else {
        m_imgSpanStart = -180.0f;
        m_imgSpanEnd   = static_cast<float>(180.0 - 360.0 / std::max(1, m_params.probeCount));
    }
}

QVector<uint8_t> CScanEngine::softwareImaging(const DataPacket &packet)
{
    QVector<uint8_t> image(SImageSize, 0xFF);
    const int count = qBound(0, packet.beamCount, MaxBeams);
    if (count < 2)
        return image;

    ScanRule rules[MaxBeams] = {};
    computeScanRules(count, rules);
    double cosA[MaxBeams], sinA[MaxBeams];
    for (int b = 0; b < count; ++b) {
        const double rad = rules[b].ang * M_PI / 180.0;
        cosA[b] = std::cos(rad);
        sinA[b] = std::sin(rad);
    }

    uint8_t *out = image.data();
    for (int y = 399; y >= 0; --y) {
        int b = 0;
        for (int x = 0; x < CScanWidth; ++x, ++out) {
            double x1 = 0.0;
            for (; b < count; ++b) {
                x1 = (x - rules[b].x) * cosA[b] - y * sinA[b];
                if (x1 < 0.0) break;
            }
            if (b == 0 || b == count) continue;

            const double y1 = y * cosA[b] + (x - rules[b].x) * sinA[b];
            const double y0 = y * cosA[b - 1] + (x - rules[b - 1].x) * sinA[b - 1];
            const int n0 = static_cast<int>(y0 + 1.0);
            const int n1 = static_cast<int>(y1 + 1.0);
            if (n0 < 0 || n1 < 0 || n0 >= WaveSampleCount || n1 >= WaveSampleCount)
                continue;

            const double x0 = (x - rules[b - 1].x) * cosA[b - 1] - y * sinA[b - 1];
            const double d = x0 - x1;
            const int v0 = packet.beams[b - 1].waveP[n0];
            const int v1 = packet.beams[b].waveP[n1];
            int value = v1;
            if (x0 < 0.25 * d) value = v0;
            else if (x0 < 0.75 * d) value = (v0 + v1) / 2;
            *out = static_cast<uint8_t>(std::min(value, 250));
        }
    }
    return image;
}

QVector<float> CScanEngine::buildCScanRow(const QVector<uint8_t> &sImage)
{
    QVector<float> row(CScanWidth, 0.0f);
    const int x1 = qBound(0, std::min(m_params.imgLineX1, m_params.imgLineX2), CScanWidth);
    const int x2 = qBound(0, std::max(m_params.imgLineX1, m_params.imgLineX2), CScanWidth);
    const int y1 = qBound(0, std::min(m_params.imgLineY1, m_params.imgLineY2), WaveSampleCount);
    const int y2 = qBound(0, std::max(m_params.imgLineY1, m_params.imgLineY2), WaveSampleCount);

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

void CScanEngine::processPacket(const DataPacket &packet)
{
    if (!m_scanning || packet.beamCount <= 0)
        return;

    DataPacket workingPacket = packet;
    if (m_params.traceEnable)
        applyTrace(workingPacket);

    int lineIndex = encoderLine(workingPacket.beams[0]);
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

    const QVector<float> row = m_params.traceEnable
        ? buildTraceRow(workingPacket)
        : buildCScanRow(softwareImaging(workingPacket));
    if (m_lastLine <= lineIndex) {
        const int first = m_lastLine < 0 ? 0 : m_lastLine + 1;
        if (m_archivedPackets.size() <= lineIndex)
            m_archivedPackets.resize(lineIndex + 1);
        for (int y = first; y <= lineIndex; ++y) {
            std::copy(row.cbegin(), row.cend(), m_image.begin() + y * CScanWidth);
            m_archivedPackets[y] = workingPacket;
        }
    } else {
        std::fill(m_image.begin() + lineIndex * CScanWidth,
                  m_image.begin() + (lineIndex + 1) * CScanWidth, 0.0f);
        std::copy(row.cbegin(), row.cend(), m_image.begin() + lineIndex * CScanWidth);
        if (m_archivedPackets.size() <= lineIndex)
            m_archivedPackets.resize(lineIndex + 1);
        m_archivedPackets[lineIndex] = workingPacket;
    }

    m_lastLine = lineIndex;
    m_capturedLines = std::max(m_capturedLines, lineIndex + 1);
    const qint64 elapsedMs = m_scanTimer.isValid() ? m_scanTimer.elapsed() : 0;
    if (m_lastImageMs == 0 || elapsedMs - m_lastImageMs >= 50
            || m_capturedLines >= CScanLineCount) {
        emit imageUpdated(m_image.mid(0, m_capturedLines * CScanWidth),
                          CScanWidth, m_capturedLines);
        m_lastImageMs = elapsedMs;
    }
    emit progressChanged(m_capturedLines, CScanLineCount);
    if (elapsedMs - m_lastMetricMs >= 100 || m_capturedLines >= CScanLineCount) {
        const int deltaLines = m_capturedLines - m_lastMetricLines;
        const qint64 deltaMs = elapsedMs - m_lastMetricMs;
        const double speed = deltaMs > 0
            ? deltaLines * m_params.degPerPoint * 1000.0 / deltaMs : 0.0;
        const double average = elapsedMs > 0
            ? m_capturedLines * m_params.degPerPoint * 1000.0 / elapsedMs : 0.0;
        emit metricsChanged(m_capturedLines, CScanLineCount,
                            m_capturedLines * m_params.degPerPoint, speed, average);
        m_lastMetricMs = elapsedMs;
        m_lastMetricLines = m_capturedLines;
    }

    if (m_capturedLines >= CScanLineCount) {
        stop();
        emit scanCompleted();
    }
}
