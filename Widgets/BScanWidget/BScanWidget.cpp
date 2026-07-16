#include "BScanWidget.h"

#include "ColorMap.h"
#include "PAParams.h"
#include "Theme.h"

#include <QPainter>
#include <QtMath>
#include <algorithm>

BScanWidget::BScanWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setMinimumWidth(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_sImage.resize(SImageSize, 0xFF);
    buildColorLUT();
    m_displayImage = buildDisplayImage();
}

void BScanWidget::setParamsSource(const PAParams *params)
{
    m_params = params;
    computeScanRules(m_params ? m_params->global.beamCount : m_scan.beamCount);
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
        return;
    }

    ::computeScanRules(*scan, *probe, range, count, nullptr, nullptr, m_rules);
}

void BScanWidget::softwareImaging(
    const std::vector<std::array<uint8_t, WaveSampleCount>> &waveforms,
    int beamCount, uint8_t *img)
{
    const int count = qBound(0, beamCount, MaxBeams);
    const uint8_t *waveP[MaxBeams] = {};
    for (int b = 0; b < count; ++b)
        waveP[b] = waveforms[b].data();
    ::softwareImaging(waveP, m_rules, count, img);
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
    const int effectiveBeamCount = qMin(beamCount, MaxBeams);

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

    const int ml = qMin(46, qMax(18, width() / 9));
    const int mt = qMin(14, qMax(6,  height() / 24));
    const int mr = qMin(50, qMax(22, width() / 8));
    const int mb = qMin(30, qMax(14, height() / 12));
    const QRect plot = rect().adjusted(ml, mt, -mr, -mb);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    if (!m_displayImage.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawImage(plot, m_displayImage);
    }

    QFont axisFont(ThemeFont::Ui, 8);
    p.setFont(axisFont);
    const QFontMetrics afm(axisFont);
    p.setPen(QColor(210, 222, 232));

    const float displayRange = m_params ? m_params->tx.range : m_scan.range;
    const QString yTopStr = "0";
    const QString yBotStr = QString::number(static_cast<int>(displayRange), 'f', 0);
    p.drawText(qMax(0, ml - afm.horizontalAdvance(yTopStr) - 2),
               plot.top() + afm.ascent(), yTopStr);
    p.drawText(qMax(0, ml - afm.horizontalAdvance(yBotStr) - 2),
               plot.bottom(), yBotStr);

    const QString xLabel = QString::fromUtf8("位置(mm)");
    const int xLabelW = afm.horizontalAdvance(xLabel);
    p.drawText(qBound(plot.left(), plot.center().x() - xLabelW / 2,
                      plot.right() - xLabelW),
               height() - 4, xLabel);

    const QString yLabel = QString::fromUtf8("深度(mm)");
    const int yLabelW = afm.horizontalAdvance(yLabel);
    p.save();
    const int yLabelX = qMax(2, (ml - afm.height()) / 2 + 2);
    p.translate(yLabelX, plot.center().y() + yLabelW / 2);
    p.rotate(-90);
    p.drawText(0, 0, yLabel);
    p.restore();

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
