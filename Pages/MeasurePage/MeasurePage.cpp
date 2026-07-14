#include "MeasurePage.h"
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <cmath>

MeasurePage::MeasurePage(QWidget *parent) : QFrame(parent)
{
    setObjectName("MeasurePagePanel");
    setFrameStyle(QFrame::Box);
    setLineWidth(1);
    setFixedWidth(160);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(4);

    // ── 读数项 ──
    m_beamReading       = new ReadingItem(QString::fromUtf8("声束号"),   "",   "0",    "#f2fbff");
    m_gainReading       = new ReadingItem(QString::fromUtf8("模拟增益"), "dB", "18.0", "#f2fbff");
    m_gateAAmpReading   = new ReadingItem(QString::fromUtf8("A门幅度"),  "%",  "--",   "#ff4444");
    m_gateAPathReading  = new ReadingItem(QString::fromUtf8("A门声程"),  "mm", "--",   "#f2fbff");
    m_gateBAmpReading   = new ReadingItem(QString::fromUtf8("B门幅度"),  "%",  "--",   "#1eea36");
    m_gateBPathReading  = new ReadingItem(QString::fromUtf8("B门声程"),  "mm", "--",   "#f2fbff");
    m_aHorizontalReading = new ReadingItem(QString::fromUtf8("A水平"),   "mm", "--",   "#1eea36");
    m_aVerticalReading  = new ReadingItem(QString::fromUtf8("A垂直"),    "mm", "--",   "#1eea36");
    m_bHorizontalReading = new ReadingItem(QString::fromUtf8("B水平"),   "mm", "--",   "#1eea36");
    m_bVerticalReading  = new ReadingItem(QString::fromUtf8("B垂直"),    "mm", "--",   "#1eea36");

    layout->addWidget(m_beamReading);
    layout->addWidget(m_gainReading);

    auto *sep1 = new QFrame;
    sep1->setObjectName("ReadingSep");
    sep1->setFixedHeight(1);
    layout->addWidget(sep1);

    layout->addWidget(m_gateAAmpReading);
    layout->addWidget(m_gateAPathReading);
    layout->addWidget(m_gateBAmpReading);
    layout->addWidget(m_gateBPathReading);

    auto *sep2 = new QFrame;
    sep2->setObjectName("ReadingSep");
    sep2->setFixedHeight(1);
    layout->addWidget(sep2);

    layout->addWidget(m_aHorizontalReading);
    layout->addWidget(m_aVerticalReading);
    layout->addWidget(m_bHorizontalReading);
    layout->addWidget(m_bVerticalReading);

    layout->addStretch();

    auto *sep3 = new QFrame;
    sep3->setObjectName("ReadingSep");
    sep3->setFixedHeight(1);
    layout->addWidget(sep3);

    // ── 操作按钮 ──
    m_loadParamsBtn = new QPushButton(QString::fromUtf8("调用参数"));
    m_loadParamsBtn->setObjectName("LoadParamsBtn");
    m_loadParamsBtn->setCursor(Qt::PointingHandCursor);
    m_loadParamsBtn->setFixedHeight(30);
    connect(m_loadParamsBtn, &QPushButton::clicked, this, &MeasurePage::loadParamsRequested);

    m_freezeBtn = new QPushButton(QString::fromUtf8("冻结"));
    m_freezeBtn->setObjectName("FreezeBtn");
    m_freezeBtn->setCursor(Qt::PointingHandCursor);
    m_freezeBtn->setFixedHeight(30);
    connect(m_freezeBtn, &QPushButton::clicked, this, [this] {
        setFrozen(!m_frozen);
        emit freezeChanged(m_frozen);
    });

    m_screenshotBtn = new QPushButton(QString::fromUtf8("截屏"));
    m_screenshotBtn->setObjectName("ScreenshotBtn");
    m_screenshotBtn->setCursor(Qt::PointingHandCursor);
    m_screenshotBtn->setFixedHeight(30);
    connect(m_screenshotBtn, &QPushButton::clicked, this, &MeasurePage::screenshotRequested);

    m_exitBtn = new QPushButton(QString::fromUtf8("退出"));
    m_exitBtn->setObjectName("ExitBtn");
    m_exitBtn->setCursor(Qt::PointingHandCursor);
    m_exitBtn->setFixedHeight(30);
    connect(m_exitBtn, &QPushButton::clicked, this, [this] {
        auto btn = QMessageBox::question(
            this, QString::fromUtf8("关闭仪器"),
            QString::fromUtf8("是否关闭仪器?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (btn == QMessageBox::Yes)
            emit powerOffAndExitRequested();
        else if (btn == QMessageBox::No)
            emit exitRequested();
    });

    layout->addWidget(m_loadParamsBtn);
    layout->addWidget(m_freezeBtn);
    layout->addWidget(m_screenshotBtn);
    layout->addWidget(m_exitBtn);

    setStyleSheet(R"(
        MeasurePage, #MeasurePagePanel {
            background:#0a1520;
            border:1px solid #1a3a52;
            border-radius:6px;
        }
        #ReadingSep {
            background:#1a3a52;
        }
        #ReadingName {
            color:#8ab8d0; font-size:13px; font-weight:600; background:transparent;
        }
        #LoadParamsBtn {
            background:#0a6e3b; color:white; border:1px solid #1a9e5e;
            border-radius:4px; font-weight:600; font-size:13px;
        }
        #LoadParamsBtn:hover { background:#0e8a4d; }
        #FreezeBtn {
            background:#18536e; color:white; border:1px solid #3b7893;
            border-radius:4px; font-weight:600; font-size:13px;
        }
        #FreezeBtn:hover { background:#126aa0; }
        #ScreenshotBtn {
            background:#18536e; color:white; border:1px solid #3b7893;
            border-radius:4px; font-weight:600; font-size:13px;
        }
        #ScreenshotBtn:hover { background:#126aa0; }
        #ExitBtn {
            background:#8b2020; color:white; border:1px solid #b03030;
            border-radius:4px; font-weight:600; font-size:13px;
        }
        #ExitBtn:hover { background:#c03030; }
    )");
}

void MeasurePage::updateGateReadings(char gate, double amplitudePercent, double soundPathMm,
                                     double angleDegrees, double horizontalOffsetMm)
{
    // ampRaw: 硬件 uint8 (0-255)，转换为百分比 (0-100%)
    //   若值 <= 100，假定硬件已直接给百分比
    //   若值 > 100，假定 0-255 满量程映射
    // pathRaw: 硬件 uint16，单位 0.1mm → mm
    constexpr double Pi = 3.14159265358979323846;
    const double radians = qAbs(angleDegrees) * Pi / 180.0;
    const double horizontal = soundPathMm * std::sin(radians) + horizontalOffsetMm;
    const double vertical = soundPathMm * std::cos(radians);

    if (gate == 'A') {
        m_gateAAmpReading->setValue(amplitudePercent, 1);
        m_gateAPathReading->setValue(soundPathMm, 1);
        // 水平/垂直：需配合编码器+闸门触发逻辑，暂无数据
        m_aHorizontalReading->setValue("--");
        m_aHorizontalReading->setValue(horizontal, 1);
        m_aVerticalReading->setValue(vertical, 1);
    } else if (gate == 'B') {
        m_gateBAmpReading->setValue(amplitudePercent, 1);
        m_gateBPathReading->setValue(soundPathMm, 1);
        m_bHorizontalReading->setValue(horizontal, 1);
        m_bVerticalReading->setValue(vertical, 1);
    }
    // Gate C: 耦合监视闸门，右侧面板不显示读数
}

void MeasurePage::updateBeamInfo(int beamNo, double gain)
{
    m_beamReading->setValue(QString::number(beamNo));
    m_gainReading->setValue(gain, 1);
}

void MeasurePage::setFrozen(bool frozen)
{
    m_frozen = frozen;
    if (frozen) {
        m_freezeBtn->setText(QString::fromUtf8("解冻"));
        m_freezeBtn->setStyleSheet(
            "QPushButton{background:#c2590a;color:white;border:1px solid #e87020;"
            "border-radius:4px;font-weight:600;font-size:13px;}"
            "QPushButton:hover{background:#e87020;}");
    } else {
        m_freezeBtn->setText(QString::fromUtf8("冻结"));
        m_freezeBtn->setStyleSheet(
            "QPushButton{background:#18536e;color:white;border:1px solid #3b7893;"
            "border-radius:4px;font-weight:600;font-size:13px;}"
            "QPushButton:hover{background:#126aa0;}");
    }
}
