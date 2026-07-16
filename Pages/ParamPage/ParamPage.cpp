#include "ParamPage.h"
#include "ParameterDispatcher.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QStackedWidget>
#include <QListWidget>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QSizePolicy>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <cmath>

namespace {
constexpr int kSidebarWidth = 170;
constexpr int kStackWidth = 215;
constexpr int kPanelBorderWidth = 1;
constexpr int kPanelPadding = 1;
// 总内边距 = border(1px) + padding(1px) = 2px，为 6px 圆角留出空间
constexpr int kFrameInset = kPanelBorderWidth + kPanelPadding;
constexpr int kCollapsedWidth = kSidebarWidth + kFrameInset * 2;
constexpr int kExpandedWidth = kSidebarWidth + kStackWidth + kFrameInset * 2;
constexpr qint64 kCollapseDebounceMs = 180;
}

// ──────────────────────────────────────────────
// 辅助函数
// ──────────────────────────────────────────────

static void polishField(QWidget *w)
{
    w->setFixedHeight(30);
    w->setMinimumWidth(0);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

static QFormLayout *makeForm(QGroupBox *box)
{
    auto *f = new QFormLayout(box);
    f->setContentsMargins(8, 12, 8, 10);
    f->setHorizontalSpacing(6);
    f->setVerticalSpacing(7);
    f->setRowWrapPolicy(QFormLayout::WrapAllRows);
    f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    f->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    f->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    return f;
}

QDoubleSpinBox *ParamPage::makeDoubleSpin(double min, double max, double val,
                                           double step, const QString &suffix, int decimals)
{
    auto *s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setValue(val);
    s->setSingleStep(step);
    if (decimals >= 0)
        s->setDecimals(decimals);
    else
        s->setDecimals(step < 0.1 ? 2 : 1);
    if (!suffix.isEmpty()) s->setSuffix(" " + suffix);
    polishField(s);
    return s;
}

QSpinBox *ParamPage::makeIntSpin(int min, int max, int val, int step)
{
    auto *s = new QSpinBox;
    s->setRange(min, max);
    s->setValue(val);
    s->setSingleStep(step);
    polishField(s);
    return s;
}

QComboBox *ParamPage::makeCombo(const QStringList &items, int currentIdx)
{
    auto *c = new QComboBox;
    c->addItems(items);
    c->setCurrentIndex(currentIdx);
    polishField(c);
    return c;
}

/// Wrap a SpinBox with a step selector (combo showing available step levels).
/// When the user selects a different step, the SpinBox's singleStep is updated.
/// If stepLabels has <= 1 item, the SpinBox is returned directly (no wrapper needed).
static QWidget* wrapWithStepSelector(QWidget *spinBox, const QStringList &stepLabels,
                                      const QList<double> &stepValues, int defaultIdx)
{
    // Single step level: no selector needed
    if (stepLabels.size() <= 1) return spinBox;

    auto *wrapper = new QWidget;
    wrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *layout = new QHBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(spinBox, 1);

    auto *stepCombo = new QComboBox;
    stepCombo->setObjectName("StepCombo");
    stepCombo->addItems(stepLabels);
    stepCombo->setCurrentIndex(defaultIdx);
    stepCombo->setFixedWidth(75);
    polishField(stepCombo);

    // Set initial step on spinbox
    double initStep = stepValues[defaultIdx];
    if (auto *ds = qobject_cast<QDoubleSpinBox*>(spinBox))
        ds->setSingleStep(initStep);
    else if (auto *is = qobject_cast<QSpinBox*>(spinBox))
        is->setSingleStep(static_cast<int>(qRound(initStep)));

    // Connect step change
    QObject::connect(stepCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), spinBox,
            [spinBox, stepValues](int idx) {
                if (idx < 0 || idx >= stepValues.size()) return;
                double step = stepValues[idx];
                if (auto *ds = qobject_cast<QDoubleSpinBox*>(spinBox))
                    ds->setSingleStep(step);
                else if (auto *is = qobject_cast<QSpinBox*>(spinBox))
                    is->setSingleStep(static_cast<int>(qRound(step)));
            });

    layout->addWidget(stepCombo);
    return wrapper;
}

QWidget *ParamPage::createCategoryPage(const QString &title)
{
    auto *page = new QWidget;
    page->setObjectName("CategoryPage");

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget;
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName("CategoryTitle");
    layout->addWidget(titleLabel);

    page->setProperty("contentWidget", QVariant::fromValue(static_cast<QWidget*>(content)));
    page->setProperty("contentLayout", QVariant::fromValue(static_cast<QLayout*>(layout)));

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(scroll);
    scroll->setWidget(content);

    return page;
}

// ──────────────────────────────────────────────
// 各参数分类页面构建
// ──────────────────────────────────────────────

void ParamPage::buildTransmitPage()
{
    auto *page = createCategoryPage(QString::fromUtf8("发射参数"));
    auto *content = page->property("contentWidget").value<QWidget*>();
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_voltCombo = makeCombo({"110 V", "40 V", "20 V"}, m_params.tx.highVoltage);
    f->addRow(QString::fromUtf8("发射电压"), m_voltCombo);

    m_pulseWidthSpin = makeIntSpin(30, 1250, m_params.tx.pulseWidth, 10);
    m_pulseWidthSpin->setSuffix(" ns");
    f->addRow(QString::fromUtf8("脉冲宽度"), wrapWithStepSelector(m_pulseWidthSpin, {"10", "50", "100"}, {10.0, 50.0, 100.0}, 1));

    m_prfSpin = makeIntSpin(25, 20000, m_params.tx.prf, 100);
    m_prfSpin->setSuffix(" Hz");
    f->addRow(QString::fromUtf8("重复频率"), wrapWithStepSelector(m_prfSpin, {"5", "100", "1000"}, {5.0, 100.0, 1000.0}, 1));

    m_rangeSpin = makeDoubleSpin(5.0, 1000.0, m_params.tx.range, 0.1, "mm", 1);
    f->addRow(QString::fromUtf8("检测范围"), wrapWithStepSelector(m_rangeSpin, {"0.1", "1.0", "10.0", "100.0"}, {0.1, 1.0, 10.0, 100.0}, 1));

    m_tempCorrectCombo = makeCombo({QString::fromUtf8("关"), QString::fromUtf8("开")}, m_params.tx.tempCorrect);
    f->addRow(QString::fromUtf8("温度补偿"), m_tempCorrectCombo);

    m_aDataLenCombo = makeCombo({QString::fromUtf8("100 点"), QString::fromUtf8("200 点"), QString::fromUtf8("400 点")}, m_params.tx.aDataLen);
    f->addRow(QString::fromUtf8("A波长度"), m_aDataLenCombo);

    // ── 硬件下发 ──
    connect(m_voltCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.tx.highVoltage = v;
        if (m_dispatcher) m_dispatcher->setHighVoltage(v);
    });
    connect(m_pulseWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_params.tx.pulseWidth = v;
        if (m_dispatcher) m_dispatcher->setPulseWidth(v);
    });
    connect(m_prfSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_params.tx.prf = v;
        if (m_dispatcher) m_dispatcher->setPRF(v);
    });
    connect(m_rangeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_params.tx.range = static_cast<float>(v);
        if (m_dispatcher) m_dispatcher->setRange(static_cast<float>(v));
    });
    connect(m_tempCorrectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.tx.tempCorrect = v;
        if (m_dispatcher) m_dispatcher->setTemperatureCompensation(v != 0);
    });
    connect(m_aDataLenCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.tx.aDataLen = v;
        if (m_dispatcher) m_dispatcher->setADataLen(v);
    });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildReceivePage()
{
    auto *page = createCategoryPage(QString::fromUtf8("接收参数"));
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    // 模拟增益: 步进 [0.1, 1.0, 6.0] dB, 默认1.0
    m_aGainSpin = makeDoubleSpin(0.0, 80.0, m_params.rx.aGain, 0.1, "dB");
    f->addRow(QString::fromUtf8("模拟增益"), wrapWithStepSelector(m_aGainSpin, {"0.1", "1.0", "6.0"}, {0.1, 1.0, 6.0}, 1));

    // 数字增益: 步进 [0.1, 1.0, 6.0] dB, 默认1.0
    m_dGainSpin = makeDoubleSpin(-12.0, 12.0, m_params.rx.dGain, 0.1, "dB");
    f->addRow(QString::fromUtf8("数字增益"), wrapWithStepSelector(m_dGainSpin, {"0.1", "1.0", "6.0"}, {0.1, 1.0, 6.0}, 1));

    // 声束号: 步进 [1, 10], 默认1 (细=中=1, 粗=10)
    m_beamNoSpin = makeIntSpin(0, 127, m_params.rx.curBeam, 1);
    f->addRow(QString::fromUtf8("声束号"), wrapWithStepSelector(m_beamNoSpin, {"1", "10"}, {1.0, 10.0}, 0));

    m_rectifyCombo = makeCombo({QString::fromUtf8("全波"), QString::fromUtf8("正半波"), QString::fromUtf8("负半波")}, m_params.rx.rectify);
    f->addRow(QString::fromUtf8("检波方式"), m_rectifyCombo);

    m_filterCombo = makeCombo({
        "0.5-20.0 MHz", "0.5-15.0 MHz", "0.5-10.0 MHz", "0.5-5.0 MHz",
        "1.0-20.0 MHz", "3.0-20.0 MHz", "5.0-20.0 MHz", "7.0-20.0 MHz",
        "10.0-20.0 MHz", "1.0 MHz", "2.5 MHz", "4.0 MHz",
        "5.0 MHz", "7.5 MHz", "10.0 MHz", "15.0 MHz"
    }, m_params.rx.filter);
    f->addRow(QString::fromUtf8("滤波器"), m_filterCombo);

    m_videoCombo = makeCombo({QString::fromUtf8("无"), "1", "2", "3", "4", QString::fromUtf8("平滑")}, m_params.rx.video);
    f->addRow(QString::fromUtf8("视频检波"), m_videoCombo);

    // ── 硬件下发 ──
    connect(m_aGainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_params.rx.aGain = static_cast<float>(v);
        if (m_dispatcher) m_dispatcher->setAnalogGain(static_cast<float>(v));
        emit beamInfoChanged(m_params.rx.curBeam, v);
    });
    connect(m_dGainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_params.rx.dGain = static_cast<float>(v);
        if (m_dispatcher) m_dispatcher->setDigitalGain(static_cast<float>(v));
    });
    connect(m_beamNoSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_params.rx.curBeam = v;
        emit beamInfoChanged(v, m_params.rx.aGain);
    });
    connect(m_rectifyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.rx.rectify = v;
        if (m_dispatcher) m_dispatcher->setRectify(v);
    });
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.rx.filter = v;
        if (m_dispatcher) m_dispatcher->setFilter(v);
    });
    connect(m_videoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.rx.video = v;
        if (m_dispatcher) {
            if (v < 5) {
                m_dispatcher->setASmooth(false);
                m_dispatcher->setVideoDetect(true);
            } else {
                m_dispatcher->setVideoDetect(false);
                m_dispatcher->setASmooth(true);
            }
        }
    });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildGatePage()
{
    auto *page = createCategoryPage(QString::fromUtf8("闸门参数"));
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_gateSelCombo = makeCombo({QString::fromUtf8("A 闸门"), QString::fromUtf8("B 闸门"), QString::fromUtf8("C 闸门")}, m_params.gate.gateSelect);
    connect(m_gateSelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onGateSelectChanged);
    f->addRow(QString::fromUtf8("闸门选择"), m_gateSelCombo);

    // 闸门起位: 步进 [0.1, 1.0, 10.0] mm, 默认1.0
    m_gateStartSpin = makeDoubleSpin(0.0, 999.0,
        m_params.gate.gateStart[qBound(0, m_params.gate.gateSelect, 2)], 0.1, "mm");
    connect(m_gateStartSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("闸门起位"), wrapWithStepSelector(m_gateStartSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 闸门宽度: 步进 [0.1, 1.0, 10.0] mm, 默认1.0
    m_gateWidthSpin = makeDoubleSpin(0.0, 999.0,
        m_params.gate.gateWidth[qBound(0, m_params.gate.gateSelect, 2)], 0.1, "mm");
    connect(m_gateWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("闸门宽度"), wrapWithStepSelector(m_gateWidthSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 闸门高度: 步进 [0.1, 1.0, 10.0] %, 默认1.0
    m_gateThreshSpin = makeDoubleSpin(0.0, 99.0,
        m_params.gate.gateThreshold[qBound(0, m_params.gate.gateSelect, 2)], 0.1, "%");
    connect(m_gateThreshSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("闸门高度"), wrapWithStepSelector(m_gateThreshSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    m_gateMeasureCombo = makeCombo({QString::fromUtf8("峰值"), QString::fromUtf8("前沿")},
        m_params.gate.gateMeasure[qBound(0, m_params.gate.gateSelect, 2)]);
    connect(m_gateMeasureCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("测量方式"), m_gateMeasureCombo);

    m_gateAlarmCombo = makeCombo({QString::fromUtf8("关"), QString::fromUtf8("开")},
        m_params.gate.gateAlarm[qBound(0, m_params.gate.gateSelect, 2)]);
    connect(m_gateAlarmCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("报警开关"), m_gateAlarmCombo);

    m_gateTraceCombo = makeCombo({QString::fromUtf8("关"), QString::fromUtf8("开")},
        m_params.gate.gateTrace[qBound(0, m_params.gate.gateSelect, 2)]);
    connect(m_gateTraceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("跟踪开关"), m_gateTraceCombo);

    m_alarmSoundCombo = makeCombo({QString::fromUtf8("关"), QString::fromUtf8("A 门"), QString::fromUtf8("B 门"), QString::fromUtf8("AB 门")}, m_params.gate.alarmSound);
    connect(m_alarmSoundCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) { m_params.gate.alarmSound = idx; });
    f->addRow(QString::fromUtf8("报警声"), m_alarmSoundCombo);

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildProbePage()
{
    auto *page = createCategoryPage(QString::fromUtf8("探头参数"));
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_probeTypeCombo = makeCombo({QString::fromUtf8("自定义"), "2.5L16", "5.0S64"}, m_params.probe.probeType);
    connect(m_probeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onProbeTypeChanged);
    f->addRow(QString::fromUtf8("探头型号"), m_probeTypeCombo);

    // 探头频率: 步进 [0.1, 1.0] MHz (细=中=0.1, 粗=1.0)
    m_probeFreqSpin = makeDoubleSpin(0.2, 20.0, m_params.probe.probeFreq, 0.1, "MHz");
    f->addRow(QString::fromUtf8("探头频率"), wrapWithStepSelector(m_probeFreqSpin, {"0.1", "1.0"}, {0.1, 1.0}, 0));

    // 阵元数: 步进 [1, 10] (细=中=1, 粗=10)
    m_probeCountSpin = makeIntSpin(1, 128, m_params.probe.probeCount, 1);
    f->addRow(QString::fromUtf8("阵元数"), wrapWithStepSelector(m_probeCountSpin, {"1", "10"}, {1.0, 10.0}, 0));

    // 阵元间距: 步进 [0.01, 0.1, 1.0] mm, 默认0.1
    m_probePitchSpin = makeDoubleSpin(0.10, 15.00, m_params.probe.probePitch, 0.01, "mm", 2);
    f->addRow(QString::fromUtf8("阵元间距"), wrapWithStepSelector(m_probePitchSpin, {"0.01", "0.1", "1.0"}, {0.01, 0.1, 1.0}, 1));

    // 同步到 m_params（下发由"应用法则"统一触发）
    connect(m_probeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.probe.probeType = v; });
    connect(m_probeFreqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params.probe.probeFreq = static_cast<float>(v); });
    connect(m_probeCountSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_params.probe.probeCount = v; });
    connect(m_probePitchSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params.probe.probePitch = static_cast<float>(v); });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildWedgePage()
{
    auto *page = createCategoryPage(QString::fromUtf8("楔块参数"));
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_wedgeEnableCombo = makeCombo({QString::fromUtf8("否"), QString::fromUtf8("是")}, m_params.wedge.wedgeEnable);
    f->addRow(QString::fromUtf8("楔块启用"), m_wedgeEnableCombo);

    m_wedgeTypeCombo = makeCombo({QString::fromUtf8("自定义"), "GW-PA"}, m_params.wedge.wedgeType);
    connect(m_wedgeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onWedgeTypeChanged);
    f->addRow(QString::fromUtf8("楔块型号"), m_wedgeTypeCombo);

    // 楔块角度: 步进 [0.1, 1.0, 10.0]°, 默认1.0
    m_wedgeAngleSpin = makeDoubleSpin(0.0, 89.0, m_params.wedge.wedgeAngle, 0.1, QString::fromUtf8("\u00B0"));
    f->addRow(QString::fromUtf8("楔块角度"), wrapWithStepSelector(m_wedgeAngleSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 楔块声速: 步进 [1, 10, 100] m/s, 默认10
    m_wedgeVelSpin = makeIntSpin(1000, 9000, m_params.wedge.wedgeVelocity, 10);
    m_wedgeVelSpin->setSuffix(" m/s");
    f->addRow(QString::fromUtf8("楔块声速"), wrapWithStepSelector(m_wedgeVelSpin, {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));

    // 楔块高度: 步进 [0.1, 1.0, 10.0] mm, 默认1.0
    m_wedgeHeightSpin = makeDoubleSpin(0.1, 100.0, m_params.wedge.wedgeHeight, 0.1, "mm");
    f->addRow(QString::fromUtf8("楔块高度"), wrapWithStepSelector(m_wedgeHeightSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 同步到 m_params
    connect(m_wedgeEnableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.wedge.wedgeEnable = v; });
    connect(m_wedgeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.wedge.wedgeType = v; });
    connect(m_wedgeAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params.wedge.wedgeAngle = static_cast<float>(v); });
    connect(m_wedgeVelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_params.wedge.wedgeVelocity = v; });
    connect(m_wedgeHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params.wedge.wedgeHeight = static_cast<float>(v); });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildWorkpiecePage()
{
    auto *page = createCategoryPage(QString::fromUtf8("工件参数"));
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_materialCombo = makeCombo({QString::fromUtf8("钢纵波"), QString::fromUtf8("钢横波")}, m_params.wp.material);
    connect(m_materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onMaterialChanged);
    f->addRow(QString::fromUtf8("材料"), m_materialCombo);

    // 工件声速: 步进 [1, 10, 100] m/s, 默认10
    m_lVelSpin = makeIntSpin(1000, 9000, m_params.wp.lVelocity, 10);
    m_lVelSpin->setSuffix(" m/s");
    f->addRow(QString::fromUtf8("声速"), wrapWithStepSelector(m_lVelSpin, {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));

    m_traceEnableCombo = makeCombo({QString::fromUtf8("否"), QString::fromUtf8("是")}, m_params.wp.traceEnable);
    f->addRow(QString::fromUtf8("跟踪启用"), m_traceEnableCombo);

    connect(m_materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.wp.material = v; });
    connect(m_lVelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_params.wp.lVelocity = v; });
    connect(m_traceEnableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.wp.traceEnable = v; });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildScanPage()
{
    auto *page = createCategoryPage(QString::fromUtf8("扫查参数"));
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    // 槽位0: 扫查方式（固定）
    m_scanTypeCombo = makeCombo({QString::fromUtf8("S 扫"), QString::fromUtf8("L 扫")}, m_params.scan.scanType);
    connect(m_scanTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onScanTypeChanged);
    f->addRow(QString::fromUtf8("扫查方式"), m_scanTypeCombo);

    // 槽位1-6: 动态控件，根据 scanType 变化
    for (int i = 1; i <= 6; ++i) {
        m_scanLabels[i] = new QLabel;
        m_scanWidgets[i] = new QWidget;
        f->addRow(m_scanLabels[i], m_scanWidgets[i]);
    }

    // 初始填充
    onScanTypeChanged(m_params.scan.scanType);

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildTcgPage()
{
    auto *page = createCategoryPage(QString::fromUtf8("校准内容"));
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    // 校准项目: 声速 / 声束延迟 / ACG / TCG
    auto *calibItem = makeCombo({QString::fromUtf8("声速"), QString::fromUtf8("声束延迟"), "ACG", "TCG"}, m_params.tcg.calibItem);
    f->addRow(QString::fromUtf8("校准项目"), calibItem);

    // 实际距离: 10~1000 mm, 步进 0.1/1.0/10.0
    auto *realDist = makeDoubleSpin(10.0, 1000.0, m_params.tcg.realDistance, 0.1, "mm", 1);
    f->addRow(QString::fromUtf8("实际距离"), wrapWithStepSelector(realDist, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 声束延迟: 0~100 us, 步进 0.1/1.0/10.0
    auto *beamDelay = makeDoubleSpin(0.0, 100.0, m_params.tcg.beamDelay, 0.1, "us", 1);
    f->addRow(QString::fromUtf8("声束延迟"), wrapWithStepSelector(beamDelay, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // TCG系数: 0~0.5, 步进 0.001/0.01/0.1
    auto *tcgCoeff = makeDoubleSpin(0.0, 0.5, m_params.tcg.tcgCoeff, 0.001, "", 3);
    f->addRow(QString::fromUtf8("TCG系数"), wrapWithStepSelector(tcgCoeff, {"0.001", "0.01", "0.1"}, {0.001, 0.01, 0.1}, 1));

    // TCG参考点: 共 9 个参考点
    auto *refLabel = new QLabel(QString::fromUtf8("共 9 个参考点"));
    f->addRow(QString::fromUtf8("TCG参考点"), refLabel);

    // 校准启用: 关闭 / ACG
    auto *calibEnable = makeCombo({QString::fromUtf8("关闭"), "ACG"}, m_params.tcg.calibEnable);
    f->addRow(QString::fromUtf8("校准启用"), calibEnable);

    layout->addWidget(form);
    m_calibrationBtn = new QPushButton(QString::fromUtf8("开始 / 完成校准"));
    m_calibrationBtn->setFixedHeight(36);
    layout->addWidget(m_calibrationBtn);
    layout->addStretch();
    m_stack->addWidget(page);

    connect(calibItem, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int value) { m_params.tcg.calibItem = value; });
    connect(realDist, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { m_params.tcg.realDistance = float(value); });
    connect(beamDelay, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { m_params.tcg.beamDelay = float(value); });
    connect(tcgCoeff, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { m_params.tcg.tcgCoeff = float(value); });
    connect(calibEnable, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int value) { m_params.tcg.calibEnable = value; });
    connect(m_calibrationBtn, &QPushButton::clicked, this,
            [this] { emit calibrationRequested(m_params.tcg.calibItem); });
}

// ══════════════════════════════════════════════
// C扫子页（从 CScanPage 合并）
// ══════════════════════════════════════════════

void ParamPage::buildImagingPage()
{
    auto *page = createCategoryPage(QString::fromUtf8("C扫成像参数"));
    auto *content = page->property("contentWidget").value<QWidget*>();
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);
    f->setContentsMargins(5, 8, 5, 8);

    auto compactField = [](QWidget *field) {
        field->setMinimumWidth(0);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        if (auto *step = field->findChild<QComboBox*>("StepCombo"))
            step->setFixedWidth(48);
        return field;
    };

    m_imagingLineSpin[0] = makeIntSpin(0, 511, m_params.img.imgLineX1, 10);
    m_imagingLineSpin[1] = makeIntSpin(0, 511, m_params.img.imgLineX2, 10);
    m_imagingLineSpin[2] = makeIntSpin(0, 399, m_params.img.imgLineY1, 10);
    m_imagingLineSpin[3] = makeIntSpin(0, 399, m_params.img.imgLineY2, 10);
    m_degPerPointSpin = makeDoubleSpin(0.1, 5.0, m_params.img.degPerPoint, 0.1, "mm/d");
    f->addRow(QString::fromUtf8("采集线 X1"), compactField(wrapWithStepSelector(m_imagingLineSpin[0], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1)));
    f->addRow(QString::fromUtf8("采集线 X2"), compactField(wrapWithStepSelector(m_imagingLineSpin[1], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1)));
    f->addRow(QString::fromUtf8("采集线 Y1"), compactField(wrapWithStepSelector(m_imagingLineSpin[2], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1)));
    f->addRow(QString::fromUtf8("采集线 Y2"), compactField(wrapWithStepSelector(m_imagingLineSpin[3], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1)));
    f->addRow(QString::fromUtf8("C扫增量"), compactField(wrapWithStepSelector(m_degPerPointSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1)));
    for (int i = 0; i < 4; ++i)
        connect(m_imagingLineSpin[i], QOverload<int>::of(&QSpinBox::valueChanged), this, [this, i](int value) {
            int *fields[] = {&m_params.img.imgLineX1, &m_params.img.imgLineX2,
                             &m_params.img.imgLineY1, &m_params.img.imgLineY2};
            *fields[i] = value;
            emit cScanViewParamsChanged();
        });
    connect(m_degPerPointSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { m_params.img.degPerPoint = float(value); emit cScanViewParamsChanged(); });

    layout->addWidget(form);

    // ── 开始扫描按钮 ──
    auto *btnFrame = new QFrame;
    btnFrame->setObjectName("ScanButtonFrame");
    auto *btnLayout = new QVBoxLayout(btnFrame);
    btnLayout->setContentsMargins(6, 10, 6, 10);
    btnLayout->setSpacing(8);

    m_scanBtn = new QPushButton(QString::fromUtf8("开始扫描"));
    m_scanBtn->setObjectName("StartScanButton");
    m_scanBtn->setFixedHeight(52);
    m_scanBtn->setCursor(Qt::PointingHandCursor);
    m_scanBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_scanBtn->setStyleSheet(
        "QPushButton{"
        "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0a8a3a, stop:1 #0bc050);"
        "  color:white;"
        "  border:1px solid #0bc050;"
        "  border-radius:8px;"
        "  font-size:18px;"
        "  font-weight:700;"
        "}"
        "QPushButton:hover{"
        "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0ba048, stop:1 #14e060);"
        "}"
        "QPushButton:pressed{"
        "  background:#087030;"
        "}"
    );
    connect(m_scanBtn, &QPushButton::clicked, this, &ParamPage::onScanButtonClicked);
    btnLayout->addWidget(m_scanBtn);

    auto *hintLabel = new QLabel(QString::fromUtf8("点击按钮开始/停止 C 扫描数据采集"));
    hintLabel->setObjectName("HintLabel");
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setMinimumWidth(0);
    hintLabel->setWordWrap(true);
    btnLayout->addWidget(hintLabel);

    layout->addWidget(btnFrame);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildEncoderPage()
{
    auto *page = createCategoryPage(QString::fromUtf8("编码器参数"));
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    auto *direction = makeCombo({QString::fromUtf8("正向"), QString::fromUtf8("反向")}, m_params.enc.direction);
    auto *coderDeg = makeDoubleSpin(0.001, 10.0, m_params.enc.coderDeg, 0.01, "mm/p", 3);
    auto *checkDistance = makeDoubleSpin(1.0, 200.0, m_params.enc.checkDistance, 0.1, "mm");
    f->addRow(QString::fromUtf8("成像方向"), direction);
    f->addRow(QString::fromUtf8("编码精度"), wrapWithStepSelector(coderDeg, {"0.001", "0.01", "0.1"}, {0.001, 0.01, 0.1}, 1));
    f->addRow(QString::fromUtf8("校准距离"), wrapWithStepSelector(checkDistance, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    m_encoderCalibrationBtn = new QPushButton(QString::fromUtf8("开始 / 结束校准"));
    auto *calBtn = m_encoderCalibrationBtn;
    calBtn->setFixedHeight(36);
    calBtn->setCursor(Qt::PointingHandCursor);
    calBtn->setStyleSheet(
        "QPushButton{background:#18536e;color:white;border:1px solid #3b7893;"
        "border-radius:4px;padding:0 16px;font-weight:600;font-size:14px;}"
        "QPushButton:hover{background:#126aa0;}"
    );
    f->addRow("", calBtn);
    connect(direction, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int value) { m_params.enc.direction = value; });
    connect(coderDeg, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { m_params.enc.coderDeg = float(value); });
    connect(checkDistance, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { m_params.enc.checkDistance = float(value); });
    connect(calBtn, &QPushButton::clicked, this, &ParamPage::encoderCalibrationRequested);

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildAnalysisPage()
{
    auto *page = createCategoryPage(QString::fromUtf8("C扫分析参数"));
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_analysisLineSpin[0] = makeIntSpin(0, 924, m_params.ana.anaLineX1, 10);
    m_analysisLineSpin[1] = makeIntSpin(0, 924, m_params.ana.anaLineX2, 10);
    m_analysisLineSpin[2] = makeIntSpin(0, 249, m_params.ana.anaLineY1, 10);
    m_analysisLineSpin[3] = makeIntSpin(0, 249, m_params.ana.anaLineY2, 10);
    const char *labels[] = {"测量线 X1", "测量线 X2", "测量线 Y1","测量线 Y2"};
    for (int i = 0; i < 4; ++i) {
        f->addRow(labels[i], wrapWithStepSelector(m_analysisLineSpin[i], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
        connect(m_analysisLineSpin[i], QOverload<int>::of(&QSpinBox::valueChanged), this, [this, i](int value) {
            int *fields[] = {&m_params.ana.anaLineX1, &m_params.ana.anaLineX2,
                             &m_params.ana.anaLineY1, &m_params.ana.anaLineY2};
            *fields[i] = value;
            emit cScanViewParamsChanged();
        });
    }

    const QString btnStyle = QString(
        "QPushButton{background:#18536e;color:white;border:1px solid #3b7893;"
        "border-radius:4px;padding:0 16px;font-weight:600;font-size:14px;}"
        "QPushButton:hover{background:#126aa0;}"
    );

    auto *pageBtn = new QPushButton(QString::fromUtf8("C扫翻页"));
    pageBtn->setFixedHeight(36);
    pageBtn->setCursor(Qt::PointingHandCursor);
    pageBtn->setStyleSheet(btnStyle);
    connect(pageBtn, &QPushButton::clicked, this, &ParamPage::cScanPageRequested);
    f->addRow("", pageBtn);

    auto *exitBtn = new QPushButton(QString::fromUtf8("退出回放"));
    exitBtn->setFixedHeight(36);
    exitBtn->setCursor(Qt::PointingHandCursor);
    exitBtn->setStyleSheet(btnStyle);
    connect(exitBtn, &QPushButton::clicked, this, &ParamPage::exitReplayRequested);
    f->addRow("", exitBtn);

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::onScanButtonClicked()
{
    m_scanning = !m_scanning;
    if (m_scanning) {
        m_scanBtn->setText(QString::fromUtf8("停止扫描"));
        m_scanBtn->setStyleSheet(
            "QPushButton{"
            "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #c23a0a, stop:1 #e85020);"
            "  color:white;"
            "  border:1px solid #e85020;"
            "  border-radius:8px;"
            "  font-size:18px;"
            "  font-weight:700;"
            "}"
            "QPushButton:hover{"
            "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #d04818, stop:1 #f86030);"
            "}"
            "QPushButton:pressed{"
            "  background:#a03008;"
            "}"
        );
        emit scanStarted();
    } else {
        m_scanBtn->setText(QString::fromUtf8("开始扫描"));
        m_scanBtn->setStyleSheet(
            "QPushButton{"
            "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0a8a3a, stop:1 #0bc050);"
            "  color:white;"
            "  border:1px solid #0bc050;"
            "  border-radius:8px;"
            "  font-size:18px;"
            "  font-weight:700;"
            "}"
            "QPushButton:hover{"
            "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0ba048, stop:1 #14e060);"
            "}"
            "QPushButton:pressed{"
            "  background:#087030;"
            "}"
        );
        emit scanStopped();
    }
}

void ParamPage::finishScan()
{
    if (m_scanning)
        onScanButtonClicked();
}

// ──────────────────────────────────────────────
// 联动逻辑
// ──────────────────────────────────────────────

void ParamPage::onScanTypeChanged(int idx)
{
    // idx: 0=S扫, 1=L扫
    struct SlotDef { const char *label; const char *unit; bool visible; };
    SlotDef defs[2][6] = {
        // S扫: 起始阵元, 结束阵元, 孔径大小, 起始角度, 结束角度, 焦距
        {{("起始阵元"),(""),true},{("结束阵元"),(""),true},{("孔径大小"),(""),true},{("起始角度"),("\u00B0"),true},{("结束角度"),("\u00B0"),true},{("焦距"),("mm"),true}},
        // L扫: 起始阵元, 结束阵元, 孔径大小, 角度, 焦距
        {{("起始阵元"),(""),true},{("结束阵元"),(""),true},{("孔径大小"),(""),true},{("角度"),("\u00B0"),true},{("焦距"),("mm"),true},{nullptr,nullptr,false}}
    };

    for (int i = 1; i <= 6; ++i) {
        const auto &d = defs[idx][i-1];
        if (!d.label) {
            m_scanLabels[i]->setText("");
            m_scanWidgets[i]->setVisible(false);
            continue;
        }
        m_scanLabels[i]->setText(d.label);
        m_scanWidgets[i]->setVisible(d.visible);

        // 删除旧控件
        auto *old = m_scanWidgets[i]->layout();
        if (old) { delete old; }

        auto *row = new QHBoxLayout(m_scanWidgets[i]);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);

        // 根据槽位和扫描类型创建控件 + 步进选择器
        QWidget *wrappedField = nullptr;
        switch (idx) {
            case 0: // S扫
                switch (i) {
                    case 1: wrappedField = wrapWithStepSelector(makeIntSpin(1, 128, m_params.scan.eleStart, 1), {"1"}, {1.0}, 0); break;
                    case 2: wrappedField = wrapWithStepSelector(makeIntSpin(1, 128, m_params.scan.eleEnd, 1), {"1"}, {1.0}, 0); break;
                    case 3: wrappedField = wrapWithStepSelector(makeIntSpin(1, 16, m_params.scan.eleAperture, 1), {"1"}, {1.0}, 0); break;
                    case 4: wrappedField = wrapWithStepSelector(makeDoubleSpin(-89.0, 89.0, m_params.scan.angleFrom, 1.0, "\u00B0"), {"1.0", "10.0"}, {1.0, 10.0}, 0); break;
                    case 5: wrappedField = wrapWithStepSelector(makeDoubleSpin(-89.0, 89.0, m_params.scan.angleTo, 1.0, "\u00B0"), {"1.0", "10.0"}, {1.0, 10.0}, 0); break;
                    case 6: wrappedField = wrapWithStepSelector(makeDoubleSpin(2.0, 1000.0, m_params.scan.focus, 0.1, "mm"), {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1); break;
                }
                break;
            case 1: // L扫
                switch (i) {
                    case 1: wrappedField = wrapWithStepSelector(makeIntSpin(1, 128, m_params.scan.eleStart, 1), {"1"}, {1.0}, 0); break;
                    case 2: wrappedField = wrapWithStepSelector(makeIntSpin(1, 128, m_params.scan.eleEnd, 1), {"1"}, {1.0}, 0); break;
                    case 3: wrappedField = wrapWithStepSelector(makeIntSpin(1, 16, m_params.scan.eleAperture, 1), {"1"}, {1.0}, 0); break;
                    case 4: wrappedField = wrapWithStepSelector(makeDoubleSpin(-89.0, 89.0, m_params.scan.angle, 1.0, "\u00B0"), {"1.0", "10.0"}, {1.0, 10.0}, 0); break;
                    case 5: wrappedField = wrapWithStepSelector(makeDoubleSpin(2.0, 1000.0, m_params.scan.focus, 0.1, "mm"), {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1); break;
                }
                break;
        }
        if (wrappedField) {
            QSpinBox *intField = qobject_cast<QSpinBox *>(wrappedField);
            if (!intField) intField = wrappedField->findChild<QSpinBox *>();
            QDoubleSpinBox *doubleField = qobject_cast<QDoubleSpinBox *>(wrappedField);
            if (!doubleField) doubleField = wrappedField->findChild<QDoubleSpinBox *>();

            if (intField) {
                connect(intField, QOverload<int>::of(&QSpinBox::valueChanged), this,
                        [this, i](int value) {
                    if (i == 1) m_params.scan.eleStart = value;
                    else if (i == 2) m_params.scan.eleEnd = value;
                    else if (i == 3) m_params.scan.eleAperture = value;
                });
            } else if (doubleField) {
                connect(doubleField, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                        [this, idx, i](double value) {
                    if (idx == 0 && i == 4) m_params.scan.angleFrom = static_cast<float>(value);
                    else if (idx == 0 && i == 5) m_params.scan.angleTo = static_cast<float>(value);
                    else if (idx == 0 && i == 6) m_params.scan.focus = static_cast<float>(value);
                    else if (idx == 1 && i == 4) m_params.scan.angle = static_cast<float>(value);
                    else if (idx == 1 && i == 5) m_params.scan.focus = static_cast<float>(value);
                });
            }
            wrappedField->setParent(m_scanWidgets[i]);
            row->addWidget(wrappedField);
        }
    }
}

void ParamPage::onProbeTypeChanged(int idx)
{
    if (idx == 1) { // 2.5L16
        m_probeFreqSpin->setValue(2.5);
        m_probeCountSpin->setValue(16);
        m_probePitchSpin->setValue(1.00);
    } else if (idx == 2) { // 5.0S64
        m_probeFreqSpin->setValue(5.0);
        m_probeCountSpin->setValue(64);
        m_probePitchSpin->setValue(1.00);
    }
}

void ParamPage::onWedgeTypeChanged(int idx)
{
    if (idx == 1) { // GW-PA
        m_wedgeAngleSpin->setValue(41.0);
        m_wedgeVelSpin->setValue(2337);
        m_wedgeHeightSpin->setValue(12.5);
    }
}

void ParamPage::onGateSelectChanged(int idx)
{
    if (idx < 0 || idx > 2) return;
    m_params.gate.gateSelect = idx;

    // 阻断 SpinBox 信号，防止切闸门时误触发 onGateParamChanged
    m_gateStartSpin->blockSignals(true);
    m_gateWidthSpin->blockSignals(true);
    m_gateThreshSpin->blockSignals(true);
    m_gateMeasureCombo->blockSignals(true);
    m_gateAlarmCombo->blockSignals(true);
    m_gateTraceCombo->blockSignals(true);

    m_gateStartSpin->setValue(m_params.gate.gateStart[idx]);
    m_gateWidthSpin->setValue(m_params.gate.gateWidth[idx]);
    m_gateThreshSpin->setValue(m_params.gate.gateThreshold[idx]);
    m_gateMeasureCombo->setCurrentIndex(m_params.gate.gateMeasure[idx]);
    m_gateAlarmCombo->setCurrentIndex(m_params.gate.gateAlarm[idx]);
    m_gateTraceCombo->setCurrentIndex(m_params.gate.gateTrace[idx]);

    m_gateStartSpin->blockSignals(false);
    m_gateWidthSpin->blockSignals(false);
    m_gateThreshSpin->blockSignals(false);
    m_gateMeasureCombo->blockSignals(false);
    m_gateAlarmCombo->blockSignals(false);
    m_gateTraceCombo->blockSignals(false);

    // 通知 HomePage 更新活跃闸门
    emit gateParamsChanged();
}

void ParamPage::onGateParamChanged()
{
    const int g = m_params.gate.gateSelect;
    const char gateName = 'A' + g;
    m_params.gate.gateStart[g]     = static_cast<float>(m_gateStartSpin->value());
    m_params.gate.gateWidth[g]     = static_cast<float>(m_gateWidthSpin->value());
    m_params.gate.gateThreshold[g] = static_cast<float>(m_gateThreshSpin->value());
    m_params.gate.gateMeasure[g]   = m_gateMeasureCombo->currentIndex();
    m_params.gate.gateAlarm[g]     = m_gateAlarmCombo->currentIndex();
    m_params.gate.gateTrace[g]     = m_gateTraceCombo->currentIndex();

    // 下发到硬件
    if (m_dispatcher)
        m_dispatcher->setGate(gateName,
                          m_params.gate.gateStart[g],
                          m_params.gate.gateWidth[g],
                          m_params.gate.gateThreshold[g],
                          m_params.gate.gateMeasure[g]);

    emit gateParamsChanged();
}

void ParamPage::onGateDragged(int gate, float start, float threshold)
{
    if (gate < 0 || gate > 2) return;
    const char gn = 'A' + gate;

    // 更新 m_params
    m_params.gate.gateStart[gate]     = start;
    m_params.gate.gateThreshold[gate] = threshold;

    // 如果拖拽的闸门与当前选中的不同，静默切换（不触发 onGateSelectChanged）
    if (gate != m_params.gate.gateSelect) {
        m_params.gate.gateSelect = gate;
        m_gateSelCombo->blockSignals(true);
        m_gateSelCombo->setCurrentIndex(gate);
        m_gateSelCombo->blockSignals(false);
    }

    // 更新 SpinBox（阻断信号，避免重复触发）
    m_gateStartSpin->blockSignals(true);
    m_gateWidthSpin->blockSignals(true);
    m_gateThreshSpin->blockSignals(true);
    m_gateMeasureCombo->blockSignals(true);
    m_gateAlarmCombo->blockSignals(true);
    m_gateTraceCombo->blockSignals(true);
    m_gateStartSpin->setValue(start);
    m_gateWidthSpin->setValue(m_params.gate.gateWidth[gate]);
    m_gateThreshSpin->setValue(threshold);
    m_gateMeasureCombo->setCurrentIndex(m_params.gate.gateMeasure[gate]);
    m_gateAlarmCombo->setCurrentIndex(m_params.gate.gateAlarm[gate]);
    m_gateTraceCombo->setCurrentIndex(m_params.gate.gateTrace[gate]);
    m_gateStartSpin->blockSignals(false);
    m_gateWidthSpin->blockSignals(false);
    m_gateThreshSpin->blockSignals(false);
    m_gateMeasureCombo->blockSignals(false);
    m_gateAlarmCombo->blockSignals(false);
    m_gateTraceCombo->blockSignals(false);

    // 下发硬件
    if (m_dispatcher)
        m_dispatcher->setGate(gn, start, m_params.gate.gateWidth[gate], threshold,
                          m_params.gate.gateMeasure[gate]);

    emit gateParamsChanged();
}

void ParamPage::getGateParams(int gate, bool &enabled, float &start, float &width,
                              float &threshold) const
{
    if (gate < 0 || gate > 2) return;
    enabled   = m_params.gate.gateAlarm[gate] != 0;
    start     = m_params.gate.gateStart[gate];
    width     = m_params.gate.gateWidth[gate];
    threshold = m_params.gate.gateThreshold[gate];
}

void ParamPage::onMaterialChanged(int idx)
{
    if (idx == 0) // 钢纵波
        m_lVelSpin->setValue(5900);
    else // 钢横波
        m_lVelSpin->setValue(3230);
}

void ParamPage::setBeamNo(int beam)
{
    if (beam < 0 || beam > 127) return;
    m_params.rx.curBeam = beam;
    if (m_beamNoSpin) {
        m_beamNoSpin->blockSignals(true);
        m_beamNoSpin->setValue(beam);
        m_beamNoSpin->blockSignals(false);
    }
    emit beamInfoChanged(beam, m_params.rx.aGain);
}

void ParamPage::setAnalysisRect(int line1, int line2, int column1, int column2)
{
    m_params.ana.anaLineX1 = line1;
    m_params.ana.anaLineX2 = line2;
    m_params.ana.anaLineY1 = column1;
    m_params.ana.anaLineY2 = column2;
    const int values[] = {line1, line2, column1, column2};
    for (int i = 0; i < 4; ++i) {
        if (!m_analysisLineSpin[i]) continue;
        QSignalBlocker blocker(m_analysisLineSpin[i]);
        m_analysisLineSpin[i]->setValue(values[i]);
    }
}

void ParamPage::setCalibratedVelocity(int velocity)
{
    m_params.wp.lVelocity = qBound(m_params.wedge.wedgeVelocity, velocity, 9000);
    if (m_lVelSpin) m_lVelSpin->setValue(m_params.wp.lVelocity);
}

void ParamPage::setCalibratedProbeDelay(float delayUs)
{
    m_params.probe.probeDelay = qBound(0.0f, delayUs, 100.0f);
    m_params.tcg.beamDelay = m_params.probe.probeDelay;
}

void ParamPage::setCalibratedACG(const QVector<float> &values)
{
    const int count = qMin(values.size(), MaxBeams);
    for (int i = 0; i < count; ++i)
        m_params.tcg.acgValue[i] = qBound(0.0f, values[i], 256.0f);
    m_params.tcg.acgSwitch = 1;
}

void ParamPage::setCalibratedCoderDeg(float mmPerPulse)
{
    if (mmPerPulse > 0.0f) m_params.enc.coderDeg = mmPerPulse;
}

void ParamPage::onApplyLaw()
{
    if (m_dispatcher) m_dispatcher->applyLaw(m_params);
}

// ═══════════════════════════════════════════════════════════
// 参数序列化 / 反序列化
// ═══════════════════════════════════════════════════════════

static double roundedJsonNumber(float value, int decimals)
{
    const double scale = std::pow(10.0, decimals);
    return std::round(static_cast<double>(value) * scale) / scale;
}

static QJsonArray floatsToJson(const float *arr, int n, int decimals = 6)
{
    QJsonArray a;
    for (int i = 0; i < n; ++i) a.append(roundedJsonNumber(arr[i], decimals));
    return a;
}

static void jsonToFloats(const QJsonArray &a, float *arr, int n)
{
    for (int i = 0; i < n && i < a.size(); ++i)
        arr[i] = static_cast<float>(a[i].toDouble());
}

static QJsonArray intsToJson(const int *arr, int n)
{
    QJsonArray a;
    for (int i = 0; i < n; ++i) a.append(arr[i]);
    return a;
}

static void jsonToInts(const QJsonArray &a, int *arr, int n)
{
    for (int i = 0; i < n && i < a.size(); ++i)
        arr[i] = a[i].toInt();
}

static QString enumText(int index, const QStringList &items)
{
    return index >= 0 && index < items.size() ? items[index] : QString::number(index);
}

static int enumIndex(const QJsonValue &value, const QStringList &items, int fallback)
{
    if (value.isDouble()) return value.toInt(fallback); // Legacy index format.
    const int index = items.indexOf(value.toString());
    return index >= 0 ? index : fallback;
}

static QJsonArray enumsToJson(const int *values, int count, const QStringList &items)
{
    QJsonArray result;
    for (int i = 0; i < count; ++i) result.append(enumText(values[i], items));
    return result;
}

static void jsonToEnums(const QJsonArray &array, int *values, int count,
                        const QStringList &items)
{
    for (int i = 0; i < count && i < array.size(); ++i)
        values[i] = enumIndex(array[i], items, values[i]);
}

static const QStringList &voltItems()           { static const QStringList s = {"110 V", "40 V", "20 V"}; return s; }
static const QStringList &tempCorrectItems()    { static const QStringList s = {QString::fromUtf8("关"), QString::fromUtf8("开")}; return s; }
static const QStringList &aDataLenItems()       { static const QStringList s = {QString::fromUtf8("100 点"), QString::fromUtf8("200 点"), QString::fromUtf8("400 点")}; return s; }
static const QStringList &rectifyItems()        { static const QStringList s = {QString::fromUtf8("全波"), QString::fromUtf8("正半波"), QString::fromUtf8("负半波")}; return s; }
static const QStringList &videoItems()          { static const QStringList s = {QString::fromUtf8("无"), "1", "2", "3", "4", QString::fromUtf8("平滑")}; return s; }
static const QStringList &gateSelectItems()     { static const QStringList s = {QString::fromUtf8("A 闸门"), QString::fromUtf8("B 闸门"), QString::fromUtf8("C 闸门")}; return s; }
static const QStringList &gateMeasureItems()    { static const QStringList s = {QString::fromUtf8("峰值"), QString::fromUtf8("前沿")}; return s; }
static const QStringList &onOffItems()          { static const QStringList s = {QString::fromUtf8("关"), QString::fromUtf8("开")}; return s; }
static const QStringList &alarmSoundItems()     { static const QStringList s = {QString::fromUtf8("关"), QString::fromUtf8("A 门"), QString::fromUtf8("B 门"), QString::fromUtf8("AB 门")}; return s; }
static const QStringList &probeTypeItems()      { static const QStringList s = {QString::fromUtf8("自定义"), "2.5L16", "5.0S64"}; return s; }
static const QStringList &wedgeEnableItems()    { static const QStringList s = {QString::fromUtf8("否"), QString::fromUtf8("是")}; return s; }
static const QStringList &wedgeTypeItems()      { static const QStringList s = {QString::fromUtf8("自定义"), "GW-PA"}; return s; }
static const QStringList &materialItems()       { static const QStringList s = {QString::fromUtf8("钢纵波"), QString::fromUtf8("钢横波")}; return s; }
static const QStringList &scanTypeItems()       { static const QStringList s = {QString::fromUtf8("S 扫"), QString::fromUtf8("L 扫")}; return s; }
static const QStringList &directionItems()      { static const QStringList s = {QString::fromUtf8("正向"), QString::fromUtf8("反向")}; return s; }
static const QStringList &calibItemItems()      { static const QStringList s = {QString::fromUtf8("声速"), QString::fromUtf8("声束延迟"), "ACG", "TCG"}; return s; }
static const QStringList &calibEnableItems()    { static const QStringList s = {QString::fromUtf8("关闭"), "ACG"}; return s; }

static const QStringList &filterTexts()
{
    static const QStringList items = {
        "0.5-20.0 MHz", "0.5-15.0 MHz", "0.5-10.0 MHz", "0.5-5.0 MHz",
        "1.0-20.0 MHz", "3.0-20.0 MHz", "5.0-20.0 MHz", "7.0-20.0 MHz",
        "10.0-20.0 MHz", "1.0 MHz", "2.5 MHz", "4.0 MHz",
        "5.0 MHz", "7.5 MHz", "10.0 MHz", "15.0 MHz"
    };
    return items;
}

static QJsonArray shortsToJson(const short *arr, int n)
{
    QJsonArray a;
    for (int i = 0; i < n; ++i) a.append(static_cast<int>(arr[i]));
    return a;
}

static void jsonToShorts(const QJsonArray &a, short *arr, int n)
{
    for (int i = 0; i < n && i < a.size(); ++i)
        arr[i] = static_cast<short>(a[i].toInt());
}

// ═══════════════════════════════════════════════════════════
// PA 参数字段声明表 — X-Macro 模式
// 每个字段声明一次，在 serialize / deserialize / _comments
// 三种上下文中定义不同宏语义，自动展开
// 新增字段只需在此表添加一行
// ═══════════════════════════════════════════════════════════
//
// 宏类型:
//   M_INT   (key, sub, field, comment)                 — int ↔ JSON int
//   M_FLOAT (key, sub, field, dec, comment)            — float ↔ JSON rounded(dec)
//   M_ENUM  (key, sub, field, itemsFn, comment)        — int enum ↔ JSON text
//   M_FARR  (key, sub, field, cnt, dec, comment)       — float[cnt] ↔ JSON array
//   M_SFARR (key, sub, field, cnt, dec, comment)       — float2D ↔ JSON array (reinterpret_cast)
//   M_SSARR (key, sub, field, cnt, comment)            — short2D ↔ JSON array (reinterpret_cast)
//   M_EARR  (key, sub, field, cnt, itemsFn, comment)   — int[cnt] ↔ JSON enum array

#define PA_PARAM_FIELDS(M) \
    /* ── 发射 ── */ \
    M_ENUM("highVoltage",   tx, highVoltage,   voltItems,          "发射电压档位") \
    M_INT("pulseWidth",     tx, pulseWidth,                         "发射脉冲宽度") \
    M_INT("prf",            tx, prf,                                "脉冲重复频率") \
    M_FLOAT("range",        tx, range, 1,                           "A扫检测范围") \
    M_ENUM("tempCorrect",   tx, tempCorrect,   tempCorrectItems,   "温度补偿开关") \
    M_ENUM("aDataLen",      tx, aDataLen,      aDataLenItems,      "A扫数据长度") \
    /* ── 接收 ── */ \
    M_FLOAT("aGain",        rx, aGain, 1,                           "模拟增益") \
    M_FLOAT("dGain",        rx, dGain, 1,                           "数字增益") \
    M_INT("curBeam",        rx, curBeam,                            "当前声束编号") \
    M_ENUM("rectify",       rx, rectify,        rectifyItems,      "检波方式") \
    M_ENUM("filter",        rx, filter,          filterTexts,      "滤波档位") \
    M_ENUM("video",         rx, video,           videoItems,        "视频滤波开关") \
    /* ── 闸门 ── */ \
    M_ENUM("gateSelect",    gate, gateSelect,    gateSelectItems,   "当前选中的闸门，0=A、1=B、2=C") \
    M_FARR("gateStart",     gate, gateStart, 3, 1,                 "A/B/C闸门起始位置数组") \
    M_FARR("gateWidth",     gate, gateWidth, 3, 1,                 "A/B/C闸门宽度数组") \
    M_FARR("gateThreshold", gate, gateThreshold, 3, 1,            "A/B/C闸门阈值数组") \
    M_EARR("gateMeasure",   gate, gateMeasure, 3, gateMeasureItems, "A/B/C闸门测量方式数组") \
    M_ENUM("alarmSound",    gate, alarmSound,    alarmSoundItems,  "报警声音开关") \
    /* ── 探头 ── */ \
    M_ENUM("probeType",     probe, probeType,    probeTypeItems,   "探头类型") \
    M_FLOAT("probeFreq",    probe, probeFreq, 1,                   "探头中心频率") \
    M_INT("probeCount",     probe, probeCount,                      "探头阵元数量") \
    M_FLOAT("probePitch",   probe, probePitch, 2,                   "探头阵元间距") \
    /* ── 楔块 ── */ \
    M_ENUM("wedgeEnable",   wedge, wedgeEnable,  wedgeEnableItems, "楔块启用开关") \
    M_ENUM("wedgeType",     wedge, wedgeType,    wedgeTypeItems,   "楔块类型") \
    M_FLOAT("wedgeAngle",   wedge, wedgeAngle, 1,                   "楔块角度") \
    M_INT("wedgeVelocity",  wedge, wedgeVelocity,                   "楔块声速") \
    M_FLOAT("wedgeHeight",  wedge, wedgeHeight, 1,                  "楔块高度") \
    /* ── 工件 ── */ \
    M_ENUM("material",      wp, material,         materialItems,   "工件材料类型") \
    M_INT("lVelocity",      wp, lVelocity,                          "工件纵波声速") \
    M_ENUM("traceEnable",   wp, traceEnable,      wedgeEnableItems, "轨迹显示开关") \
    /* ── 扫查 ── */ \
    M_ENUM("scanType",      scan, scanType,       scanTypeItems,    "扫描类型") \
    M_INT("eleStart",       scan, eleStart,                          "起始阵元") \
    M_INT("eleEnd",         scan, eleEnd,                            "结束阵元") \
    M_INT("eleAperture",    scan, eleAperture,                       "有效孔径阵元数") \
    M_FLOAT("angleFrom",    scan, angleFrom, 0,                      "扫描起始角度") \
    M_FLOAT("angleTo",      scan, angleTo, 0,                        "扫描终止角度") \
    M_FLOAT("angle",        scan, angle, 0,                          "固定扫描角度") \
    M_FLOAT("focus",        scan, focus, 1,                          "聚焦深度") \
    /* ── 成像 ── */ \
    M_INT("imgLineX1",      img, imgLineX1,                          "C扫成像X方向起始线") \
    M_INT("imgLineX2",      img, imgLineX2,                          "C扫成像X方向终止线") \
    M_INT("imgLineY1",      img, imgLineY1,                          "C扫成像Y方向起始线") \
    M_INT("imgLineY2",      img, imgLineY2,                          "C扫成像Y方向终止线") \
    M_FLOAT("degPerPoint",  img, degPerPoint, 1,                     "C扫每采样点对应角度") \
    /* ── 编码器 ── */ \
    M_ENUM("direction",     enc, direction,       directionItems,   "编码器运动方向") \
    M_FLOAT("coderDeg",     enc, coderDeg, 3,                        "编码器每点角度") \
    M_FLOAT("checkDistance", enc, checkDistance, 1,                   "检测距离") \
    /* ── 校准/TCG ── */ \
    M_ENUM("calibItem",     tcg, calibItem,       calibItemItems,   "当前校准项目") \
    M_FLOAT("realDistance",  tcg, realDistance, 1,                    "校准实际距离") \
    M_FLOAT("beamDelay",    tcg, beamDelay, 1,                       "声束延迟") \
    M_FLOAT("tcgCoeff",     tcg, tcgCoeff, 3,                        "TCG补偿系数") \
    M_ENUM("calibEnable",   tcg, calibEnable,     calibEnableItems, "校准启用开关") \
    M_INT("tcgStart",       tcg, tcgStart,                           "TCG起始点") \
    M_INT("tcgEnd",         tcg, tcgEnd,                             "TCG终止点") \
    M_ENUM("acgSwitch",     tcg, acgSwitch,       onOffItems,       "ACG开关") \
    M_ENUM("tcgSwitch",     tcg, tcgSwitch,       onOffItems,       "TCG开关") \
    M_FARR("tcgX",          tcg, tcgX, 6, 6,                        "TCG控制点位置数组") \
    M_FARR("tcgRatio",      tcg, tcgRatio, 6, 6,                    "TCG控制点增益比例数组") \
    M_FARR("acgValue",      tcg, acgValue, 128, 6,                  "各声束ACG值数组") \
    M_SSARR("tcgPointX",    tcg, tcgPointX, 10*128,                 "各声束TCG点位置数组") \
    M_SFARR("tcgPointValue", tcg, tcgPointValue, 10*128, 6,         "各声束TCG点增益数组") \
    /* ── MFC 补齐 ── */ \
    M_INT("sVelocity",      wp, sVelocity,                          "工件横波声速") \
    M_INT("diameter",       wp, diameter,                           "工件直径") \
    M_EARR("gateAlarm",     gate, gateAlarm, 3, onOffItems,        "A/B/C闸门报警方式数组") \
    M_EARR("gateTrace",     gate, gateTrace, 3, onOffItems,        "A/B/C闸门跟踪方式数组") \
    M_FLOAT("probeDelay",   probe, probeDelay, 3,                   "探头延迟") \
    /* ── TFM ── */ \
    M_FLOAT("dimX",         tfm, dimX, 3,                           "TFM成像X尺寸") \
    M_FLOAT("dimY",         tfm, dimY, 3,                           "TFM成像Y尺寸") \
    M_FLOAT("offsetX",      tfm, offsetX, 3,                        "TFM成像X偏移") \
    M_FLOAT("offsetY",      tfm, offsetY, 3,                        "TFM成像Y偏移") \
    M_FLOAT("pixelSize",    tfm, pixelSize, 3,                      "TFM像素尺寸") \
    M_FLOAT("pieceThickness", tfm, pieceThickness, 3,               "工件厚度") \
    M_FLOAT("tfmDGain",     tfm, tfmDGain, 3,                       "TFM数字增益") \
    M_INT("tfmSmooth",      tfm, tfmSmooth,                         "TFM平滑参数") \
    M_INT("parRestrainH16", tfm, parRestrainH16,                    "TFM高16位抑制参数") \
    M_INT("parRestrainL16", tfm, parRestrainL16,                    "TFM低16位抑制参数") \
    M_INT("circleDeg",      enc, circleDeg,                          "编码器一周计数") \
    M_FLOAT("imgSpanStart",  img, imgSpanStart, 3,                  "C扫成像跨度起点") \
    M_FLOAT("imgSpanEnd",    img, imgSpanEnd, 3,                    "C扫成像跨度终点") \
    /* ── 全局 ── */ \
    M_INT("readNum",        global, readNum,                         "全局读取序号") \
    M_INT("beamCount",      global, beamCount,                       "有效声束数量") \
    M_INT("tempBeamCount",  global, tempBeamCount,                   "临时声束数量")

QJsonObject ParamPage::serializeParams() const
{
    const auto &p = m_params;
    QJsonObject j;

    // ── X-Macro 展开: 序列化所有 PA_PARAM_FIELDS 字段 ──
#define M_INT(key, sub, field, _comment)             j[key] = p.sub.field;
#define M_FLOAT(key, sub, field, dec, _comment)      j[key] = roundedJsonNumber(p.sub.field, dec);
#define M_ENUM(key, sub, field, itemsFn, _comment)   j[key] = enumText(p.sub.field, itemsFn());
#define M_FARR(key, sub, field, cnt, dec, _comment)  j[key] = floatsToJson(p.sub.field, cnt, dec);
#define M_SFARR(key, sub, field, cnt, dec, _comment) j[key] = floatsToJson(reinterpret_cast<const float*>(p.sub.field), cnt, dec);
#define M_SSARR(key, sub, field, cnt, _comment)      j[key] = shortsToJson(reinterpret_cast<const short*>(p.sub.field), cnt);
#define M_EARR(key, sub, field, cnt, itemsFn, _comment) j[key] = enumsToJson(p.sub.field, cnt, itemsFn());

    PA_PARAM_FIELDS(M)

#undef M_INT
#undef M_FLOAT
#undef M_ENUM
#undef M_FARR
#undef M_SFARR
#undef M_SSARR
#undef M_EARR

    // 声束描述符（仅保存非零项以减少文件体积）
    {
        QJsonArray beamArr;
        for (int i = 0; i < 128; ++i) {
            const auto &b = p.global.beams[i];
            if (b.x0 == 0 && b.y0 == 0 && b.x1 == 0 && b.y1 == 0) continue;
            QJsonObject bo;
            bo["i"]  = i;
            bo["x0"] = b.x0; bo["y0"] = b.y0;
            bo["x1"] = b.x1; bo["y1"] = b.y1;
            beamArr.append(bo);
        }
        if (!beamArr.isEmpty())
            j["beams"] = beamArr;
    }

    // 参数注释字典 — 从 PA_PARAM_FIELDS 自动生成
    QJsonObject comments;
#define M_INT(key, _sub, _field, comment)                comments[key] = QString::fromUtf8(comment);
#define M_FLOAT(key, _sub, _field, _dec, comment)        comments[key] = QString::fromUtf8(comment);
#define M_ENUM(key, _sub, _field, _itemsFn, comment)     comments[key] = QString::fromUtf8(comment);
#define M_FARR(key, _sub, _field, _cnt, _dec, comment)   comments[key] = QString::fromUtf8(comment);
#define M_SFARR(key, _sub, _field, _cnt, _dec, comment)  comments[key] = QString::fromUtf8(comment);
#define M_SSARR(key, _sub, _field, _cnt, comment)        comments[key] = QString::fromUtf8(comment);
#define M_EARR(key, _sub, _field, _cnt, _itemsFn, comment) comments[key] = QString::fromUtf8(comment);

    PA_PARAM_FIELDS(M)

#undef M_INT
#undef M_FLOAT
#undef M_ENUM
#undef M_FARR
#undef M_SFARR
#undef M_SSARR
#undef M_EARR

    comments["beams"] = QString::fromUtf8("非零声束几何描述，i为编号，x0/y0和x1/y1为端点");
    j["_comments"] = comments;

    return j;
}

void ParamPage::deserializeParams(const QJsonObject &j)
{
    auto &p = m_params;

    // ── X-Macro 展开: 反序列化所有 PA_PARAM_FIELDS 字段 ──
#define M_INT(key, sub, field, _comment)             if (j.contains(key)) p.sub.field = j[key].toInt();
#define M_FLOAT(key, sub, field, dec, _comment)      if (j.contains(key)) p.sub.field = static_cast<float>(j[key].toDouble());
#define M_ENUM(key, sub, field, itemsFn, _comment)   if (j.contains(key)) p.sub.field = enumIndex(j[key], itemsFn(), p.sub.field);
#define M_FARR(key, sub, field, cnt, dec, _comment)  if (j.contains(key)) jsonToFloats(j[key].toArray(), p.sub.field, cnt);
#define M_SFARR(key, sub, field, cnt, dec, _comment) if (j.contains(key)) jsonToFloats(j[key].toArray(), reinterpret_cast<float*>(p.sub.field), cnt);
#define M_SSARR(key, sub, field, cnt, _comment)      if (j.contains(key)) jsonToShorts(j[key].toArray(), reinterpret_cast<short*>(p.sub.field), cnt);
#define M_EARR(key, sub, field, cnt, itemsFn, _comment) if (j.contains(key)) jsonToEnums(j[key].toArray(), p.sub.field, cnt, itemsFn());

    PA_PARAM_FIELDS(M)

#undef M_INT
#undef M_FLOAT
#undef M_ENUM
#undef M_FARR
#undef M_SFARR
#undef M_SSARR
#undef M_EARR

    // 声束描述符
    if (j.contains("beams")) {
        QJsonArray arr = j["beams"].toArray();
        for (const auto &v : arr) {
            QJsonObject bo = v.toObject();
            int i = bo["i"].toInt();
            if (i < 0 || i >= 128) continue;
            p.global.beams[i].x0 = bo["x0"].toInt();
            p.global.beams[i].y0 = bo["y0"].toInt();
            p.global.beams[i].x1 = bo["x1"].toInt();
            p.global.beams[i].y1 = bo["y1"].toInt();
        }
    }
}

// ═══════════════════════════════════════════════════════════
// 从 m_params 回写所有 UI 控件
// ═══════════════════════════════════════════════════════════

void ParamPage::syncUiFromParams()
{
    const auto &p = m_params;

    // 批量阻断信号，避免触发硬件下发
    QSignalBlocker b1(m_voltCombo);
    QSignalBlocker b2(m_pulseWidthSpin);
    QSignalBlocker b3(m_prfSpin);
    QSignalBlocker b4(m_rangeSpin);
    QSignalBlocker b5(m_tempCorrectCombo);
    QSignalBlocker b6(m_aDataLenCombo);
    QSignalBlocker b7(m_aGainSpin);
    QSignalBlocker b8(m_dGainSpin);
    QSignalBlocker b9(m_beamNoSpin);
    QSignalBlocker ba(m_rectifyCombo);
    QSignalBlocker bb(m_filterCombo);
    QSignalBlocker bc(m_videoCombo);
    QSignalBlocker bd(m_gateSelCombo);
    QSignalBlocker be(m_gateStartSpin);
    QSignalBlocker bf(m_gateWidthSpin);
    QSignalBlocker bg(m_gateThreshSpin);
    QSignalBlocker bh(m_gateMeasureCombo);
    QSignalBlocker bi(m_gateAlarmCombo);
    QSignalBlocker bj(m_gateTraceCombo);
    QSignalBlocker bk(m_alarmSoundCombo);
    QSignalBlocker bl(m_probeTypeCombo);
    QSignalBlocker bm(m_probeFreqSpin);
    QSignalBlocker bn(m_probeCountSpin);
    QSignalBlocker bo(m_probePitchSpin);
    QSignalBlocker bp(m_wedgeEnableCombo);
    QSignalBlocker bq(m_wedgeTypeCombo);
    QSignalBlocker br(m_wedgeAngleSpin);
    QSignalBlocker bs(m_wedgeVelSpin);
    QSignalBlocker bt(m_wedgeHeightSpin);
    QSignalBlocker bu(m_materialCombo);
    QSignalBlocker bv(m_lVelSpin);
    QSignalBlocker bw(m_traceEnableCombo);

    // ── 发射 ──
    m_voltCombo->setCurrentIndex(p.tx.highVoltage);
    m_pulseWidthSpin->setValue(p.tx.pulseWidth);
    m_prfSpin->setValue(p.tx.prf);
    m_rangeSpin->setValue(static_cast<double>(p.tx.range));
    m_tempCorrectCombo->setCurrentIndex(p.tx.tempCorrect);
    m_aDataLenCombo->setCurrentIndex(p.tx.aDataLen);

    // ── 接收 ──
    m_aGainSpin->setValue(static_cast<double>(p.rx.aGain));
    m_dGainSpin->setValue(static_cast<double>(p.rx.dGain));
    m_beamNoSpin->setValue(p.rx.curBeam);
    m_rectifyCombo->setCurrentIndex(p.rx.rectify);
    m_filterCombo->setCurrentIndex(p.rx.filter);
    m_videoCombo->setCurrentIndex(p.rx.video);

    // ── 闸门 ──
    m_gateSelCombo->setCurrentIndex(p.gate.gateSelect);
    m_gateStartSpin->setValue(static_cast<double>(p.gate.gateStart[p.gate.gateSelect]));
    m_gateWidthSpin->setValue(static_cast<double>(p.gate.gateWidth[p.gate.gateSelect]));
    m_gateThreshSpin->setValue(static_cast<double>(p.gate.gateThreshold[p.gate.gateSelect]));
    m_gateMeasureCombo->setCurrentIndex(p.gate.gateMeasure[p.gate.gateSelect]);
    m_gateAlarmCombo->setCurrentIndex(p.gate.gateAlarm[p.gate.gateSelect]);
    m_gateTraceCombo->setCurrentIndex(p.gate.gateTrace[p.gate.gateSelect]);
    m_alarmSoundCombo->setCurrentIndex(p.gate.alarmSound);

    // ── 探头 ──
    m_probeTypeCombo->setCurrentIndex(p.probe.probeType);
    m_probeFreqSpin->setValue(static_cast<double>(p.probe.probeFreq));
    m_probeCountSpin->setValue(p.probe.probeCount);
    m_probePitchSpin->setValue(static_cast<double>(p.probe.probePitch));

    // ── 楔块 ──
    m_wedgeEnableCombo->setCurrentIndex(p.wedge.wedgeEnable);
    m_wedgeTypeCombo->setCurrentIndex(p.wedge.wedgeType);
    m_wedgeAngleSpin->setValue(static_cast<double>(p.wedge.wedgeAngle));
    m_wedgeVelSpin->setValue(p.wedge.wedgeVelocity);
    m_wedgeHeightSpin->setValue(static_cast<double>(p.wedge.wedgeHeight));

    // ── 工件 ──
    m_materialCombo->setCurrentIndex(p.wp.material);
    m_lVelSpin->setValue(p.wp.lVelocity);
    m_traceEnableCombo->setCurrentIndex(p.wp.traceEnable);

    // ── 扫查（重建动态槽位控件）──
    if (m_scanTypeCombo) {
        m_scanTypeCombo->blockSignals(true);
        m_scanTypeCombo->setCurrentIndex(p.scan.scanType);
        m_scanTypeCombo->blockSignals(false);
    }
    onScanTypeChanged(p.scan.scanType);

    // 通知闸门同步
    emit gateParamsChanged();
}

// ── 文件目录辅助 ──
static QString paramsDir()  { QString d = QCoreApplication::applicationDirPath() + "/params";  QDir().mkpath(d); return d; }
static QString dataDir()    { QString d = QCoreApplication::applicationDirPath() + "/data";    QDir().mkpath(d); return d; }

bool ParamPage::initializeParams()
{
    const QString path = paramsDir() + "/default.json";
    QFile file(path);

    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly))
            return false;

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            return false;

        deserializeParams(document.object());
        syncUiFromParams();
        return true;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(serializeParams()).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}

void ParamPage::applyCurrentParams()
{
    onApplyLaw();
}

void ParamPage::onSaveParams()
{
    // Commit the visible gate editor before serializing all three gate records.
    const int gate = qBound(0, m_params.gate.gateSelect, 2);
    m_params.gate.gateStart[gate] = static_cast<float>(m_gateStartSpin->value());
    m_params.gate.gateWidth[gate] = static_cast<float>(m_gateWidthSpin->value());
    m_params.gate.gateThreshold[gate] = static_cast<float>(m_gateThreshSpin->value());
    m_params.gate.gateMeasure[gate] = m_gateMeasureCombo->currentIndex();
    m_params.gate.gateAlarm[gate] = m_gateAlarmCombo->currentIndex();
    m_params.gate.gateTrace[gate] = m_gateTraceCombo->currentIndex();

    QString path = QFileDialog::getSaveFileName(
        this, QString::fromUtf8("保存参数"), paramsDir(),
        QString::fromUtf8("参数文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;

    QJsonDocument doc(serializeParams());
    file.write(doc.toJson(QJsonDocument::Indented));
}

void ParamPage::onLoadParams()
{
    QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("调用参数"), paramsDir(),
        QString::fromUtf8("参数文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return;
    if (!doc.isObject()) return;

    deserializeParams(doc.object());
    syncUiFromParams();
    if (m_dispatcher && m_dispatcher->isConnected())
        applyCurrentParams();

    // 加载后自动展开参数面板，确保用户能看到更新后的值
    if (!m_stack->isVisible()) {
        m_activeRow = 0;  // 发射页
        m_nav->setCurrentRow(0);
        m_stack->setCurrentIndex(0);
        m_stack->show();
        setFixedWidth(kExpandedWidth);
        updateGeometry();
    }
    updateCScanButtons();
}

void ParamPage::updateCScanButtons()
{
    // 仅在展开且当前页为 C扫相关页（成像=8, 编码器=9, 分析=10）时可用
    const bool isCScanPage = m_stack->isVisible()
                             && m_activeRow >= 8 && m_activeRow <= 10;
    if (m_saveDataBtn)
        m_saveDataBtn->setEnabled(isCScanPage);
    if (m_replayDataBtn)
        m_replayDataBtn->setEnabled(isCScanPage);
}

void ParamPage::onSaveData()
{
    QString path = QFileDialog::getSaveFileName(
        this, QString::fromUtf8("保存C扫数据"), dataDir(),
        QString::fromUtf8("PA数据 (*.dat);;所有文件 (*)"));
    if (path.isEmpty()) return;
    emit saveDataRequested(path);
}

void ParamPage::onReplayData()
{
    QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("回放C扫数据"), dataDir(),
        QString::fromUtf8("C扫数据 (*.dat);;所有文件 (*)"));
    if (path.isEmpty()) return;
    emit replayDataRequested(path);
}

// ──────────────────────────────────────────────
// 主UI构建
// ──────────────────────────────────────────────

void ParamPage::setupUi()
{
    setObjectName("ParamPagePanel");
    setUpdatesEnabled(false);  // 批量构建，避免中间重绘

    auto *root = new QHBoxLayout(this);
    // 不要设置 margins —— stylesheet 的 border:1px 已经自动缩进内容区。
    // 再加 margin 会导致子控件溢出，覆盖右侧边框（Sidebar 170px > 可用 168px）。
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 左侧导航栏 ──
    auto *sidebar = new QFrame;
    sidebar->setObjectName("ParamSidebar");
    sidebar->setFixedWidth(kSidebarWidth);
    sidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    // 顶部标题
    auto *topLabel = new QLabel(QString::fromUtf8("参数设置"));
    topLabel->setObjectName("SidebarTitle");
    topLabel->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(topLabel);

    auto *subLabel = new QLabel(QString::fromUtf8("常规相控阵参数"));
    subLabel->setObjectName("PageLabel");
    subLabel->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(subLabel);

    // 导航列表（11项：8项参数 + 3项C扫）
    m_nav = new QListWidget;
    m_nav->setObjectName("ParamNav");
    const QStringList cats = {QString::fromUtf8("发射"), QString::fromUtf8("接收"), QString::fromUtf8("闸门"), QString::fromUtf8("探头"), QString::fromUtf8("楔块"), QString::fromUtf8("工件"), QString::fromUtf8("扫查"), QString::fromUtf8("校准"),
                               QString::fromUtf8("成像"), QString::fromUtf8("编码器"), QString::fromUtf8("分析")};
    for (const auto &c : cats)
        m_nav->addItem(c);
    m_nav->setCurrentRow(0);
    sideLayout->addWidget(m_nav, 1);

    // ── 底部操作按钮 ──
    auto *btnLayout = new QVBoxLayout;
    btnLayout->setContentsMargins(8, 6, 8, 8);
    btnLayout->setSpacing(5);

    auto *loadBtn = new QPushButton(QString::fromUtf8("调用参数"));
    loadBtn->setObjectName("LoadParamsButton");
    loadBtn->setCursor(Qt::PointingHandCursor);
    loadBtn->setFixedHeight(30);
    loadBtn->setStyleSheet(
        "QPushButton{background:#18536e;color:white;border:1px solid #3b7893;"
        "border-radius:4px;font-weight:600;font-size:13px;}"
        "QPushButton:hover{background:#126aa0;}"
    );
    connect(loadBtn, &QPushButton::clicked, this, &ParamPage::onLoadParams);

    auto *saveBtn = new QPushButton(QString::fromUtf8("保存参数"));
    saveBtn->setObjectName("SaveButton");
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setFixedHeight(30);
    connect(saveBtn, &QPushButton::clicked, this, &ParamPage::onSaveParams);

	// ── C扫数据按钮（默认置灰，在成像/编码器/分析页解锁）──
	m_saveDataBtn = new QPushButton(QString::fromUtf8("保存数据"));
	m_saveDataBtn->setObjectName("SaveDataButton");
	m_saveDataBtn->setCursor(Qt::PointingHandCursor);
	m_saveDataBtn->setFixedHeight(30);
	m_saveDataBtn->setEnabled(false);
	connect(m_saveDataBtn, &QPushButton::clicked, this, &ParamPage::onSaveData);

	m_replayDataBtn = new QPushButton(QString::fromUtf8("回放数据"));
	m_replayDataBtn->setObjectName("ReplayDataButton");
	m_replayDataBtn->setCursor(Qt::PointingHandCursor);
	m_replayDataBtn->setFixedHeight(30);
	m_replayDataBtn->setEnabled(false);
	connect(m_replayDataBtn, &QPushButton::clicked, this, &ParamPage::onReplayData);

    btnLayout->addWidget(loadBtn);
    btnLayout->addWidget(saveBtn);
	btnLayout->addWidget(m_saveDataBtn);
	btnLayout->addWidget(m_replayDataBtn);
    sideLayout->addLayout(btnLayout);

    root->addWidget(sidebar);

    // ── 右侧内容区 ──
    m_stack = new QStackedWidget;
    m_stack->setObjectName("ParamStack");
    m_stack->setMinimumWidth(0);
    m_stack->setMaximumWidth(kStackWidth);
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 构建所有页面（索引 0-10 对应导航 11 项）
    buildTransmitPage();    // 0 发射
    buildReceivePage();     // 1 接收
    buildGatePage();        // 2 闸门
    buildProbePage();       // 3 探头
    buildWedgePage();       // 4 楔块
    buildWorkpiecePage();   // 5 工件
    buildScanPage();        // 6 扫查
    buildTcgPage();         // 7 校准
    buildImagingPage();     // 8 成像 (C扫)
    buildEncoderPage();     // 9 编码器 (C扫)
    buildAnalysisPage();    // 10 分析 (C扫)

    // 默认隐藏参数内容区（仅显示左侧导航栏）
    m_stack->hide();
    setFixedWidth(kCollapsedWidth);

    // 导航联动：点击同一项=折叠/展开，不同项=切换并展开
    // 折叠=仅侧边栏170px，展开=侧边栏+内容区共600px
    // 批量更新避免中间重绘导致卡顿
    connect(m_nav, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        static QElapsedTimer collapseTimer;
        static int lastCollapsedRow = -1;

        const int row = m_nav->row(item);
        if (!m_stack->isVisible()
            && row == lastCollapsedRow
            && collapseTimer.isValid()
            && collapseTimer.elapsed() < kCollapseDebounceMs) {
            return;
        }

        if (row == m_activeRow && m_stack->isVisible()) {
            // 折叠
            lastCollapsedRow = row;
            collapseTimer.restart();
            m_activeRow = -1;
            setUpdatesEnabled(false);
            m_stack->hide();
            setFixedWidth(kCollapsedWidth);
            updateGeometry();
            setUpdatesEnabled(true);
        } else if (row != m_activeRow || !m_stack->isVisible()) {
            // 展开/切换
            const bool wasCollapsed = !m_stack->isVisible();
            lastCollapsedRow = -1;
            m_activeRow = row;
            m_stack->setUpdatesEnabled(false);
            if (m_stack->currentIndex() != row)
                m_stack->setCurrentIndex(row);
            if (wasCollapsed) {
                setFixedWidth(kExpandedWidth);
                m_stack->show();
                updateGeometry();
            }
            m_stack->setUpdatesEnabled(true);
        }
        updateCScanButtons();
    });

    root->addWidget(m_stack, 1);

    // ── 样式表 ──
    setStyleSheet(R"(
        ParamPage, #ParamPagePanel {
            background:#0a1520;
            border:1px solid #1a3a52;
            border-radius:6px;
            padding:1px;
        }
        QWidget {
            background:#08131d;
            color:#cfe7f4;
            font-family:"Microsoft YaHei";
            font-size:13px;
        }
        #ParamSidebar {
            background:#0a1a28;
            border:0;
        }
        #SidebarTitle {
            color:#f2fbff;
            font-size:16px;
            font-weight:700;
            padding:16px 0 8px 0;
            background:transparent;
        }
        #PageLabel {
            color:#5ea8d0;
            font-size:12px;
            padding:0 0 10px 0;
            background:transparent;
        }
        #ParamNav {
            background:transparent;
            border:0;
            outline:0;
            font-size:14px;
        }
        #ParamNav::item {
            padding:10px 16px;
            border-left:3px solid transparent;
            color:#a8c5d6;
        }
        #ParamNav::item:selected {
            background:#102940;
            border-left:3px solid #1688d8;
            color:#f2fbff;
        }
        #ParamNav::item:hover {
            background:#0e2338;
            color:#d5e9f5;
        }
        #ApplyLawButton {
            background:#0a72d6;
            color:white;
            border:1px solid #0a72d6;
            border-radius:5px;
            font-weight:700;
            font-size:14px;
        }
        #ApplyLawButton:hover {
            background:#0b88e8;
        }
        #SaveButton {
            background:#0a6e3b;
            color:white;
            border:1px solid #0a6e3b;
            border-radius:4px;
            font-weight:600;
        }
        #SaveButton:hover {
            background:#0b8a48;
        }
        #SaveDataButton {
            background:#18536e;
            color:white;
            border:1px solid #3b7893;
            border-radius:4px;
            font-weight:600;
        }
        #SaveDataButton:hover {
            background:#126aa0;
        }
        #SaveDataButton:disabled {
            background:#1a2832;
            color:#4a6a7a;
            border:1px solid #1a3a52;
        }
        #ReplayDataButton {
            background:#6e3a18;
            color:white;
            border:1px solid #935a3b;
            border-radius:4px;
            font-weight:600;
        }
        #ReplayDataButton:hover {
            background:#a05030;
        }
        #ReplayDataButton:disabled {
            background:#1a2832;
            color:#4a6a7a;
            border:1px solid #1a3a52;
        }
        #LoadButton {
            background:#2a3a4a;
            color:#d5e9f5;
            border:1px solid #3a5a72;
            border-radius:4px;
            font-weight:600;
        }
        #LoadButton:hover {
            background:#3a5a72;
        }
        #ParamStack {
            background:#08131d;
            border-left:1px solid #1a3a52;
        }
        #CategoryPage {
            background:transparent;
        }
        #CategoryTitle {
            color:#f2fbff;
            font-size:17px;
            font-weight:700;
            padding:14px 16px 6px 16px;
            background:transparent;
        }
        QGroupBox {
            background:#0a1b2b;
            border:1px solid #29445c;
            border-radius:8px;
            margin-top:10px;
            font-weight:600;
        }
        QGroupBox::title {
            subcontrol-origin:margin;
            left:14px;
            padding:0 6px;
            color:#dff5ff;
        }
        QLabel {
            color:#b9d3e2;
            background:transparent;
        }
        #StepCombo {
            background:#0c1825;
            color:#a8d0e8;
            border:1px solid #1a5a73;
            border-radius:4px;
            padding:2px 4px;
            font-size:12px;
            font-weight:600;
            min-width:70px;
        }
        #StepCombo:focus {
            border-color:#1688d8;
        }
        #StepCombo::drop-down {
            border:0;
            width:16px;
        }
        #StepCombo QAbstractItemView {
            background:#0c1f2e;
            color:#d5e9f5;
            selection-background-color:#0a72d6;
            border:1px solid #2a4a62;
        }
        QLineEdit, QDoubleSpinBox, QSpinBox, QComboBox {
            background:#101d2a;
            color:white;
            border:1px solid #355a73;
            border-radius:4px;
            padding:3px 8px;
            font-size:13px;
        }
        QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus {
            border-color:#1688d8;
        }
        QComboBox::drop-down {
            border:0;
            width:20px;
        }
        QComboBox QAbstractItemView {
            background:#0c1f2e;
            color:#d5e9f5;
            selection-background-color:#0a72d6;
            border:1px solid #2a4a62;
        }
        QPushButton {
            background:#18536e;
            color:white;
            border:1px solid #3b7893;
            border-radius:4px;
            padding:5px 12px;
            font-weight:600;
        }
        QPushButton:hover {
            background:#126aa0;
        }
        QScrollArea {
            background:transparent;
            border:0;
        }
        QScrollBar:vertical {
            background:#0a1a28;
            width:8px;
            border:0;
        }
        QScrollBar::handle:vertical {
            background:#2a4a62;
            border-radius:4px;
            min-height:30px;
        }
        QScrollBar::handle:vertical:hover {
            background:#3a6a82;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height:0;
        }
    )");

    setUpdatesEnabled(true);
}

ParamPage::ParamPage(QWidget *parent) : QFrame(parent)
{
    setObjectName("ParamPagePanel");
    // 不设置原生 QFrame 边框样式，由 stylesheet 统一绘制 border
    // setFrameStyle + setLineWidth 会与 stylesheet 的 border 冲突，导致右边框不显示
    setupUi();
}

void ParamPage::setDispatcher(ParameterDispatcher *dispatcher)
{
    m_dispatcher = dispatcher;
}
