#pragma once
#include <QWidget>
#include <QVector>
#include <QElapsedTimer>

/// @brief 闸门参数
struct GateDef
{
    bool  enabled   = false;   // 是否启用
    float start     = 0.0f;    // 起位 (mm)
    float width     = 5.0f;    // 宽度 (mm)
    float threshold = 30.0f;   // 阈值 (%)
    QColor color = Qt::red;    // 显示颜色
};

/// @brief A 扫描视图 — 横轴=深度(mm)，纵轴=回波幅度(%)
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

    /// 冻结控制：frozen=true 时不更新波形，但保留最后一帧画面
    void setLive(bool live);

    /// 设置单个闸门参数
    /// @param gate 闸门编号: 0=A, 1=B, 2=C
    void setGate(int gate, bool enabled, float start, float width,
                 float threshold, const QColor &color);

    void setCalibrationGuide(bool visible, int targetPercent = 80);

    /// 设置当前拖拽闸门 (0=A, 1=B, 2=C)
    void setActiveGate(int gate) { m_activeGate = gate; }
    int  activeGate() const       { return m_activeGate; }

    /// 报警状态
    void setAlarm(bool on) { if (m_alarm != on) { m_alarm = on; update(); } }

signals:
    /// 闸门拖拽完成 (gate, start_mm, threshold_pct)
    void gateDragged(int gate, float start, float threshold);
    /// 点击A扫区域切换声束 (beamIndex)
    void beamChangeRequested(int beamIndex);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QRectF plotRect() const;        // 绘图区矩形
    double totalRangeMm() const;    // 总声程 mm
    // ── 波形数据 ──
    QVector<double> m_data;
    int  m_beamIndex   = 0;
    int  m_frameIndex  = 0;
    int  m_rectifyMode = 0;          // 默认全波检波，幅度范围 0~100%
    bool m_isLive      = false;      // 收到过真实数据（区分 mock vs real）
    bool m_frozen      = false;      // 冻结模式：不接收新波形

    // ── 声学参数（横轴标尺） ──
    float m_velocity   = 5920.0f;    // m/s
    int   m_sampleRate = 100;        // MHz
    float m_userRange  = 0.0f;       // mm, 0=自动

    // ── 闸门 ──
    GateDef m_gates[3];              // 0=A, 1=B, 2=C
    int     m_activeGate = 0;        // 当前拖拽闸门
    bool    m_dragging   = false;    // 正在拖拽中
    bool    m_alarm      = false;    // 报警状态
    bool    m_calibrationGuide = false;
    int     m_calibrationTarget = 80;
    QElapsedTimer m_fpsTimer;
    int  m_frameCount   = 0;
    float m_currentFps  = 0.0f;

    // ── 常量 ──
    static constexpr int kMaxSamples = 400;  // 当前硬件每次发 400 点
};
