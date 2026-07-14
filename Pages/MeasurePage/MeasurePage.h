#pragma once
#include <QFrame>
#include <QPushButton>
#include "HomePage.h"  // for ElidedLabel, ReadingValueWidget, ReadingItem

/// @brief 右侧测量面板：读数显示 + 操作按钮
///
/// 从 HomePage 右侧面板独立出来，作为 MainWindow 右侧固定栏。
/// 布局与 MFC 版右侧按钮区对应。
class MeasurePage : public QFrame
{
    Q_OBJECT
public:
    explicit MeasurePage(QWidget *parent = nullptr);

    // ========= 读数更新接口 =========
    void updateGateReadings(char gate, double amplitudePercent, double soundPathMm,
                            double angleDegrees, double horizontalOffsetMm = 0.0);
    void updateBeamInfo(int beamNo, double gain);

    // ========= 冻结控制 =========
    void setFrozen(bool frozen);
    bool isFrozen() const { return m_frozen; }

signals:
    void freezeChanged(bool frozen);
    void screenshotRequested();
    void exitRequested();
    void powerOffAndExitRequested();
    void loadParamsRequested();

private:
    // ── 读数项 ──
    ReadingItem *m_beamReading;
    ReadingItem *m_gainReading;
    ReadingItem *m_gateAAmpReading;
    ReadingItem *m_gateAPathReading;
    ReadingItem *m_gateBAmpReading;
    ReadingItem *m_gateBPathReading;
    ReadingItem *m_aHorizontalReading;
    ReadingItem *m_aVerticalReading;
    ReadingItem *m_bHorizontalReading;
    ReadingItem *m_bVerticalReading;

    // ── 按钮 ──
    QPushButton *m_freezeBtn;
    QPushButton *m_loadParamsBtn;
    QPushButton *m_screenshotBtn;
    QPushButton *m_exitBtn;

    bool m_frozen = false;
};
