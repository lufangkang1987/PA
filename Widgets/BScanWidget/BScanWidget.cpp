#include "BScanWidget.h"

#include "ColorMap.h"
#include "PAParams.h"
#include "Theme.h"

#include <QPainter>
#include <QMouseEvent>
#include <QtMath>
#include <algorithm>

BScanWidget::BScanWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setMinimumWidth(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_sImage.resize(SImageSize, 0xFF);
    m_mapBeam0.resize(SImageSize, 0xFF);
    m_mapBeam1.resize(SImageSize, 0xFF);
    m_mapSample0.resize(SImageSize);
    m_mapSample1.resize(SImageSize);
    m_mapBlend.resize(SImageSize);
    buildColorLUT();
    m_displayImage = buildDisplayImage();
}

void BScanWidget::setParamsSource(const PAParams *params)
{
    m_params = params;
    m_selectedBeam = -1;  // 扫查参数变化后清除旧的声束选择
    int beamCount = m_scan.beamCount;
    if (m_params) {
        // MFC OnRuleReset: L-scan rule count is determined by the active
        // element interval and aperture, not by the global 128-beam slot.
        beamCount = m_params->scan.scanType == 1
            ? m_params->scan.eleEnd - m_params->scan.eleStart + 2
                - m_params->scan.eleAperture
            : m_params->global.beamCount;
    }
    computeScanRules(qBound(1, beamCount, MaxBeams));
    m_displayImage = buildDisplayImage();
    update();
}

void BScanWidget::buildColorLUT()
{
    m_colorLUT.resize(256);
    for (int i = 0; i < 256; ++i)
        m_colorLUT[i] = amplitudeToColor(i);
}

void BScanWidget::computeScanRules(int beamCount)
{
    const int count = qBound(1, beamCount, MaxBeams);
    ScanParams fallbackScan;
    ProbeParams fallbackProbe;
    const ScanParams *scan = nullptr;
    const ProbeParams *probe = nullptr;
    float range = m_scan.range;

    if (m_params) {
        scan = &m_params->scan;
        probe = &m_params->probe;
        range = m_params->tx.range;
    } else {
        fallbackScan.scanType = m_scan.scanType;
        fallbackScan.angleFrom = m_scan.angleFrom;
        fallbackScan.angleTo = m_scan.angleTo;
        fallbackScan.angle = m_scan.angleFrom;
        fallbackScan.eleStart = m_scan.eleStart;
        fallbackScan.eleEnd = m_scan.eleEnd;
        fallbackScan.eleAperture = m_scan.eleAperture;
        fallbackProbe.probeCount = m_scan.probeCount;
        fallbackProbe.probePitch = 1.0f;
        scan = &fallbackScan;
        probe = &fallbackProbe;
    }

    if (scan->scanType == 3) {
        std::fill_n(m_rules, MaxBeams, ScanRule{});
        m_ruleBeamCount = count;
        rebuildImagingMap(0);
        return;
    }

    ::computeScanRules(*scan, *probe, range, count,
                       m_rulePositions.size() >= count ? &m_rulePositions : nullptr,
                       nullptr, m_rules, &m_imgSpanStart, &m_imgSpanEnd);
    m_ruleBeamCount = count;
    rebuildImagingMap(count);
}

void BScanWidget::rebuildImagingMap(int beamCount)
{
    std::fill(m_mapBeam0.begin(), m_mapBeam0.end(), quint8(0xFF));
    if (beamCount < 2) return;

    double cosA[MaxBeams], sinA[MaxBeams];
    for (int b = 0; b < beamCount; ++b) {
        const double rad = qDegreesToRadians(double(m_rules[b].ang));
        cosA[b] = std::cos(rad);
        sinA[b] = std::sin(rad);
    }

    int outIndex = 0;
    for (int y = WaveSampleCount - 1; y >= 0; --y) {
        int b = 0;
        for (int x = 0; x < CScanWidth; ++x, ++outIndex) {
            double x1 = 0.0;
            for (; b < beamCount; ++b) {
                x1 = (x - m_rules[b].x) * cosA[b] - y * sinA[b];
                if (x1 < 0.0) break;
            }
            if (b == 0 || b == beamCount) continue;

            const double y1 = y * cosA[b] + (x - m_rules[b].x) * sinA[b];
            const double y0 = y * cosA[b - 1] + (x - m_rules[b - 1].x) * sinA[b - 1];
            const int n0 = int(y0 + 1.0);
            const int n1 = int(y1 + 1.0);
            if (n0 < 0 || n1 < 0 || n0 >= WaveSampleCount || n1 >= WaveSampleCount)
                continue;

            const double x0 = (x - m_rules[b - 1].x) * cosA[b - 1] - y * sinA[b - 1];
            const double d = x0 - x1;
            m_mapBeam0[outIndex] = quint8(b - 1);
            m_mapBeam1[outIndex] = quint8(b);
            m_mapSample0[outIndex] = quint16(n0);
            m_mapSample1[outIndex] = quint16(n1);
            m_mapBlend[outIndex] = x0 < 0.25 * d ? 0 : (x0 < 0.75 * d ? 1 : 2);
        }
    }
}

void BScanWidget::setRulePositions(const QVector<double> &positions)
{
    m_rulePositions = positions.mid(0, MaxBeams);
    const int configuredCount = m_params ? m_params->global.beamCount : m_scan.beamCount;
    const int effectiveCount = m_rulePositions.isEmpty()
        ? configuredCount : qMin(configuredCount, m_rulePositions.size());
    computeScanRules(effectiveCount);
    m_displayImage = buildDisplayImage();
    update();
}

void BScanWidget::softwareImaging(
    const std::vector<std::array<uint8_t, WaveSampleCount>> &waveforms,
    int beamCount, uint8_t *img)
{
    std::fill_n(img, SImageSize, uint8_t(0xFF));
    const int count = qBound(0, beamCount, MaxBeams);
    for (int i = 0; i < SImageSize; ++i) {
        const int b0 = m_mapBeam0[i];
        if (b0 == 0xFF || b0 >= count) continue;
        const int b1 = m_mapBeam1[i];
        if (b1 >= count) continue;
        const int v0 = waveforms[b0][m_mapSample0[i]];
        const int v1 = waveforms[b1][m_mapSample1[i]];
        const int value = m_mapBlend[i] == 0 ? v0 : (m_mapBlend[i] == 1 ? (v0 + v1) / 2 : v1);
        img[i] = uint8_t(qMin(value, 250));
    }
}

void BScanWidget::softwareImaging(const DataPacket &packet, int beamCount, uint8_t *img)
{
    std::fill_n(img, SImageSize, uint8_t(0xFF));
    const int count = qBound(0, qMin(beamCount, packet.beamCount), MaxBeams);
    for (int i = 0; i < SImageSize; ++i) {
        const int b0 = m_mapBeam0[i];
        if (b0 == 0xFF || b0 >= count) continue;
        const int b1 = m_mapBeam1[i];
        if (b1 >= count) continue;
        const int v0 = packet.beams[b0].waveP[m_mapSample0[i]];
        const int v1 = packet.beams[b1].waveP[m_mapSample1[i]];
        const int value = m_mapBlend[i] == 0 ? v0
            : (m_mapBlend[i] == 1 ? (v0 + v1) / 2 : v1);
        img[i] = uint8_t(qMin(value, 250));
    }
}

QImage BScanWidget::buildDisplayImage() const
{
    QImage img(CScanWidth, WaveSampleCount, QImage::Format_RGB32);
    if (!m_hasData) {
        img.fill(ThemeColor::DeepBg.rgb());
        return img;
    }

    for (int y = 0; y < WaveSampleCount; ++y) {
        const int srcRow = WaveSampleCount - 1 - y;
        const uint8_t *src = m_sImage.data() + srcRow * CScanWidth;
        QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(y));

        for (int x = 0; x < CScanWidth; ++x) {
            const uint8_t val = src[x];
            if (val >= 255)
                dst[x] = ThemeColor::DeepBg.rgb();
            else if (val == 0)
                dst[x] = qRgb(255, 255, 255);
            else
                dst[x] = m_colorLUT[val];
        }
    }

    return img;
}

void BScanWidget::setMultiBeamData(const QVector<QVector<double>> &waves, bool isRF)
{
    if (m_frozen) return;

    const int beamCount = waves.size();
    if (beamCount < 1) return;
    // MFC 使用设备规则对应的实际声束数。数据包的存储槽可能仍为128，
    // 不能因此丢弃数量较少的有效规则，否则会退回55..455的宽顶假扇形。
    int effectiveBeamCount = qMin(beamCount, MaxBeams);
    if (!m_rulePositions.isEmpty())
        effectiveBeamCount = qMin(effectiveBeamCount, m_rulePositions.size());

    if (effectiveBeamCount != m_ruleBeamCount)
        computeScanRules(effectiveBeamCount);

    std::vector<std::array<uint8_t, WaveSampleCount>> rawWaves(effectiveBeamCount);
    for (int b = 0; b < effectiveBeamCount; ++b) {
        const QVector<double> &src = waves[b];
        for (int i = 0; i < WaveSampleCount; ++i) {
            const double v = (isRF ? qAbs(src.value(i, 0.0)) : src.value(i, 0.0)) * 255.0;
            rawWaves[b][i] = static_cast<uint8_t>(qBound(0.0, v, 255.0));
        }
    }

    softwareImaging(rawWaves, effectiveBeamCount, m_sImage.data());

    m_hasData = true;
    m_displayImage = buildDisplayImage();
    update();
}

void BScanWidget::renderFromPacket(const std::shared_ptr<DataPacket> &packet)
{
    if (m_frozen || !packet || packet->beamCount < 1) return;

    int effectiveBeamCount = qMin(packet->beamCount, MaxBeams);
    if (!m_rulePositions.isEmpty())
        effectiveBeamCount = qMin(effectiveBeamCount, m_rulePositions.size());
    if (effectiveBeamCount != m_ruleBeamCount)
        computeScanRules(effectiveBeamCount);

    softwareImaging(*packet, effectiveBeamCount, m_sImage.data());
    m_hasData = true;
    m_displayImage = buildDisplayImage();
    update();
}

void BScanWidget::setScanParams(int scanType, float angleFrom, float angleTo,
                                int beamCount, float range,
                                int probeCount, int eleStart,
                                int eleEnd, int eleAperture)
{
    m_scan.scanType = scanType;
    m_scan.angleFrom = angleFrom;
    m_scan.angleTo = angleTo;
    m_scan.beamCount = beamCount;
    m_scan.range = range;
    m_scan.probeCount = probeCount;
    m_scan.eleStart = eleStart;
    m_scan.eleEnd = eleEnd;
    m_scan.eleAperture = eleAperture;

    if (!m_params)
        computeScanRules(m_scan.beamCount);
}

void BScanWidget::setAcousticParams(float velocity, float range, int sampleRate)
{
    Q_UNUSED(velocity);
    Q_UNUSED(sampleRate);
    if (!m_params) {
        m_scan.range = range;
        computeScanRules(m_scan.beamCount);
    }
}

void BScanWidget::setFrozen(bool frozen)
{
    m_frozen = frozen;
}

void BScanWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), ThemeColor::DeepBg);

    const int ml = qMin(62, qMax(48, width() / 9));
    const int mt = qMin(38, qMax(30, height() / 10));
    const int mr = qMin(50, qMax(22, width() / 8));
    const int mb = qMin(18, qMax(8, height() / 20));
    QRect plot = rect().adjusted(ml, mt, -mr, -mb);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    // MFC renders the 512 x 400 software-imaging buffer at almost its native
    // aspect ratio (about 1.28).  Stretching it to the whole Qt panel makes
    // the sector roughly 15-20% wider on the current wide layout.  Keep the
    // same imaging aspect ratio and make the rulers describe that exact area.
    constexpr double imageAspect = double(CScanWidth) / WaveSampleCount;
    const int aspectWidth = qRound(plot.height() * imageAspect);
    if (aspectWidth < plot.width()) {
        const int left = plot.left() + (plot.width() - aspectWidth) / 2;
        plot.setLeft(left);
        plot.setWidth(aspectWidth);
    } else {
        const int aspectHeight = qRound(plot.width() / imageAspect);
        if (aspectHeight < plot.height())
            plot.setHeight(aspectHeight);
    }

    if (!m_displayImage.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        const QRect target = imageRect();
        if (target.isValid()) {
            p.drawImage(target, m_displayImage);

            // 用户点击选择的声束指示虚线
            if (m_selectedBeam >= 0 && m_selectedBeam < m_ruleBeamCount) {
            const ScanRule &rule = m_rules[m_selectedBeam];
            const double rad = qDegreesToRadians(double(rule.ang));
            const double x0 = rule.x;
            const double y0 = 0.0;
            const double x1 = x0 + 400.0 * std::sin(rad);
            const double y1 = y0 + 400.0 * std::cos(rad);

            auto mapX = [&target](double ix) { return target.left() + ix * target.width() / double(CScanWidth); };
            auto mapY = [&target](double iy) { return target.top()  + iy * target.height() / double(WaveSampleCount); };

            QPen beamPen(QColor(255, 220, 60), 1.0, Qt::DashLine);  // 亮黄色虚线
            p.setPen(beamPen);
            p.drawLine(QPointF(mapX(x0), mapY(y0)), QPointF(mapX(x1), mapY(y1)));
            }

            // MFC DrawBView：C扫取点区域线，有参数时始终显示
            if (m_params) {
                const int x1 = m_params->img.imgLineX1;
                const int x2 = m_params->img.imgLineX2;
                const int y1 = m_params->img.imgLineY1;
                const int y2 = m_params->img.imgLineY2;

                QPen greenX(QColor(0, 230, 80), 1.0);   // 竖线：亮绿
                QPen greenY(QColor(0, 200, 60), 1.0);   // 横线：深绿 (MFC GREEN2)
                auto mapX = [&target](double ix) { return target.left() + ix * target.width() / double(CScanWidth); };
                auto mapY = [&target](double iy) { return target.top()  + iy * target.height() / double(WaveSampleCount); };

                p.setPen(greenX);
                p.drawLine(QPointF(mapX(x1), mapY(0)),   QPointF(mapX(x1), mapY(399)));
                p.drawLine(QPointF(mapX(x2), mapY(0)),   QPointF(mapX(x2), mapY(399)));

                p.setPen(greenY);
                p.drawLine(QPointF(mapX(0),   mapY(y1)), QPointF(mapX(511), mapY(y1)));
                p.drawLine(QPointF(mapX(0),   mapY(y2)), QPointF(mapX(511), mapY(y2)));
            }
        }
    }

    QFont axisFont(ThemeFont::Ui, 8);
    p.setFont(axisFont);
    const QFontMetrics afm(axisFont);
    p.setPen(QColor(210, 222, 232));

    const float displayRange = m_params ? m_params->tx.range : m_scan.range;

    // MFC B_RulerLeft：5组主刻度数值、50组小刻度，量程数值保留1位小数。
    for (int i = 0; i < 50; ++i) {
        const int y = plot.top() + qRound(double(i) * plot.height() / 50.0);
        const bool major = (i % 10) == 0;
        p.drawLine(plot.left() - (major ? 10 : 5), y, plot.left(), y);
    }
    for (int i = 0; i < 5; ++i) {
        const double y = plot.top() + double(i) * plot.height() / 5.0;
        const QString text = QString::number(displayRange * i / 5.0f, 'f', 1);
        p.drawText(QPointF(plot.left() - 13 - afm.horizontalAdvance(text),
                           y + afm.ascent() / 2.0), text);
    }
    p.save();
    const QString yUnit = QStringLiteral("mm");
    p.translate(9, plot.bottom() - afm.horizontalAdvance(yUnit));
    p.rotate(-90);
    p.drawText(0, 0, yUnit);
    p.restore();

    // MFC B_RulerTop：扫描类型、10组主刻度、50组小刻度和5组物理坐标。
    const int scanType = m_params ? m_params->scan.scanType : m_scan.scanType;
    const QString scanName = scanType == 0 ? QStringLiteral("S")
                           : scanType == 1 ? QStringLiteral("L")
                                           : QStringLiteral("CL");
    p.drawText(plot.left() - afm.horizontalAdvance(scanName) - 8,
               plot.top() - 14, scanName);
    for (int i = 0; i < 50; ++i) {
        const int x = plot.left() + qRound(double(i) * plot.width() / 50.0);
        const bool major = (i % 5) == 0;
        p.drawLine(x, plot.top() - (major ? 10 : 5), x, plot.top());
    }
    for (int i = 0; i < 5; ++i) {
        const double x = plot.left() + double(i) * plot.width() / 5.0;
        const double value = m_imgSpanStart
                           + (m_imgSpanEnd - m_imgSpanStart) * i / 5.0;
        const QString text = QString::number(value, 'f', 1);
        p.drawText(QPointF(x, plot.top() - 14), text);
    }
    const QString xUnit = scanType < 2 ? QStringLiteral("mm") : QString::fromUtf8("°");
    p.drawText(plot.right() - afm.horizontalAdvance(xUnit), plot.top() - 14, xUnit);

    const int scaleGap = qMin(14, qMax(4, mr / 3));
    const int scaleW = qMin(11, qMax(6, mr / 4));
    const QRect scale(plot.right() + scaleGap, plot.top(), scaleW, plot.height());
    const int barH = scale.height();
    for (int y = 0; y < barH; ++y) {
        const int val = 255 - (y * 256 / barH);
        const int c = qBound(0, val, 255);
        p.fillRect(scale.left(), scale.top() + y, scaleW, 1, QColor(m_colorLUT[c]));
    }
    p.setPen(QColor(185, 205, 218));
    p.drawRect(scale);

    if (m_frozen) {
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(plot.topLeft(), plot.bottomRight());
        p.drawLine(plot.topRight(), plot.bottomLeft());
    }

    if (!m_hasData) {
        p.setPen(QColor(100, 140, 160));
        QFont hintFont(ThemeFont::Ui, 14);
        p.setFont(hintFont);
        p.drawText(plot, Qt::AlignCenter, QString::fromUtf8("等待数据..."));
    }
}

QRect BScanWidget::imageRect() const
{
    const int ml = qMin(62, qMax(48, width() / 9));
    const int mt = qMin(38, qMax(30, height() / 10));
    const int mr = qMin(50, qMax(22, width() / 8));
    const int mb = qMin(18, qMax(8, height() / 20));
    QRect plot = rect().adjusted(ml, mt, -mr, -mb);
    if (plot.width() <= 0 || plot.height() <= 0) return {};

    constexpr double imageAspect = double(CScanWidth) / WaveSampleCount;
    const int aspectWidth = qRound(plot.height() * imageAspect);
    if (aspectWidth < plot.width()) {
        plot.setLeft(plot.left() + (plot.width() - aspectWidth) / 2);
        plot.setWidth(aspectWidth);
    } else {
        const int aspectHeight = qRound(plot.width() / imageAspect);
        if (aspectHeight < plot.height())
            plot.setHeight(aspectHeight);
    }

    const QSize fitted = QSize(CScanWidth, WaveSampleCount)
        .scaled(plot.size(), Qt::KeepAspectRatio);
    return QRect(
        plot.left() + (plot.width()  - fitted.width())  / 2,
        plot.top()  + (plot.height() - fitted.height()) / 2,
        fitted.width(), fitted.height());
}

void BScanWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_frozen || m_ruleBeamCount < 1) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QRect imgRect = imageRect();
    if (!imgRect.isValid() || !imgRect.contains(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    // 点击 X → 图像 X 坐标 → 找最近声束
    const double imgX = (event->pos().x() - imgRect.left())
        * double(CScanWidth) / imgRect.width();
    int bestBeam = 0;
    double bestDist = std::abs(m_rules[0].x - imgX);
    for (int b = 1; b < m_ruleBeamCount; ++b) {
        const double dist = std::abs(m_rules[b].x - imgX);
        if (dist < bestDist) {
            bestDist = dist;
            bestBeam = b;
        }
    }

    m_selectedBeam = bestBeam;
    update();
    emit beamSelected(bestBeam);
}
