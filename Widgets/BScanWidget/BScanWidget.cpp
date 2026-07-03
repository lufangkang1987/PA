#include "BScanWidget.h"
#include <QPainter>
#include <QtMath>

BScanWidget::BScanWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(220);
}

static QColor heatColor(double v)
{
    v = qBound(0.0, v, 1.0);
    if (v < 0.25)
        return QColor(0, 0, int(90 + v * 420));
    if (v < 0.5)
        return QColor(0, int((v - 0.25) * 900), 255);
    if (v < 0.75)
        return QColor(int((v - 0.5) * 900), 255, int(255 - (v - 0.5) * 900));
    return QColor(255, int(255 - (v - 0.75) * 760), 0);
}

void BScanWidget::rebuildCache(const QSize &size)
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
            double v = 0.05 + 0.05 * qSin(nx * 42.0 + ny * 18.0);
            v += qExp(-qPow((ny - 0.12) / 0.025, 2.0)) * (0.35 + 0.55 * qAbs(qSin(nx * 18.0)));
            v += qExp(-qPow((ny - 0.24) / 0.018, 2.0)) * qExp(-qPow((nx - 0.36) / 0.06, 2.0)) * 0.8;
            v += qExp(-qPow((ny - 0.47) / 0.020, 2.0)) * qExp(-qPow((nx - 0.78) / 0.05, 2.0)) * 0.7;
            line[x] = heatColor(v).rgb();
        }
    }
}

void BScanWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(7, 17, 27));

    const QRect plot = rect().adjusted(46, 14, -50, -30);
    if (plot.size() != m_cache.size())
        rebuildCache(plot.size());

    p.drawImage(plot.topLeft(), m_cache);

    p.setPen(QColor(210, 222, 232));
    p.setFont(QFont("Microsoft YaHei", 8));
    p.drawText(8, plot.top() + 4, "0");
    p.drawText(4, plot.bottom() + 2, "200");
    p.drawText(plot.center().x() - 28, height() - 8, "位置(mm)");
    p.save();
    p.translate(16, plot.center().y() + 30);
    p.rotate(-90);
    p.drawText(0, 0, "深度(mm)");
    p.restore();

    const QRect scale(plot.right() + 14, plot.top(), 11, plot.height());
    QLinearGradient grad(scale.bottomLeft(), scale.topLeft());
    grad.setColorAt(0.0, QColor(0, 0, 120));
    grad.setColorAt(0.25, QColor(0, 220, 255));
    grad.setColorAt(0.5, QColor(0, 255, 80));
    grad.setColorAt(0.75, QColor(255, 230, 0));
    grad.setColorAt(1.0, QColor(255, 0, 0));
    p.fillRect(scale, grad);
    p.setPen(QColor(185, 205, 218));
    p.drawRect(scale);
}
