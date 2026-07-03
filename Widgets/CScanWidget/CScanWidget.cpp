#include "CScanWidget.h"
#include <QPainter>
#include <QtMath>

CScanWidget::CScanWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(220);
}

void CScanWidget::rebuildCache(const QSize &size)
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

void CScanWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(7, 17, 27));

    const QRect plot = rect().adjusted(50, 14, -42, -30);
    if (plot.size() != m_cache.size())
        rebuildCache(plot.size());

    p.drawImage(plot.topLeft(), m_cache);

    p.setPen(QColor(18, 42, 60, 120));
    for (int x = plot.left(); x <= plot.right(); x += 42)
        p.drawLine(x, plot.top(), x, plot.bottom());
    for (int y = plot.top(); y <= plot.bottom(); y += 42)
        p.drawLine(plot.left(), y, plot.right(), y);

    p.setPen(QColor(210, 222, 232));
    p.setFont(QFont("Microsoft YaHei", 8));
    p.drawText(14, plot.top() + 4, "0");
    p.drawText(6, plot.bottom() + 2, "400");
    p.drawText(plot.center().x() - 20, height() - 8, "X(mm)");
    p.save();
    p.translate(17, plot.center().y() + 18);
    p.rotate(-90);
    p.drawText(0, 0, "Y(mm)");
    p.restore();

    const QRect scale(plot.right() + 13, plot.top() + 20, 12, qMax(20, plot.height() - 40));
    QLinearGradient grad(scale.topLeft(), scale.bottomLeft());
    grad.setColorAt(0.0, QColor(245, 247, 249));
    grad.setColorAt(1.0, QColor(8, 8, 8));
    p.fillRect(scale, grad);
    p.setPen(QColor(175, 194, 206));
    p.drawRect(scale);
}
