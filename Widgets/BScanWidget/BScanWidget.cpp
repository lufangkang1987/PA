#include "BScanWidget.h"
#include "PAParams.h"
#include <QPainter>
#include "Theme.h"
#include "ColorMap.h"
#include <QtMath>
#include <algorithm>
#include <cstring>
#include <cmath>

// ================================================================
//  构造 & 初始化
// ================================================================

BScanWidget::BScanWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setMinimumWidth(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 初始化 SImage 为全 0xFF（白色背景——无信号区域）
    m_sImage.resize(SImageSize, 0xFF);

    // 构建颜色查找表
    buildColorLUT();

    // 构建初始显示图像
    m_displayImage = buildDisplayImage();
}

void BScanWidget::buildColorLUT()
{
    m_colorLUT.resize(256);
    for (int i = 0; i < 256; ++i)
        m_colorLUT[i] = amplitudeToColor(i);
}

// ================================================================
//  扫查法则计算 —— 移植自 MFC GetScanRules()
// ================================================================

void BScanWidget::computeScanRules()
{
    if (m_scanType == 3) {
        std::fill_n(m_rules, MaxBeams, ScanRule{});
        return;
    }
    PAParams p;
    p.scanType    = m_scanType;
    p.beamCount   = m_beamCount;
    p.angleFrom   = m_angleFrom;
    p.angleTo     = m_angleTo;
    p.angle       = m_angleFrom;
    p.range       = m_range;
    p.probeCount  = m_probeCount;
    p.probePitch  = 1.0f;
    p.eleStart    = m_eleStart;
    p.eleEnd      = m_eleEnd;
    p.eleAperture = m_eleAperture;
    ::computeScanRules(p, nullptr, nullptr, m_rules);
}

// ================================================================
//  软件波束合成 —— 移植自 MFC softwareImaging()
// ================================================================

void BScanWidget::softwareImaging(
    const std::vector<std::array<uint8_t, 400>> &waveforms,
    int beamCount, uint8_t *img)
{
    const int count = qBound(0, beamCount, MaxBeams);
    const uint8_t *waveP[MaxBeams];
    for (int b = 0; b < count; ++b)
        waveP[b] = waveforms[b].data();
    ::softwareImaging(waveP, m_rules, count, img);
}

// ================================================================
//  构建显示图像
// ================================================================

QImage BScanWidget::buildDisplayImage() const
{
    QImage img(512, 400, QImage::Format_RGB32);
    if (!m_hasData) {
        // 无数据时显示纯深色背景，提示"等待数据…"
        img.fill(ThemeColor::DeepBg.rgb());
        return img;
    }

    // SImage 的布局（从 MFC softwareImaging 输出）：
    //   SImage 行 0 → 屏幕坐标 i=399（最深/底部）
    //   SImage 行 399 → 屏幕坐标 i=0（最浅/顶部）
    //
    // Qt QImage 行 0 是顶部，行 399 是底部。
    // 因此需要垂直翻转：
    //   QImage 行 y ← SImage 行 (399 - y)
    for (int y = 0; y < 400; ++y) {
        const int srcRow = 399 - y;
        const uint8_t *src = m_sImage.data() + srcRow * 512;
        QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(y));

        for (int x = 0; x < 512; ++x) {
            const uint8_t val = src[x];
            if (val >= 255) {
                // 背景区域保持深色
                dst[x] = ThemeColor::DeepBg.rgb();
            } else if (val == 0) {
                // 零幅度 → 白色（与 MFC 一致）
                dst[x] = qRgb(255, 255, 255);
            } else {
                // 颜色映射
                dst[x] = m_colorLUT[val];
            }
        }
    }

    return img;
}

// ================================================================
//  数据入口
// ================================================================

void BScanWidget::setMultiBeamData(const QVector<QVector<double>> &waves, bool isRF)
{
    if (m_frozen) return;

    const int beamCount = waves.size();
    if (beamCount < 1) return;

    // 用实际接收到的声束数更新
    m_beamCount = qMin(beamCount, MaxBeams);

    // 重新计算扫查法则（参数可能已变更）
    computeScanRules();

    // 将归一化的 double 数据转为 uint8_t [0, 255]
    // RF 模式：[-1, +1] → 取绝对值 → [0, 255]
    // 非RF模式：[0, 1] → [0, 255]
    std::vector<std::array<uint8_t, 400>> rawWaves(m_beamCount);
    for (int b = 0; b < m_beamCount; ++b) {
        const QVector<double> &src = waves[b];
        for (int i = 0; i < 400; ++i) {
            double v;
            if (isRF) {
                v = qAbs(src.value(i, 0.0)) * 255.0;
            } else {
                v = src.value(i, 0.0) * 255.0;
            }
            rawWaves[b][i] = static_cast<uint8_t>(qBound(0.0, v, 255.0));
        }
    }

    // 运行波束合成
    softwareImaging(rawWaves, m_beamCount, m_sImage.data());

    m_hasData = true;

    // 重建显示图像并触发重绘
    m_displayImage = buildDisplayImage();
    update();
}

void BScanWidget::setScanParams(int scanType, float angleFrom, float angleTo,
                                 int beamCount, float range,
                                 int probeCount, int eleStart,
                                 int eleEnd, int eleAperture)
{
    m_scanType    = scanType;
    m_angleFrom   = angleFrom;
    m_angleTo     = angleTo;
    m_beamCount   = beamCount;
    m_range       = range;
    m_probeCount  = probeCount;
    m_eleStart    = eleStart;
    m_eleEnd      = eleEnd;
    m_eleAperture = eleAperture;

    // 参数变更后重新计算扫查法则
    computeScanRules();
}

void BScanWidget::setAcousticParams(float velocity, float range, int sampleRate)
{
    m_velocity   = velocity;
    m_range      = range;
    m_sampleRate = sampleRate;
}

void BScanWidget::setFrozen(bool frozen)
{
    m_frozen = frozen;
}

// ================================================================
//  绘制
// ================================================================

void BScanWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), ThemeColor::DeepBg);

    // 自适应边距
    const int ml = qMin(46, qMax(18, width() / 9));
    const int mt = qMin(14, qMax(6,  height() / 24));
    const int mr = qMin(50, qMax(22, width() / 8));
    const int mb = qMin(30, qMax(14, height() / 12));
    const QRect plot = rect().adjusted(ml, mt, -mr, -mb);

    if (plot.width() <= 0 || plot.height() <= 0) return;

    // 绘制 B 扫图像（缩放到 plot 区域）
    if (!m_displayImage.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawImage(plot, m_displayImage);
    }

    // ── 轴标签 ──
    QFont axisFont(ThemeFont::Ui, 8);
    p.setFont(axisFont);
    const QFontMetrics afm(axisFont);

    p.setPen(QColor(210, 222, 232));

    // Y 轴刻度
    const QString yTopStr = "0";
    const QString yBotStr = QString::number(static_cast<int>(m_range), 'f', 0);
    p.drawText(qMax(0, ml - afm.horizontalAdvance(yTopStr) - 2),
               plot.top() + afm.ascent(), yTopStr);
    p.drawText(qMax(0, ml - afm.horizontalAdvance(yBotStr) - 2),
               plot.bottom(), yBotStr);

    // X 轴标签
    const QString xLabel = QString::fromUtf8("位置(mm)"); // "位置(mm)"
    const int xLabelW = afm.horizontalAdvance(xLabel);
    p.drawText(qBound(plot.left(), plot.center().x() - xLabelW / 2,
                      plot.right() - xLabelW),
               height() - 4, xLabel);

    // Y 轴标签 (旋转)
    const QString yLabel = QString::fromUtf8("深度(mm)"); // "深度(mm)"
    const int yLabelW = afm.horizontalAdvance(yLabel);
    p.save();
    const int yLabelX = qMax(2, (ml - afm.height()) / 2 + 2);
    p.translate(yLabelX, plot.center().y() + yLabelW / 2);
    p.rotate(-90);
    p.drawText(0, 0, yLabel);
    p.restore();

    // ── 色阶条 ──
    const int scaleGap = qMin(14, qMax(4, mr / 3));
    const int scaleW   = qMin(11, qMax(6, mr / 4));
    const QRect scale(plot.right() + scaleGap, plot.top(), scaleW, plot.height());

    // 用 256 级颜色查找表绘制垂直渐变色条
    const int barH = scale.height();
    for (int y = 0; y < barH; ++y) {
        // 色条顶部 = 红色 (val 255), 底部 = 蓝色 (val 0)
        const int val = 255 - (y * 256 / barH);
        const int c = qBound(0, val, 255);
        p.fillRect(scale.left(), scale.top() + y, scaleW, 1,
                   QColor(m_colorLUT[c]));
    }
    p.setPen(QColor(185, 205, 218));
    p.drawRect(scale);

    // ── 冻结指示 ──
    if (m_frozen) {
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(plot.topLeft(), plot.bottomRight());
        p.drawLine(plot.topRight(), plot.bottomLeft());
    }

    // ── 数据状态提示 ──
    if (!m_hasData) {
        p.setPen(QColor(100, 140, 160));
        QFont hintFont(ThemeFont::Ui, 14);
        p.setFont(hintFont);
        p.drawText(plot, Qt::AlignCenter,
                   QString::fromUtf8("等待数据…")); // "等待数据…"
    }
}
