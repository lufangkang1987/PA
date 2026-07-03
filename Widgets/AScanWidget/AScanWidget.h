#pragma once
#include <QWidget>
#include <QVector>
#include <QElapsedTimer>

/// @brief A 扫描视图 — 横轴=螺栓长度(mm)，纵轴=回波幅度(%)
///
/// 通过 setAcousticParams(velocity, sampleRate) 实现
/// 采样点序号 → 长度(mm) 的标尺换算：
///     depth_mm = sample_index × velocity / (2 × sampleRate_hz) × 1000
///
/// 若未调用 setAcousticParams()，默认 velocity=5920 m/s(钢纵波), sampleRate=100 MHz
class AScanWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AScanWidget(QWidget *parent = nullptr);

    /// 设置声学参数，用于横轴长度标尺换算
    /// @param velocity   声速(m/s)，默认 5920（钢纵波）
    /// @param sampleRate 采样率(MHz)，默认 100
    /// @param range      显示声程(mm)，0=自动（按全部采样点计算）
    void setAcousticParams(float velocity = 5920.0f,
                           int   sampleRate = 100,
                           float range = 0.0f);

public slots:
    /// 接收 Driver 波形数据
    void setWaveform(const QVector<double> &data,
                     int beamIndex, int frameIndex, int rectifyMode);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    // ── 波形数据 ──
    QVector<double> m_data;
    int  m_beamIndex   = 0;
    int  m_frameIndex  = 0;
    int  m_rectifyMode = 3;          // 默认 RF
    bool m_isLive      = false;

    // ── 声学参数（横轴标尺） ──
    float m_velocity   = 5920.0f;    // m/s
    int   m_sampleRate = 100;        // MHz
    float m_userRange  = 0.0f;       // mm, 0=自动

    // ── FPS 统计 ──
    QElapsedTimer m_fpsTimer;
    int  m_frameCount   = 0;
    float m_currentFps  = 0.0f;

    // ── 常量 ──
    static constexpr int kMaxSamples = 400;  // 当前硬件每次发 400 点
};
