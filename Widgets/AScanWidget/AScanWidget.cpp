#include "AScanWidget.h"
#include "DataTypes.h"
#include "Theme.h"

#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>
#include <QPainter>

AScanWidget::AScanWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(220);
    setMinimumWidth(280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // 不填充 mock 数据：无真实波形时绘图区仅显示网格和坐标轴
    m_data.resize(kMaxSamples);
    m_fpsTimer.start();
}

void AScanWidget::setAcousticParams(float velocity, int sampleRate, float range)
{
    m_velocity   = velocity;
    m_sampleRate = sampleRate;
    m_userRange  = range;
    update();
}

void AScanWidget::setWaveform(const QVector<double> &data,
                               int beamIndex, int frameIndex, int rectifyMode)
{
    if (m_frozen)
        return;

    // 波形限幅 (同MFC: if waveP[i] > 250 → 250)
    m_data = data;
    for (auto &v : m_data)
        v = qBound(0.0, v, 1.0);

    m_beamIndex   = beamIndex;
    m_frameIndex  = frameIndex;
    m_rectifyMode = rectifyMode;
    m_isLive      = true;

    ++m_frameCount;
    if (m_fpsTimer.elapsed() >= 1000) {
        m_currentFps = m_frameCount * 1000.0 / m_fpsTimer.elapsed();
        m_frameCount = 0;
        m_fpsTimer.restart();
    }

    update();
}

void AScanWidget::setLive(bool live)
{
    m_frozen = !live;
    update();
}

void AScanWidget::setGate(int gate, bool enabled, float start, float width,
                           float threshold, const QColor &color)
{
    if (gate < 0 || gate > 2) return;
    m_gates[gate].enabled   = enabled;
    m_gates[gate].start     = start;
    m_gates[gate].width     = width;
    m_gates[gate].threshold = threshold;
    m_gates[gate].color     = color;
    update();
}

void AScanWidget::setCalibrationGuide(bool visible, int targetPercent)
{
    m_calibrationGuide = visible;
    m_calibrationTarget = targetPercent == 50 ? 50 : 80;
    update();
}

// ═══════════════════════════════════════════════════════════
// 辅助：绘图区 / 总声程
// ═══════════════════════════════════════════════════════════

QRectF AScanWidget::plotRect() const
{
    const int marginLeft  = 50;
    const int marginRight = 10;
    const int marginTop   = 8;
    const int marginBot   = 36;
    return QRectF(marginLeft, marginTop,
                  qMax(40, width() - marginLeft - marginRight),
                  qMax(30, height() - marginTop - marginBot));
}

double AScanWidget::totalRangeMm() const
{
    if (m_userRange > 0.0f)
        return m_userRange;
    const int N = qMax(1, m_data.size());
    return double(N - 1) * m_velocity / (2.0 * m_sampleRate * 1e6) * 1000.0;
}

// ═══════════════════════════════════════════════════════════
// MFC 风格 A 扫绘制
// 坐标系：X=幅度(0~100%)，Y=深度(从上到下，0~Range mm)
// ═══════════════════════════════════════════════════════════

void AScanWidget::paintEvent(QPaintEvent *)
{
    const int N = m_data.size();
    const double totalMm = totalRangeMm();
    const QRectF plot = plotRect();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), ThemeColor::DeepBg);

    // 黑色背景
    p.fillRect(plot, QColor(0, 0, 0));

    // ── 网格 (5竖线×8横线) ──
    p.setPen(QPen(QColor(40, 40, 40), 1));
    for (int i = 1; i < 5; ++i) {   // 竖线：幅度 25/50/75%
        const int x = plot.left() + i * plot.width() / 5;
        p.drawLine(x, plot.top(), x, plot.bottom());
    }
    for (int i = 1; i < 10; ++i) {   // 与 MFC 一致：声程方向 10 等分
        const int y = plot.top() + i * plot.height() / 10;
        p.drawLine(plot.left(), y, plot.right(), y);
    }

    if (m_calibrationGuide) {
        p.setPen(QPen(QColor(0, 0, 255), 1));
        for (const int percent : {m_calibrationTarget - 5, m_calibrationTarget + 5}) {
            const double x = plot.left() + percent * plot.width() / 100.0;
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
    }

    // ── X 轴标尺：幅度 % ──
    {
        p.setPen(QColor(160, 185, 200));
        QFont f(ThemeFont::Ui, 9);
        p.setFont(f);
        const QFontMetrics fm(f);
        const int textH = fm.ascent();
        for (int i = 0; i <= 5; ++i) {
            const int pct = i * 20;
            const double x = plot.left() + i * plot.width() / 5.0;
            p.drawLine(QPointF(x, plot.bottom()), QPointF(x, plot.bottom() + 4));
            const QString s = QString("%1%").arg(pct);
            const int textW = fm.horizontalAdvance(s);
            // 首尾标签靠边对齐避免溢出，其余居中
            double textX;
            if (i == 0)
                textX = plot.left();
            else if (i == 5)
                textX = plot.right() - textW;
            else
                textX = x - textW / 2.0;
            p.drawText(QPointF(textX, plot.bottom() + 5 + textH), s);
        }
    }

    // ── Y 轴标尺：深度 mm ──
    {
        p.setPen(QColor(160, 185, 200));
        QFont f(ThemeFont::Ui, 9);
        p.setFont(f);
        const QFontMetrics fm(f);
        const int ml = int(plot.left());
        const int textH = fm.ascent();
        for (int i = 0; i <= 5; ++i) {
            const double mm = totalMm * i / 5.0;
            const double y = plot.top() + i * plot.height() / 5.0;
            p.drawLine(QPointF(ml - 4, y), QPointF(ml, y));
            const QString s = QString::number(mm, 'f', 1);
            // 统一右对齐
            p.drawText(QPointF(ml - 6 - fm.horizontalAdvance(s),
                               y + textH / 3), s);
        }
        // 单位 "mm" 旋转标注在 Y 轴左侧
        p.save();
        const QString unit = "mm";
        const int unitW = fm.horizontalAdvance(unit);
        p.translate(qMax(4.0, ml - 10.0 - fm.horizontalAdvance(
            QString::number(totalMm, 'f', 1))),
                    plot.top() + plot.height() / 2.0);
        p.rotate(-90);
        p.drawText(QPointF(-unitW / 2, 0), unit);
        p.restore();
    }

    // ── 波形绘制 (X=幅度, Y=采样点序号) ──
    // 仅在收到真实波形数据后才绘制，无数据时只显示网格+坐标轴
    if (m_isLive && N >= 2) {
        QPainterPath path;
        const double ampW = plot.width();              // 100% 幅度 = 全宽
        // MFC 在 400 高逻辑画布上使用 y=i，最后一个采样点位于 399。
        const double yScale = double(plot.height()) / 400.0;

        auto clampA = [](double v) { return qBound(0.0, v, 1.0); };

        path.moveTo(plot.left() + clampA(m_data[0]) * ampW,
                    double(plot.top()));
        for (int i = 1; i < N; ++i) {
            path.lineTo(plot.left() + clampA(m_data[i]) * ampW,
                        plot.top() + double(i) * yScale);
        }

        p.setPen(QPen(QColor(0, 255, 42), 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    // ── 闸门绘制 (同MFC: 竖线, X=阈值; 回放时用replayGates) ──
    {
        const float rng = float(totalMm);
        for (int g = 0; g < 3; ++g) {
            const GateDef &gate = m_gates[g];
            if (!gate.enabled) continue;

            const double y1 = plot.top() + (gate.start / rng) * plot.height();
            const double y2 = plot.top() + ((gate.start + gate.width) / rng) * plot.height();
            const double thX = plot.left() + (gate.threshold / 100.0) * plot.width();

            // 竖线
            p.setPen(QPen(gate.color, 2));
            p.drawLine(QPointF(thX, y1), QPointF(thX, y2));
            // 起止横标
            p.drawLine(QPointF(thX - 5, y1), QPointF(thX + 5, y1));
            p.drawLine(QPointF(thX - 5, y2), QPointF(thX + 5, y2));
            // 标签
            static const char *lbl[] = {"A", "B", "C"};
            QFont gf(ThemeFont::Ui, 11); gf.setBold(true);
            p.setFont(gf);
            p.setPen(gate.color);
            p.drawText(QPointF(thX + 4, y1 + 14), QString::fromLatin1(lbl[g]));
        }
    }

    // ── HUD ──
    {
        QFont hud(ThemeFont::Mono, 8);
        p.setFont(hud);
        const QFontMetrics hm(hud);
        static const char *modes[] = {"QW", "ZW", "FW", "RF"};
        const char *mn = (m_rectifyMode >= 0 && m_rectifyMode <= 3) ? modes[m_rectifyMode] : "??";
        QString txt = QString("Beam %1  Frame %2  %3  %4mm  %5 FPS")
            .arg(m_beamIndex + 1).arg(m_frameIndex).arg(mn)
            .arg(totalMm, 0, 'f', 1).arg(m_currentFps, 0, 'f', 1);
        p.setPen(m_isLive ? QColor(0, 220, 50, 200) : QColor(80, 100, 80, 150));
        p.drawText(plot.right() - hm.horizontalAdvance(txt) - 4,
                   plot.top() + 12, txt);

        // 状态圆点
        const QPointF dot(plot.left() + 6, plot.top() + 6);
        p.setPen(Qt::NoPen);
        p.setBrush(m_alarm ? QColor(255, 50, 50, 220)
                 : m_frozen ? QColor(255, 180, 50, 220)
                 : m_isLive ? QColor(0, 255, 42, 220) : QColor(120, 140, 120, 120));
        p.drawEllipse(dot, 4, 4);
        p.setPen(m_alarm ? QColor(255, 80, 80, 200)
                 : m_frozen ? QColor(255, 200, 80, 200)
                 : m_isLive ? QColor(0, 200, 40, 200) : QColor(120, 140, 120, 110));
        p.setFont(QFont(ThemeFont::Ui, 8));
        p.drawText(plot.left() + 16, plot.top() + 12,
                   m_alarm ? "ALARM" :
                   m_frozen ? "FROZEN" : m_isLive ? "LIVE" : "SIM");
    }
}

// ═══════════════════════════════════════════════════════════
// 闸门拖拽（同 MFC OnLButtonDown: Y→起位, X→阈值）
// ═══════════════════════════════════════════════════════════

void AScanWidget::mousePressEvent(QMouseEvent *ev)
{
    const QRectF plot = plotRect();
    if (ev->button() == Qt::LeftButton && plot.contains(ev->position())) {
        // Ctrl+点击 → 切换声束
        if (ev->modifiers() & Qt::ControlModifier) {
            const int beamCount = MaxBeams;
            const int beam = int((ev->position().y() - plot.top()) / plot.height() * beamCount);
            emit beamChangeRequested(qBound(0, beam, beamCount - 1));
        } else {
            m_dragging = true;
            const float totalMm = float(totalRangeMm());
            const float px = float(ev->position().x());
            const float py = float(ev->position().y());

            // 检测是否点击在某个闸门附近（±8px），优先选最近的门
            int hitGate = -1;
            float bestDist = 12.0f;  // 命中阈值（像素）
            for (int g = 2; g >= 0; --g) {  // 倒序：C/B/A, C在最前面
                const GateDef &gate = m_gates[g];
                if (!gate.enabled) continue;
                const float thX = float(plot.left() + (gate.threshold / 100.0) * plot.width());
                const float gy1 = float(plot.top() + (gate.start / totalMm) * plot.height());
                const float gy2 = float(plot.top() + ((gate.start + gate.width) / totalMm) * plot.height());
                if (py >= gy1 - 4 && py <= gy2 + 4 && qAbs(px - thX) < bestDist) {
                    hitGate = g;
                    bestDist = qAbs(px - thX);
                }
            }

            if (hitGate >= 0)
                m_activeGate = hitGate;

            const int g = m_activeGate;
            GateDef &gate = m_gates[g];

            gate.start = qBound(0.0f, float(py - float(plot.top())) / float(plot.height()) * totalMm,
                                totalMm - gate.width);
            gate.threshold = qBound(0.0f, float(px - float(plot.left())) / float(plot.width()) * 100.0f, 99.0f);

            update();
            emit gateDragged(g, gate.start, gate.threshold);
        }
    }
    QWidget::mousePressEvent(ev);
}

void AScanWidget::mouseMoveEvent(QMouseEvent *ev)
{
    if (!m_dragging) return;
    const QRectF plot = plotRect();
    const double totalMm = totalRangeMm();
    const int g = m_activeGate;
    GateDef &gate = m_gates[g];

    // Y → start
    gate.start = float((ev->position().y() - plot.top()) / plot.height() * totalMm);
    gate.start = qBound(0.0f, float(gate.start), float(totalMm - gate.width));
    // X → threshold
    gate.threshold = float((ev->position().x() - plot.left()) / plot.width() * 100.0);
    gate.threshold = qBound(0.0f, gate.threshold, 99.0f);

    update();
    emit gateDragged(g, gate.start, gate.threshold);
}

void AScanWidget::mouseReleaseEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        // 最后一次更新确保最终位置精确
        const QRectF plot = plotRect();
        const double totalMm = totalRangeMm();
        const int g = m_activeGate;
        GateDef &gate = m_gates[g];

        gate.start = float((ev->position().y() - plot.top()) / plot.height() * totalMm);
        gate.start = qBound(0.0f, float(gate.start), float(totalMm - gate.width));
        gate.threshold = float((ev->position().x() - plot.left()) / plot.width() * 100.0);
        gate.threshold = qBound(0.0f, gate.threshold, 99.0f);

        update();
        emit gateDragged(g, gate.start, gate.threshold);
    }
    QWidget::mouseReleaseEvent(ev);
}
