#pragma once
#include <QWidget>
#include <QImage>
#include <QVector>
#include <vector>
#include <array>
#include <cstdint>
#include "DataTypes.h"

/// @brief B-Scan 实时成像 Widget
///
/// 接收全部 128 声束的波形数据，运行 MFC 版 softwareImaging() 算法
/// 进行扇扫→笛卡尔坐标变换，生成 512×400 彩色 B 扫图像。
///
/// 数据流：
///   CTSPA22SDriver::multiBeamWaveformsReady
///     → HomePage lambda
///       → BScanWidget::setMultiBeamData()
///         → computeScanRules()
///         → softwareImaging() → m_sImage[204800]
///         → update() → paintEvent() → QImage 渲染
class BScanWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BScanWidget(QWidget *parent = nullptr);

    // ========= 数据入口 =========

    /// 接收全部声束波形（128 声束 × 400 采样点，已归一化到 [0,1] 或 RF 时 [-1,1]）
    /// @param waves   [beamIndex][sampleIndex] 归一化幅度
    /// @param isRF    是否为 RF 模式（双极性数据）
    void setMultiBeamData(const QVector<QVector<double>> &waves, bool isRF = false);

    // ========= 参数设置 =========

    /// 设置扫查参数（影响 scan rule 计算）
    void setScanParams(int scanType, float angleFrom, float angleTo,
                       int beamCount, float range,
                       int probeCount = 64, int eleStart = 1,
                       int eleEnd = 64, int eleAperture = 16);

    /// 设置声学参数（影响深度标尺）
    void setAcousticParams(float velocity, float range, int sampleRate);

    /// 冻结 / 解冻
    void setFrozen(bool frozen);
    bool isFrozen() const { return m_frozen; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // ========= 颜色映射 =========

    void buildColorLUT();

    // ========= 扫查法则计算 =========

    /// 根据扫描类型和参数计算每条声束的 (x, ang) 成像法则
    void computeScanRules();

    // ========= 波束合成 =========

    /// MFC softwareImaging() 算法：
    /// 将 128 条声束的 A 波数据合成为 512×400 的 B 扫扇形成像
    void softwareImaging(const std::vector<std::array<uint8_t, 400>> &waveforms,
                         int beamCount, uint8_t *img);

    // ========= 渲染 =========

    /// 从 SImage 构建用于绘制的 QImage（适配显示尺寸）
    QImage buildDisplayImage() const;

    // ============================================================
    //  数据成员
    // ============================================================

    // 颜色查找表（256 级）
    QVector<QRgb> m_colorLUT;

    // B-Scan 图像缓冲（512 列 × 400 行，共 204800 字节）
    // SImage[0..511] = 最深行 (i=399)，SImage[203776..204799] = 最浅行 (i=0)
    std::vector<uint8_t> m_sImage;

    // 显示用 QImage（512×400，缩放后绘制）
    QImage m_displayImage;

    // 扫查法则（每条声束在 512 宽图像中的 x 起点和角度）
    ScanRule m_rules[MaxBeams];

    // ===== 扫查参数 =====
    int   m_scanType     = 0;       // 0=S-Scan, 1=L-Scan, 2=CL-Scan, 3=TFM
    float m_angleFrom    = -30.0f;  // S 扫起始角 (deg)
    float m_angleTo      = 30.0f;   // S 扫结束角 (deg)
    int   m_beamCount    = 128;     // 实际有效声束数
    float m_range        = 100.0f;  // 检测范围 (mm)
    int   m_probeCount   = 64;
    int   m_eleStart     = 1;
    int   m_eleEnd       = 64;
    int   m_eleAperture  = 16;

    // ===== 声学参数 =====
    float m_velocity     = 5920.0f; // 声速 (m/s)
    int   m_sampleRate   = 100;     // 采样率 (MHz)

    // ===== 状态 =====
    bool  m_frozen       = false;
    bool  m_hasData      = false;   // 是否已接收过真实数据
};
