#include "AScanWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

AScanWidget::AScanWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(260);
    // 初始 mock 数据（RF 模式，双极性，400 点）
    m_data.resize(kMaxSamples);
    for (int i = 0; i < m_data.size(); ++i) {
        const double echo1 = qExp(-qPow((i - 30)  / 16.0, 2.0)) * qSin(i * 0.8)  * 0.85;
        const double echo2 = qExp(-qPow((i - 180) / 10.0, 2.0)) * qSin(i * 0.95) * 0.6;
        const double echo3 = qExp(-qPow((i - 260) / 8.0,  2.0)) * qSin(i * 1.1)  * 0.35;
        const double noise  = qSin(i * 0.23) * 0.02;
        m_data[i] = echo1 + echo2 + echo3 + noise;
    }
    m_fpsTimer.start();
}

void AScanWidget::setAcousticParams(float velocity, int sampleRate, float range)
{
    m_velocity   = velocity;
    m_sampleRate = sampleRate;
    m_userRange  = range;
}

void AScanWidget::setWaveform(const QVector<double> &data,
                               int beamIndex, int frameIndex, int rectifyMode)
{
    m_data        = data;
    m_beamIndex   = beamIndex;
    m_frameIndex  = frameIndex;
    m_rectifyMode = rectifyMode;
    m_isLive      = true;

    // FPS 统计
    ++m_frameCount;
    if (m_fpsTimer.elapsed() >= 1000) {
        m_currentFps = m_frameCount * 1000.0 / m_fpsTimer.elapsed();
        m_frameCount = 0;
        m_fpsTimer.restart();
    }

    update();
}

// ─── 辅助：采样点 → 长度(mm) ───
// 公式: depth = index × velocity(m/s) / (2 × sampleRate(Hz)) × 1000
static double sampleToMm(int i, int total, float velocity, int sampleRateMHz, float userRange)
{
    if (userRange > 0.0f) {
        // 用户指定声程：按比例等距映射
        return userRange * double(i) / double(total - 1);
    }
    // 自动：根据声速和采样率计算真实物理长度
    const double sampleRateHz = sampleRateMHz * 1e6;
    return double(i) * velocity / (2.0 * sampleRateHz) * 1000.0;
}

static double totalRangeMm(int total, float velocity, int sampleRateMHz, float userRange)
{
    if (userRange > 0.0f)
        return userRange;
    return sampleToMm(total - 1, total, velocity, sampleRateMHz, userRange);
}

void AScanWidget::paintEvent(QPaintEvent *)
{
    const bool isRF = (m_rectifyMode == 3 || m_rectifyMode < 0);
    const int  N    = m_data.size();
    const double totalMm = totalRangeMm(N, m_velocity, m_sampleRate, m_userRange);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(7, 17, 27));

    // ── 绘图区 ──
    const int marginLeft  = 54;
    const int marginRight = 10;
    const int marginTop   = 12;
    const int marginBot   = 40;
    const QRect plot(marginLeft, marginTop,
                     width() - marginLeft - marginRight,
                     height() - marginTop - marginBot);
    p.fillRect(plot, QColor(6, 20, 32));

    // ── 网格 ──
    p.setPen(QPen(QColor(30, 58, 82, 140), 1));
    for (int i = 0; i <= 10; ++i) {
        const int x = plot.left() + i * plot.width() / 10;
        p.drawLine(x, plot.top(), x, plot.bottom());
    }
    const int midY = plot.center().y();
    for (int i = 0; i <= 8; ++i) {
        const int y = plot.top() + i * plot.height() / 8;
        p.drawLine(plot.left(), y, plot.right(), y);
    }
    if (isRF) {
        p.setPen(QPen(QColor(50, 100, 140, 80), 1, Qt::DashLine));
        p.drawLine(plot.left(), midY, plot.right(), midY);
    }

    // ── X 轴长度标尺 ──
    p.setPen(QColor(180, 195, 210));
    p.setFont(QFont("Microsoft YaHei", 9));
    {
        const int xTickCount = (totalMm < 20.0) ? 5 : 10;
        const double xStep   = totalMm / xTickCount;
        for (int i = 0; i <= xTickCount; ++i) {
            const double mm = i * xStep;
            const int x = plot.left() + int(double(i) / xTickCount * plot.width());
            // 刻度线
            p.drawLine(x, plot.bottom(), x, plot.bottom() + 4);
            // 刻度数字
            QString label;
            if (totalMm < 5.0)
                label = QString::number(mm, 'f', 2);
            else if (totalMm < 100.0)
                label = QString::number(mm, 'f', 1);
            else
                label = QString::number(int(mm + 0.5));
            const int textW = p.fontMetrics().horizontalAdvance(label);
            p.drawText(x - textW / 2, plot.bottom() + 16, label);
        }
    }
    // X 轴标签
    p.setFont(QFont("Microsoft YaHei", 9));
    p.drawText(plot.center().x() - 28, height() - 6,
               QString::fromUtf8("\u957F\u5EA6(mm)"));   // "长度(mm)"

    // ── Y 轴标尺 ──
    p.setPen(QColor(180, 195, 210));
    p.setFont(QFont("Microsoft YaHei", 9));
    if (isRF) {
        for (int i = 0; i <= 4; ++i) {
            const int pct = 100 - i * 50;
            const int y = plot.top() + i * plot.height() / 4;
            p.drawText(4, y + 4, QString::number(pct));
        }
    } else {
        for (int i = 0; i <= 4; ++i) {
            const int pct = 100 - i * 25;
            const int y = plot.top() + i * plot.height() / 4;
            p.drawText(12, y + 4, QString::number(pct));
        }
    }

    // ── 波形绘制 ──
    if (N >= 2) {
        QPainterPath path;
        const double amp = plot.height() * 0.44;

        if (isRF) {
            path.moveTo(plot.left(),
                        midY - qBound(-1.0, m_data[0], 1.0) * amp);
            for (int i = 1; i < N; ++i) {
                const double x = plot.left() +
                    double(i) / double(N - 1) * plot.width();
                const double y = midY - qBound(-1.0, m_data[i], 1.0) * amp;
                path.lineTo(x, y);
            }
        } else {
            const double baseY = plot.bottom() - 2;
            path.moveTo(plot.left(),
                        baseY - qBound(0.0, m_data[0], 1.0) * plot.height() * 0.92);
            for (int i = 1; i < N; ++i) {
                const double x = plot.left() +
                    double(i) / double(N - 1) * plot.width();
                const double y = baseY - qBound(0.0, m_data[i], 1.0) * plot.height() * 0.92;
                path.lineTo(x, y);
            }
        }

        QColor waveColor = m_isLive ? QColor(0, 255, 42) : QColor(80, 190, 100);
        p.setPen(QPen(waveColor, 1.2));
        p.drawPath(path);
    }

    // ── HUD 叠加信息 ──
    p.setFont(QFont("Consolas", 9));
    const int hudX = plot.right() - 10;

    static const char *modeNames[] = { "QW", "ZW", "FW", "RF" };
    const char *modeName = (m_rectifyMode >= 0 && m_rectifyMode <= 3)
                           ? modeNames[m_rectifyMode] : "??";

    p.setPen(m_isLive ? QColor(0, 220, 50, 220) : QColor(80, 100, 80, 160));
    p.drawText(hudX - 260, plot.top() + 14, 260, 16,
               Qt::AlignRight,
               QString("Beam %1  Frame %2  %3  %.1fmm  %4 FPS")
                   .arg(m_beamIndex)
                   .arg(m_frameIndex)
                   .arg(QString::fromLatin1(modeName))
                   .arg(totalMm, 0, 'f', 1)
                   .arg(m_currentFps, 0, 'f', 1));

    // ── LIVE / 模拟 状态指示 ──
    const QPointF dotPos(plot.left() + 6, plot.top() + 6);
    p.setPen(Qt::NoPen);
    p.setBrush(m_isLive ? QColor(0, 255, 42, 220) : QColor(120, 140, 120, 120));
    p.drawEllipse(dotPos, 4, 4);
    p.setPen(m_isLive ? QColor(0, 200, 40, 200) : QColor(120, 140, 120, 110));
    p.setFont(QFont("Microsoft YaHei", 8));
    p.drawText(plot.left() + 16, plot.top() + 12,
               m_isLive ? QString::fromUtf8("LIVE") : QString::fromUtf8("SIM"));
}
