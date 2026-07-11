#include "CScanWidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QtMath>

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
    rebuildFromData();
    update();
}

void CScanWidget::clearData()
{
    m_data.clear();
    m_dataWidth  = 0;
    m_dataHeight = 0;
    m_replay     = false;
    m_cache = QImage();  // 强制重绘 mock
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
            float v = qBound(0.0f, m_data[y * m_dataWidth + x], 1.0f);
            int r = int(24  + v * 221);
            int g = int(30  + v * 217);
            int b = int(36  + v * 213);
            line[x] = QColor(r, g, b).rgb();
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
    p.fillRect(rect(), QColor(7, 17, 27));

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
        p.drawImage(plot, m_cache);
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
    QFont axisFont("Microsoft YaHei", 8);
    p.setFont(axisFont);
    const QFontMetrics afm(axisFont);
    p.setPen(QColor(210, 222, 232));

    // Y 轴刻度
    const QString yTopStr = hasRealData ? "0" : "0";
    const QString yBotStr = hasRealData ? QString::number(m_dataHeight) : "400";
    const int yTopW = afm.horizontalAdvance(yTopStr);
    const int yBotW = afm.horizontalAdvance(yBotStr);
    p.drawText(qMax(0, ml - yTopW - 2), plot.top() + afm.ascent(), yTopStr);
    p.drawText(qMax(0, ml - yBotW - 2), plot.bottom(), yBotStr);

    // X 轴标签
    const QString xLabel = hasRealData
        ? QString("X (%1 pts)").arg(m_dataWidth) : QString("X(mm)");
    const int xLabelW = afm.horizontalAdvance(xLabel);
    const int xLabelX = qBound(plot.left(), plot.center().x() - xLabelW / 2,
                               plot.right() - xLabelW);
    p.drawText(xLabelX, height() - 4, xLabel);

    // Y 轴标签 (旋转)
    const QString yLabel = hasRealData
        ? QString("Y (%1 pts)").arg(m_dataHeight) : QString("Y(mm)");
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
        QFont rf("Microsoft YaHei", 11); rf.setBold(true);
        p.setFont(rf);
        p.drawText(plot.right() - 70, plot.top() + 16, "REPLAY");
    }
}
