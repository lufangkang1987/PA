#include "BScanWidget.h"
#include <QPainter>
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

// ================================================================
//  颜色映射 —— 移植自 MFC Value2Color4 / hsv2rgb
// ================================================================

QRgb BScanWidget::hsv2rgb(double h, double s, double v)
{
    s /= 100.0;
    v /= 100.0;
    double c = v * s;
    double x = c * (1.0 - qAbs(std::fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;
    double r = 0.0, g = 0.0, b = 0.0;

    if (h < 60.0)        { r = c; g = x; b = 0; }
    else if (h < 120.0)  { r = x; g = c; b = 0; }
    else if (h < 180.0)  { r = 0; g = c; b = x; }
    else if (h < 240.0)  { r = 0; g = x; b = c; }
    else if (h < 300.0)  { r = x; g = 0; b = c; }
    else                 { r = c; g = 0; b = x; }

    int ri = static_cast<int>((r + m) * 255.0 + 0.5);
    int gi = static_cast<int>((g + m) * 255.0 + 0.5);
    int bi = static_cast<int>((b + m) * 255.0 + 0.5);

    return qRgb(qBound(0, ri, 255), qBound(0, gi, 255), qBound(0, bi, 255));
}

QRgb BScanWidget::valueToColor(unsigned int value)
{
    // 5 段 HSV 颜色映射，与 MFC Value2Color4 完全一致
    double h = 0.0, s = 0.0, v = 0.0;

    if (value <= 60) {
        h = 240.0 - (24.0 / 61.0) * value;
        s = (73.0 / 61.0) * value;
        v = 100.0 - (51.0 / 61.0) * value;
    } else if (value <= 102) {
        h = 216.0 - (75.0 / 42.0) * (value - 61);
        s = 73.0 + (7.0 / 42.0) * (value - 61);
        v = 49.0 + (6.0 / 42.0) * (value - 61);
    } else if (value <= 179) {
        h = 141.0 - (84.0 / 76.0) * (value - 103);
        s = 86.0 - (15.0 / 76.0) * (value - 103);
        v = 55.0 + (36.0 / 76.0) * (value - 103);
    } else if (value <= 230) {
        h = 57.0 - (56.0 / 50.0) * (value - 180);
        s = 71.0 + (5.0 / 50.0) * (value - 180);
        v = 91.0 - (9.0 / 50.0) * (value - 180);
    } else { // value <= 255
        h = 1.0;
        s = 86.0 + (13.0 / 25.0) * (value - 231);
        v = 82.0 + (17.0 / 25.0) * (value - 231);
    }

    return hsv2rgb(h, s, v);
}

void BScanWidget::buildColorLUT()
{
    m_colorLUT.resize(256);
    for (int i = 0; i < 256; ++i) {
        m_colorLUT[i] = valueToColor(static_cast<unsigned int>(i));
    }
}

// ================================================================
//  扫查法则计算 —— 移植自 MFC GetScanRules()
// ================================================================

void BScanWidget::computeScanRules()
{
    if (m_scanType == 3) {  // TFM — 不适用 B 扫
        for (int k = 0; k < MaxBeams; ++k) {
            m_rules[k].x = 0.0;
            m_rules[k].ang = 0.0;
        }
        return;
    }

    int num;
    if (m_scanType == 0) {
        // S-Scan: 声束数由 BeamCount 决定
        num = m_beamCount;
        if (num < 1) num = 1;

        const double angRange = m_angleTo - m_angleFrom;
        for (int k = 0; k < num; ++k) {
            const double ang = m_angleFrom + k * angRange / (num - 1);
            m_rules[k].ang = ang;

            // S 扫声束从扇出点出发，在图像上均匀分布
            // 默认以 x=255 为中心，按角度差分配偏移
            double x = 255.0;
            if (qAbs(angRange) > 0.01) {
                x = 255.0 + (ang - (m_angleFrom + m_angleTo) / 2.0) * 400.0 / qAbs(angRange);
            }
            m_rules[k].x = qBound(0.0, x, 511.0);
        }
    } else if (m_scanType == 1) {
        // L-Scan: 电子线性扫描
        num = m_eleEnd - m_eleStart + 1 - m_eleAperture + 1;
        if (num < 1) num = 1;

        const double angRad = m_angleFrom * M_PI / 180.0;
        double usableW = 512.0 - 400.0 * qSin(angRad);
        if (usableW < 1.0) usableW = 1.0;

        for (int k = 0; k < num; ++k) {
            m_rules[k].x = k * usableW / (num - 1);
            m_rules[k].ang = m_angleFrom;
        }
    } else {
        // CL-Scan: 环形阵列
        num = m_probeCount;
        if (num < 1) num = 1;

        for (int k = 0; k < num; ++k) {
            m_rules[k].x = k * 512.0 / (num - 1);
            m_rules[k].ang = m_angleFrom;
        }
    }

    // 未使用的规则置零
    for (int k = num; k < MaxBeams; ++k) {
        m_rules[k].x = 0.0;
        m_rules[k].ang = 0.0;
    }
}

// ================================================================
//  软件波束合成 —— 移植自 MFC softwareImaging()
// ================================================================

void BScanWidget::softwareImaging(
    const std::vector<std::array<uint8_t, 400>> &waveforms,
    int beamCount, uint8_t *img)
{
    if (beamCount < 2) {
        // 单声束或无数据：填充背景
        std::memset(img, 0xFF, SImageSize);
        return;
    }

    // 预计算每条声束的 sin/cos
    double cosA[128], sinA[128];
    for (int i = 0; i < beamCount; ++i) {
        cosA[i] = qCos(m_rules[i].ang * M_PI / 180.0);
        sinA[i] = qSin(m_rules[i].ang * M_PI / 180.0);
    }

    // 逐像素扫描：从屏幕底部 (i=399) 到顶部 (i=0)
    for (int i = 399; i >= 0; --i) {
        int b = 0;  // 当前搜索到的声束索引（跨像素保持，单调递增）
        for (int j = 0; j < 512; ++j) {

            // 寻找像素 (j,i) 右侧的第一条声束线
            double x1;
            for (; b < beamCount; ++b) {
                x1 = (j - m_rules[b].x) * cosA[b] - i * sinA[b];
                if (x1 < 0) break;  // 像素在声束 b 的左侧
            }

            if (b == 0 || b == beamCount) {
                // 像素在所有声束的左侧或右侧 → 背景
                *img = 0xFF;
            } else {
                // 像素位于声束 b-1 和 b 之间
                const double y1 = i * cosA[b] + (j - m_rules[b].x) * sinA[b];
                const double y0 = i * cosA[b - 1] + (j - m_rules[b - 1].x) * sinA[b - 1];
                const int n0 = static_cast<int>(y0 + 1.0);
                const int n1 = static_cast<int>(y1 + 1.0);

                if (n0 >= 400 || n1 >= 400) {
                    *img = 0xFF;
                } else {
                    // 计算像素到声束 b-1 的垂直距离
                    const double x0 = (j - m_rules[b - 1].x) * cosA[b - 1] - i * sinA[b - 1];
                    const double d = x0 - x1;  // 两声束间的总垂直距离

                    const int v0 = waveforms[b - 1][n0];
                    const int v1 = waveforms[b][n1];

                    // 三段式插值（与 MFC 一致）
                    if (x0 < 0.25 * d)
                        *img = static_cast<uint8_t>(v0);
                    else if (x0 < 0.75 * d)
                        *img = static_cast<uint8_t>((v0 + v1) / 2);
                    else
                        *img = static_cast<uint8_t>(v1);

                    if (*img > 250)
                        *img = 250;
                }
            }
            ++img;  // 下一个像素
        }
    }
}

// ================================================================
//  构建显示图像
// ================================================================

QImage BScanWidget::buildDisplayImage() const
{
    QImage img(512, 400, QImage::Format_RGB32);
    if (!m_hasData) {
        // 无数据时显示纯深色背景，提示"等待数据…"
        img.fill(QColor(7, 17, 27).rgb());
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
                dst[x] = QColor(7, 17, 27).rgb();
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
    ++m_frameCount;

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
    p.fillRect(rect(), QColor(7, 17, 27));

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
    QFont axisFont("Microsoft YaHei", 8);
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
        QFont hintFont("Microsoft YaHei", 14);
        p.setFont(hintFont);
        p.drawText(plot, Qt::AlignCenter,
                   QString::fromUtf8("等待数据…")); // "等待数据…"
    }
}
