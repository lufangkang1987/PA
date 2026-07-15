#include "CScanWidget.h"
#include <QPainter>
#include "Theme.h"
#include "ColorMap.h"
#include <QFontMetrics>
#include <QtMath>
#include <QMouseEvent>


CScanWidget::CScanWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setMinimumWidth(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

// ═══════════════════════════════════════════════════════════
// 数据接口
// ═══════════════════════════════════════════════════════════

void CScanWidget::setData(const QVector<float> &data, int w, int h)
{
    m_data       = data;
    m_dataWidth  = w;
    m_dataHeight = h;
    m_pageStart = qBound(0, m_pageStart, qMax(0, m_dataHeight - PageLineCount));
    rebuildFromData();
    update();
}

void CScanWidget::clearData()
{
    m_data.clear();
    m_dataWidth  = 0;
    m_dataHeight = 0;
    m_replay     = false;
    m_pageStart  = 0;
    m_cache = QImage();  // 强制重绘 mock
    update();
}

void CScanWidget::setSelectedLine(int line)
{
    m_selectedLine = qBound(-1, line, m_dataHeight - 1);
    update();
}

void CScanWidget::setAnalysisRect(int line1, int line2, int column1, int column2)
{
    m_analysisLine1 = qMin(line1, line2);
    m_analysisLine2 = qMax(line1, line2);
    m_analysisColumn1 = qMin(column1, column2);
    m_analysisColumn2 = qMax(column1, column2);
    update();
}

void CScanWidget::setPhysicalScale(float scanStepMm, float imageStartMm, float imageEndMm)
{
    m_scanStepMm = qMax(0.0f, scanStepMm);
    m_imageStartMm = imageStartMm;
    m_imageEndMm = imageEndMm;
    update();
}

void CScanWidget::setPageStart(int line)
{
    m_pageStart = qBound(0, line, qMax(0, m_dataHeight - PageLineCount));
    update();
}

void CScanWidget::setImageColumnRange(int first, int last)
{
    m_imageColumnStart = qBound(0, qMin(first, last), 511);
    m_imageColumnEnd = qBound(m_imageColumnStart + 1, qMax(first, last), 512);
    update();
}

// ═══════════════════════════════════════════════════════════
// 图像重建
// ═══════════════════════════════════════════════════════════

void CScanWidget::rebuildFromData()
{
    if (m_dataWidth <= 0 || m_dataHeight <= 0 || m_data.size() < m_dataWidth * m_dataHeight) {
        rebuildMock(QSize(width(), height()));
        return;
    }

    m_cache = QImage(m_dataWidth, m_dataHeight, QImage::Format_RGB32);
    for (int y = 0; y < m_dataHeight; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(m_cache.scanLine(y));
        for (int x = 0; x < m_dataWidth; ++x) {
            // 振幅 0→暗灰(24,30,36), 1→亮白(245,247,249)
            const float v = qBound(0.0f, m_data[y * m_dataWidth + x], 1.0f);
            line[x] = amplitudeToColor(qRound(v * 255.0f));
        }
    }
}

void CScanWidget::rebuildMock(const QSize &size)
{
    if (size.width() <= 0 || size.height() <= 0) {
        m_cache = QImage();
        return;
    }

    m_cache = QImage(size, QImage::Format_RGB32);
    for (int y = 0; y < size.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(m_cache.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            const double nx = double(x) / qMax(1, size.width());
            const double ny = double(y) / qMax(1, size.height());
            int base = int(180 + 32 * qSin(nx * 46.0) + 18 * qSin(ny * 74.0));
            double flaw = qExp(-qPow((nx - 0.46) / 0.08, 2.0) - qPow((ny - 0.48) / 0.12, 2.0));
            flaw += 0.5 * qExp(-qPow((nx - 0.32) / 0.045, 2.0) - qPow((ny - 0.34) / 0.055, 2.0));
            int shade = qBound(24, base - int(flaw * 145.0), 235);
            line[x] = QColor(shade, shade + 6, shade + 12).rgb();
        }
    }
}

// ═══════════════════════════════════════════════════════════
// 绘制
// ═══════════════════════════════════════════════════════════

void CScanWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), ThemeColor::DeepBg);

    // 自适应边距
    const int ml = qMin(50, qMax(20, width() / 8));
    const int mt = qMin(14, qMax(6,  height() / 24));
    const int mr = qMin(42, qMax(18, width() / 9));
    const int mb = qMin(30, qMax(14, height() / 12));
    const QRect plot = rect().adjusted(ml, mt, -mr, -mb);

    // 重建缓存（尺寸变化时）
    const bool hasRealData = hasData();
    if (hasRealData) {
        if (m_cache.size() != QSize(m_dataWidth, m_dataHeight))
            rebuildFromData();
    } else {
        if (plot.size() != m_cache.size())
            rebuildMock(plot.size());
    }

    // 绘制图像：真实数据拉伸到 plot 区域，mock 数据 1:1
    if (hasRealData) {
        const int visibleLines = qMin(PageLineCount, m_dataHeight - m_pageStart);
        const int visibleColumns = qMax(1, m_imageColumnEnd - m_imageColumnStart);
        p.drawImage(plot, m_cache,
                    QRect(m_imageColumnStart, m_pageStart,
                          visibleColumns, qMax(1, visibleLines)));
    } else {
        p.drawImage(plot.topLeft(), m_cache);
    }

    // 网格
    p.setPen(QColor(18, 42, 60, 120));
    const int gridStep = qMax(20, qMin(42, plot.width() / 10));
    for (int x = plot.left(); x <= plot.right(); x += gridStep)
        p.drawLine(x, plot.top(), x, plot.bottom());
    for (int y = plot.top(); y <= plot.bottom(); y += gridStep)
        p.drawLine(plot.left(), y, plot.right(), y);

    // 轴标签
    QFont axisFont(ThemeFont::Ui, 8);
    p.setFont(axisFont);
    const QFontMetrics afm(axisFont);
    p.setPen(QColor(210, 222, 232));

    // Y 轴刻度
    const int visibleLines = hasRealData ? qMin(PageLineCount, m_dataHeight - m_pageStart) : 0;
    const QString yTopStr = hasRealData
        ? QString::number(m_pageStart * m_scanStepMm, 'f', 1) : "0";
    const QString yBotStr = hasRealData
        ? QString::number((m_pageStart + visibleLines) * m_scanStepMm, 'f', 1) : "400";
    const int yTopW = afm.horizontalAdvance(yTopStr);
    const int yBotW = afm.horizontalAdvance(yBotStr);
    p.drawText(qMax(0, ml - yTopW - 2), plot.top() + afm.ascent(), yTopStr);
    p.drawText(qMax(0, ml - yBotW - 2), plot.bottom(), yBotStr);

    // X 轴标签
    const QString xLabel = hasRealData
        ? QString("X %1-%2 mm").arg(m_imageStartMm, 0, 'f', 1).arg(m_imageEndMm, 0, 'f', 1) : QString("X(mm)");
    const int xLabelW = afm.horizontalAdvance(xLabel);
    const int xLabelX = qBound(plot.left(), plot.center().x() - xLabelW / 2,
                               plot.right() - xLabelW);
    p.drawText(xLabelX, height() - 4, xLabel);

    // Y 轴标签 (旋转)
    const QString yLabel = hasRealData
        ? QString("Y (mm)") : QString("Y(mm)");
    const int yLabelW = afm.horizontalAdvance(yLabel);
    p.save();
    const int yLabelX = qMax(2, (ml - afm.height()) / 2 + 2);
    p.translate(yLabelX, plot.center().y() + yLabelW / 2);
    p.rotate(-90);
    p.drawText(0, 0, yLabel);
    p.restore();

    // 色阶条
    const int scaleGap = qMin(13, qMax(4, mr / 3));
    const int scaleW   = qMin(12, qMax(6, mr / 4));
    const QRect scale(plot.right() + scaleGap, plot.top() + 20,
                      scaleW, qMax(20, plot.height() - 40));
    QLinearGradient grad(scale.topLeft(), scale.bottomLeft());
    grad.setColorAt(0.0, QColor(245, 247, 249));
    grad.setColorAt(1.0, QColor(8, 8, 8));
    p.fillRect(scale, grad);
    p.setPen(QColor(175, 194, 206));
    p.drawRect(scale);

    // 回放标记
    if (m_replay) {
        p.setPen(QColor(255, 140, 0, 200));
        QFont rf(ThemeFont::Ui, 11); rf.setBold(true);
        p.setFont(rf);
        p.drawText(plot.right() - 70, plot.top() + 16, "REPLAY");
    }

    if (hasRealData && m_selectedLine >= 0) {
        if (m_selectedLine >= m_pageStart && m_selectedLine < m_pageStart + visibleLines) {
            const int y = plot.top() + qRound((m_selectedLine - m_pageStart + 0.5)
                                              * plot.height() / qMax(1, visibleLines));
            p.setPen(QPen(QColor(255, 190, 0), 1));
            p.drawLine(plot.left(), y, plot.right(), y);
        }
    }

    if (hasRealData) {
        const int visibleColumns = qMax(1, m_imageColumnEnd - m_imageColumnStart);
        const int left = plot.left() + qRound(m_analysisColumn1 * plot.width() / float(visibleColumns));
        const int right = plot.left() + qRound((m_analysisColumn2 + 1) * plot.width() / float(visibleColumns));
        const int top = plot.top() + qRound(m_analysisLine1 * plot.height() / float(qMax(1, visibleLines)));
        const int bottom = plot.top() + qRound((m_analysisLine2 + 1) * plot.height() / float(qMax(1, visibleLines)));
        p.setPen(QPen(QColor(0, 255, 80), 1));
        p.drawRect(QRect(QPoint(left, top), QPoint(right, bottom)).normalized());
    }
}

void CScanWidget::mousePressEvent(QMouseEvent *event)
{
    if (!hasData()) return;
    const int ml = qMin(50, qMax(20, width() / 8));
    const int mt = qMin(14, qMax(6, height() / 24));
    const int mr = qMin(42, qMax(18, width() / 9));
    const int mb = qMin(30, qMax(14, height() / 12));
    const QRect plot = rect().adjusted(ml, mt, -mr, -mb);
    if (!plot.contains(event->position().toPoint())) return;

    const int visibleLines = qMin(PageLineCount, m_dataHeight - m_pageStart);
    const int localLine = qBound(0, int((event->position().y() - plot.top())
        * visibleLines / qMax(1, plot.height())), qMax(0, visibleLines - 1));
    const int line = m_pageStart + localLine;
    const int visibleColumns = qMax(1, m_imageColumnEnd - m_imageColumnStart);
    const int localColumn = qBound(0, int((event->position().x() - plot.left())
        * visibleColumns / qMax(1, plot.width())), visibleColumns - 1);
    const int column = m_imageColumnStart + localColumn;
    setSelectedLine(line);
    emit positionSelected(line, column);

    const int lineSpan = qMax(0, m_analysisLine2 - m_analysisLine1);
    const int columnSpan = qMax(0, m_analysisColumn2 - m_analysisColumn1);
    m_analysisLine1 = qBound(0, localLine - lineSpan / 2, qMax(0, visibleLines - lineSpan - 1));
    m_analysisLine2 = m_analysisLine1 + lineSpan;
    m_analysisColumn1 = qBound(0, localColumn - columnSpan / 2,
                               qMax(0, visibleColumns - columnSpan - 1));
    m_analysisColumn2 = m_analysisColumn1 + columnSpan;

    float maximum = 0.0f;
    double sum = 0.0;
    int count = 0;
    int maxLine = m_analysisLine1;
    int maxColumn = m_analysisColumn1;
    for (int localY = m_analysisLine1; localY <= m_analysisLine2; ++localY) {
        const int y = m_pageStart + localY;
        for (int localX = m_analysisColumn1; localX <= m_analysisColumn2; ++localX) {
            const int x = m_imageColumnStart + localX;
            const float value = m_data[y * m_dataWidth + x];
            sum += value;
            ++count;
            if (value > maximum) {
                maximum = value;
                maxLine = y;
                maxColumn = localX;
            }
        }
    }
    emit analysisRectChanged(m_analysisLine1, m_analysisLine2,
                             m_analysisColumn1, m_analysisColumn2);
    emit analysisMeasured(maximum, count ? float(sum / count) : 0.0f,
                          maxLine, maxColumn);
    update();
}
