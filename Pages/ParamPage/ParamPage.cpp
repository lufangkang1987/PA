#include "ParamPage.h"
#include "IDriver.h"
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

namespace {
constexpr int kSidebarWidth = 170;
constexpr int kStackWidth = 430;
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
    f->setContentsMargins(16, 18, 16, 16);
    f->setHorizontalSpacing(12);
    f->setVerticalSpacing(10);
    f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
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
    auto *page = createCategoryPage("发射参数");
    auto *content = page->property("contentWidget").value<QWidget*>();
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_voltCombo = makeCombo({"110 V", "40 V", "20 V"}, m_params.highVoltage);
    f->addRow("发射电压", m_voltCombo);

    m_pulseWidthSpin = makeIntSpin(30, 1250, m_params.pulseWidth, 10);
    m_pulseWidthSpin->setSuffix(" ns");
    f->addRow("脉冲宽度", wrapWithStepSelector(m_pulseWidthSpin, {"10", "50", "100"}, {10.0, 50.0, 100.0}, 1));

    m_prfSpin = makeIntSpin(25, 20000, m_params.prf, 100);
    m_prfSpin->setSuffix(" Hz");
    f->addRow("重复频率", wrapWithStepSelector(m_prfSpin, {"5", "100", "1000"}, {5.0, 100.0, 1000.0}, 1));

    m_rangeSpin = makeDoubleSpin(5.0, 1000.0, m_params.range, 0.1, "mm", 1);
    f->addRow("检测范围", wrapWithStepSelector(m_rangeSpin, {"0.1", "1.0", "10.0", "100.0"}, {0.1, 1.0, 10.0, 100.0}, 1));

    m_tempCorrectCombo = makeCombo({"关", "开"}, m_params.tempCorrect);
    f->addRow("温度补偿", m_tempCorrectCombo);

    m_aDataLenCombo = makeCombo({"100 点", "200 点", "400 点"}, m_params.aDataLen);
    f->addRow("A波长度", m_aDataLenCombo);

    // ── 硬件下发 ──
    connect(m_voltCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.highVoltage = v;
        if (m_driver) m_driver->setHighVoltage(v);
    });
    connect(m_pulseWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_params.pulseWidth = v;
        if (m_driver) m_driver->setPulseWidth(v);
    });
    connect(m_prfSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_params.prf = v;
        if (m_driver) m_driver->setPRF(v);
    });
    connect(m_rangeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_params.range = static_cast<float>(v);
        if (m_driver) m_driver->setRange(static_cast<float>(v));
    });
    connect(m_tempCorrectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.tempCorrect = v;
    });
    connect(m_aDataLenCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.aDataLen = v;
        if (m_driver) m_driver->setADataLen(v);
    });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildReceivePage()
{
    auto *page = createCategoryPage("接收参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    // 模拟增益: 步进 [0.1, 1.0, 6.0] dB, 默认1.0
    m_aGainSpin = makeDoubleSpin(0.0, 80.0, m_params.aGain, 0.1, "dB");
    f->addRow("模拟增益", wrapWithStepSelector(m_aGainSpin, {"0.1", "1.0", "6.0"}, {0.1, 1.0, 6.0}, 1));

    // 数字增益: 步进 [0.1, 1.0, 6.0] dB, 默认1.0
    m_dGainSpin = makeDoubleSpin(-12.0, 12.0, m_params.dGain, 0.1, "dB");
    f->addRow("数字增益", wrapWithStepSelector(m_dGainSpin, {"0.1", "1.0", "6.0"}, {0.1, 1.0, 6.0}, 1));

    // 声束号: 步进 [1, 10], 默认1 (细=中=1, 粗=10)
    m_beamNoSpin = makeIntSpin(0, 127, m_params.curBeam, 1);
    f->addRow("声束号", wrapWithStepSelector(m_beamNoSpin, {"1", "10"}, {1.0, 10.0}, 0));

    m_rectifyCombo = makeCombo({"全波", "正半波", "负半波"}, m_params.rectify);
    f->addRow("检波方式", m_rectifyCombo);

    m_filterCombo = makeCombo({
        "0.5-20.0 MHz", "0.5-15.0 MHz", "0.5-10.0 MHz", "0.5-5.0 MHz",
        "1.0-20.0 MHz", "3.0-20.0 MHz", "5.0-20.0 MHz", "7.0-20.0 MHz",
        "10.0-20.0 MHz", "1.0 MHz", "2.5 MHz", "4.0 MHz",
        "5.0 MHz", "7.5 MHz", "10.0 MHz", "15.0 MHz"
    }, m_params.filter);
    f->addRow("滤波器", m_filterCombo);

    m_videoCombo = makeCombo({"无", "1", "2", "3", "4", "平滑"}, m_params.video);
    f->addRow("视频检波", m_videoCombo);

    // ── 硬件下发 ──
    connect(m_aGainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_params.aGain = static_cast<float>(v);
        if (m_driver) m_driver->setAnalogGain(static_cast<float>(v));
        emit beamInfoChanged(m_params.curBeam, v);
    });
    connect(m_dGainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_params.dGain = static_cast<float>(v);
        if (m_driver) m_driver->setDigitalGain(static_cast<float>(v));
    });
    connect(m_beamNoSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_params.curBeam = v;
        emit beamInfoChanged(v, m_params.aGain);
    });
    connect(m_rectifyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.rectify = v;
        if (m_driver) m_driver->setRectify(v);
    });
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.filter = v;
        if (m_driver) m_driver->setFilter(v);
    });
    connect(m_videoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params.video = v;
        if (m_driver) {
            if (v < 5) {
                m_driver->setASmooth(false);
                m_driver->setVideoDetect(true);
            } else {
                m_driver->setVideoDetect(false);
                m_driver->setASmooth(true);
            }
        }
    });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildGatePage()
{
    auto *page = createCategoryPage("闸门参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_gateSelCombo = makeCombo({"A 闸门", "B 闸门", "C 闸门"}, m_params.gateSelect);
    connect(m_gateSelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onGateSelectChanged);
    f->addRow("闸门选择", m_gateSelCombo);

    // 闸门起位: 步进 [0.1, 1.0, 10.0] mm, 默认1.0
    m_gateStartSpin = makeDoubleSpin(0.0, 999.0, m_params.gateStart[0], 0.1, "mm");
    connect(m_gateStartSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ParamPage::onGateParamChanged);
    f->addRow("闸门起位", wrapWithStepSelector(m_gateStartSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 闸门宽度: 步进 [0.1, 1.0, 10.0] mm, 默认1.0
    m_gateWidthSpin = makeDoubleSpin(0.0, 999.0, m_params.gateWidth[0], 0.1, "mm");
    connect(m_gateWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ParamPage::onGateParamChanged);
    f->addRow("闸门宽度", wrapWithStepSelector(m_gateWidthSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 闸门高度: 步进 [0.1, 1.0, 10.0] %, 默认1.0
    m_gateThreshSpin = makeDoubleSpin(0.0, 99.0, m_params.gateThreshold[0], 0.1, "%");
    connect(m_gateThreshSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ParamPage::onGateParamChanged);
    f->addRow("闸门高度", wrapWithStepSelector(m_gateThreshSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    m_gateMeasureCombo = makeCombo({"峰值", "前沿"}, 0);
    f->addRow("测量方式", m_gateMeasureCombo);

    m_alarmCombo = makeCombo({"关", "A 门", "B 门", "AB 门"}, m_params.alarmSound);
    f->addRow("报警声", m_alarmCombo);

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildProbePage()
{
    auto *page = createCategoryPage("探头参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_probeTypeCombo = makeCombo({"自定义", "2.5L16", "5.0S64"}, m_params.probeType);
    connect(m_probeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onProbeTypeChanged);
    f->addRow("探头型号", m_probeTypeCombo);

    // 探头频率: 步进 [0.1, 1.0] MHz (细=中=0.1, 粗=1.0)
    m_probeFreqSpin = makeDoubleSpin(0.2, 20.0, m_params.probeFreq, 0.1, "MHz");
    f->addRow("探头频率", wrapWithStepSelector(m_probeFreqSpin, {"0.1", "1.0"}, {0.1, 1.0}, 0));

    // 阵元数: 步进 [1, 10] (细=中=1, 粗=10)
    m_probeCountSpin = makeIntSpin(1, 128, m_params.probeCount, 1);
    f->addRow("阵元数", wrapWithStepSelector(m_probeCountSpin, {"1", "10"}, {1.0, 10.0}, 0));

    // 阵元间距: 步进 [0.01, 0.1, 1.0] mm, 默认0.1
    m_probePitchSpin = makeDoubleSpin(0.10, 15.00, m_params.probePitch, 0.01, "mm", 2);
    f->addRow("阵元间距", wrapWithStepSelector(m_probePitchSpin, {"0.01", "0.1", "1.0"}, {0.01, 0.1, 1.0}, 1));

    // 同步到 m_params（下发由"应用法则"统一触发）
    connect(m_probeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.probeType = v; });
    connect(m_probeFreqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params.probeFreq = static_cast<float>(v); });
    connect(m_probeCountSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_params.probeCount = v; });
    connect(m_probePitchSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params.probePitch = static_cast<float>(v); });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildWedgePage()
{
    auto *page = createCategoryPage("楔块参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_wedgeEnableCombo = makeCombo({"否", "是"}, m_params.wedgeEnable);
    f->addRow("楔块启用", m_wedgeEnableCombo);

    m_wedgeTypeCombo = makeCombo({"自定义", "GW-PA"}, m_params.wedgeType);
    connect(m_wedgeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onWedgeTypeChanged);
    f->addRow("楔块型号", m_wedgeTypeCombo);

    // 楔块角度: 步进 [0.1, 1.0, 10.0]°, 默认1.0
    m_wedgeAngleSpin = makeDoubleSpin(0.0, 89.0, m_params.wedgeAngle, 0.1, QString::fromUtf8("\u00B0"));
    f->addRow("楔块角度", wrapWithStepSelector(m_wedgeAngleSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 楔块声速: 步进 [1, 10, 100] m/s, 默认10
    m_wedgeVelSpin = makeIntSpin(1000, 9000, m_params.wedgeVelocity, 10);
    m_wedgeVelSpin->setSuffix(" m/s");
    f->addRow("楔块声速", wrapWithStepSelector(m_wedgeVelSpin, {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));

    // 楔块高度: 步进 [0.1, 1.0, 10.0] mm, 默认1.0
    m_wedgeHeightSpin = makeDoubleSpin(0.1, 100.0, m_params.wedgeHeight, 0.1, "mm");
    f->addRow("楔块高度", wrapWithStepSelector(m_wedgeHeightSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 同步到 m_params
    connect(m_wedgeEnableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.wedgeEnable = v; });
    connect(m_wedgeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.wedgeType = v; });
    connect(m_wedgeAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params.wedgeAngle = static_cast<float>(v); });
    connect(m_wedgeVelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_params.wedgeVelocity = v; });
    connect(m_wedgeHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params.wedgeHeight = static_cast<float>(v); });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildWorkpiecePage()
{
    auto *page = createCategoryPage("工件参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    m_materialCombo = makeCombo({"钢纵波", "钢横波"}, m_params.material);
    connect(m_materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onMaterialChanged);
    f->addRow("材料", m_materialCombo);

    // 工件声速: 步进 [1, 10, 100] m/s, 默认10
    m_lVelSpin = makeIntSpin(1000, 9000, m_params.lVelocity, 10);
    m_lVelSpin->setSuffix(" m/s");
    f->addRow("声速", wrapWithStepSelector(m_lVelSpin, {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));

    m_traceEnableCombo = makeCombo({"否", "是"}, m_params.traceEnable);
    f->addRow("跟踪启用", m_traceEnableCombo);

    connect(m_materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.material = v; });
    connect(m_lVelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_params.lVelocity = v; });
    connect(m_traceEnableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params.traceEnable = v; });

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildScanPage()
{
    auto *page = createCategoryPage("扫查参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    // 槽位0: 扫查方式（固定）
    m_scanTypeCombo = makeCombo({"S 扫", "L 扫"}, m_params.scanType);
    connect(m_scanTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParamPage::onScanTypeChanged);
    f->addRow("扫查方式", m_scanTypeCombo);

    // 槽位1-6: 动态控件，根据 scanType 变化
    for (int i = 1; i <= 6; ++i) {
        m_scanLabels[i] = new QLabel;
        m_scanWidgets[i] = new QWidget;
        f->addRow(m_scanLabels[i], m_scanWidgets[i]);
    }

    // 初始填充
    onScanTypeChanged(m_params.scanType);

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildTcgPage()
{
    auto *page = createCategoryPage("校准内容");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    // 校准项目: 声速 / 声束延迟 / ACG / TCG
    auto *calibItem = makeCombo({"声速", "声束延迟", "ACG", "TCG"}, m_params.calibItem);
    f->addRow("校准项目", calibItem);

    // 实际距离: 10~1000 mm, 步进 0.1/1.0/10.0
    auto *realDist = makeDoubleSpin(10.0, 1000.0, m_params.realDistance, 0.1, "mm", 1);
    f->addRow("实际距离", wrapWithStepSelector(realDist, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // 声束延迟: 0~100 us, 步进 0.1/1.0/10.0
    auto *beamDelay = makeDoubleSpin(0.0, 100.0, m_params.beamDelay, 0.1, "us", 1);
    f->addRow("声束延迟", wrapWithStepSelector(beamDelay, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    // TCG系数: 0~0.5, 步进 0.001/0.01/0.1
    auto *tcgCoeff = makeDoubleSpin(0.0, 0.5, m_params.tcgCoeff, 0.001, "", 3);
    f->addRow("TCG系数", wrapWithStepSelector(tcgCoeff, {"0.001", "0.01", "0.1"}, {0.001, 0.01, 0.1}, 1));

    // TCG参考点: 共 9 个参考点
    auto *refLabel = new QLabel("共 9 个参考点");
    f->addRow("TCG参考点", refLabel);

    // 校准启用: 关闭 / ACG
    auto *calibEnable = makeCombo({"关闭", "ACG"}, m_params.calibEnable);
    f->addRow("校准启用", calibEnable);

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

// ══════════════════════════════════════════════
// C扫子页（从 CScanPage 合并）
// ══════════════════════════════════════════════

void ParamPage::buildImagingPage()
{
    auto *page = createCategoryPage("C扫成像参数");
    auto *content = page->property("contentWidget").value<QWidget*>();
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    f->addRow("采集线 X1", wrapWithStepSelector(makeIntSpin(0, 511, m_params.imgLineX1, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("采集线 X2", wrapWithStepSelector(makeIntSpin(0, 511, m_params.imgLineX2, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("采集线 Y1", wrapWithStepSelector(makeIntSpin(0, 399, m_params.imgLineY1, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("采集线 Y2", wrapWithStepSelector(makeIntSpin(0, 399, m_params.imgLineY2, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("C扫增量", wrapWithStepSelector(makeDoubleSpin(0.1, 5.0, m_params.degPerPoint, 0.1, "mm/d"), {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    layout->addWidget(form);

    // ── 开始扫描按钮 ──
    auto *btnFrame = new QFrame;
    btnFrame->setObjectName("ScanButtonFrame");
    auto *btnLayout = new QVBoxLayout(btnFrame);
    btnLayout->setContentsMargins(16, 12, 16, 12);
    btnLayout->setSpacing(8);

    m_scanBtn = new QPushButton("开始扫描");
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

    auto *hintLabel = new QLabel("点击按钮开始/停止 C 扫描数据采集");
    hintLabel->setObjectName("HintLabel");
    hintLabel->setAlignment(Qt::AlignCenter);
    btnLayout->addWidget(hintLabel);

    layout->addWidget(btnFrame);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildEncoderPage()
{
    auto *page = createCategoryPage("编码器参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    f->addRow("成像方向", makeCombo({"正向", "反向"}, m_params.direction));
    f->addRow("编码精度", wrapWithStepSelector(makeDoubleSpin(0.001, 10.0, m_params.coderDeg, 0.01, "mm/p", 3), {"0.001", "0.01", "0.1"}, {0.001, 0.01, 0.1}, 1));
    f->addRow("校准距离", wrapWithStepSelector(makeDoubleSpin(1.0, 200.0, m_params.checkDistance, 0.1, "mm"), {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    auto *calBtn = new QPushButton("开始 / 结束校准");
    calBtn->setFixedHeight(36);
    calBtn->setCursor(Qt::PointingHandCursor);
    calBtn->setStyleSheet(
        "QPushButton{background:#18536e;color:white;border:1px solid #3b7893;"
        "border-radius:4px;padding:0 16px;font-weight:600;font-size:14px;}"
        "QPushButton:hover{background:#126aa0;}"
    );
    f->addRow("", calBtn);

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::buildAnalysisPage()
{
    auto *page = createCategoryPage("C扫分析参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    f->addRow("测量线 X1", wrapWithStepSelector(makeIntSpin(0, 924, m_params.anaLineX1, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("测量线 X2", wrapWithStepSelector(makeIntSpin(0, 924, m_params.anaLineX2, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("测量线 Y1", wrapWithStepSelector(makeIntSpin(0, 400, m_params.anaLineY1, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("测量线 Y2", wrapWithStepSelector(makeIntSpin(0, 400, m_params.anaLineY2, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));

    const QString btnStyle = QString(
        "QPushButton{background:#18536e;color:white;border:1px solid #3b7893;"
        "border-radius:4px;padding:0 16px;font-weight:600;font-size:14px;}"
        "QPushButton:hover{background:#126aa0;}"
    );

    auto *pageBtn = new QPushButton("C扫翻页");
    pageBtn->setFixedHeight(36);
    pageBtn->setCursor(Qt::PointingHandCursor);
    pageBtn->setStyleSheet(btnStyle);
    f->addRow("", pageBtn);

    auto *exitBtn = new QPushButton("退出回放");
    exitBtn->setFixedHeight(36);
    exitBtn->setCursor(Qt::PointingHandCursor);
    exitBtn->setStyleSheet(btnStyle);
    f->addRow("", exitBtn);

    layout->addWidget(form);
    layout->addStretch();
    m_stack->addWidget(page);
}

void ParamPage::onScanButtonClicked()
{
    m_scanning = !m_scanning;
    if (m_scanning) {
        m_scanBtn->setText("停止扫描");
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
        m_scanBtn->setText("开始扫描");
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
                    case 1: wrappedField = wrapWithStepSelector(makeIntSpin(1, 128, m_params.eleStart, 1), {"1"}, {1.0}, 0); break;
                    case 2: wrappedField = wrapWithStepSelector(makeIntSpin(1, 128, m_params.eleEnd, 1), {"1"}, {1.0}, 0); break;
                    case 3: wrappedField = wrapWithStepSelector(makeIntSpin(1, 16, m_params.eleAperture, 1), {"1"}, {1.0}, 0); break;
                    case 4: wrappedField = wrapWithStepSelector(makeDoubleSpin(-89.0, 89.0, m_params.angleFrom, 1.0, "\u00B0"), {"1.0", "10.0"}, {1.0, 10.0}, 0); break;
                    case 5: wrappedField = wrapWithStepSelector(makeDoubleSpin(-89.0, 89.0, m_params.angleTo, 1.0, "\u00B0"), {"1.0", "10.0"}, {1.0, 10.0}, 0); break;
                    case 6: wrappedField = wrapWithStepSelector(makeDoubleSpin(2.0, 1000.0, m_params.focus, 0.1, "mm"), {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1); break;
                }
                break;
            case 1: // L扫
                switch (i) {
                    case 1: wrappedField = wrapWithStepSelector(makeIntSpin(1, 128, m_params.eleStart, 1), {"1"}, {1.0}, 0); break;
                    case 2: wrappedField = wrapWithStepSelector(makeIntSpin(1, 128, m_params.eleEnd, 1), {"1"}, {1.0}, 0); break;
                    case 3: wrappedField = wrapWithStepSelector(makeIntSpin(1, 16, m_params.eleAperture, 1), {"1"}, {1.0}, 0); break;
                    case 4: wrappedField = wrapWithStepSelector(makeDoubleSpin(-89.0, 89.0, m_params.angle, 1.0, "\u00B0"), {"1.0", "10.0"}, {1.0, 10.0}, 0); break;
                    case 5: wrappedField = wrapWithStepSelector(makeDoubleSpin(2.0, 1000.0, m_params.focus, 0.1, "mm"), {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1); break;
                }
                break;
        }
        if (wrappedField) {
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
    m_params.gateSelect = idx;

    // 阻断 SpinBox 信号，防止切闸门时误触发 onGateParamChanged
    m_gateStartSpin->blockSignals(true);
    m_gateWidthSpin->blockSignals(true);
    m_gateThreshSpin->blockSignals(true);

    m_gateStartSpin->setValue(m_params.gateStart[idx]);
    m_gateWidthSpin->setValue(m_params.gateWidth[idx]);
    m_gateThreshSpin->setValue(m_params.gateThreshold[idx]);

    m_gateStartSpin->blockSignals(false);
    m_gateWidthSpin->blockSignals(false);
    m_gateThreshSpin->blockSignals(false);

    // 通知 HomePage 更新活跃闸门
    emit gateParamsChanged();
}

void ParamPage::onGateParamChanged()
{
    const int g = m_params.gateSelect;
    const char gateName = 'A' + g;
    m_params.gateStart[g]     = static_cast<float>(m_gateStartSpin->value());
    m_params.gateWidth[g]     = static_cast<float>(m_gateWidthSpin->value());
    m_params.gateThreshold[g] = static_cast<float>(m_gateThreshSpin->value());

    // 下发到硬件
    if (m_driver)
        m_driver->setGate(gateName,
                          m_params.gateStart[g],
                          m_params.gateWidth[g],
                          m_params.gateThreshold[g],
                          m_params.gateMeasure[g] == 0 ? "peak" : "edge");

    emit gateParamsChanged();
}

void ParamPage::onGateDragged(int gate, float start, float threshold)
{
    if (gate < 0 || gate > 2) return;
    const char gn = 'A' + gate;

    // 更新 m_params
    m_params.gateStart[gate]     = start;
    m_params.gateThreshold[gate] = threshold;

    // 如果拖拽的闸门与当前选中的不同，静默切换（不触发 onGateSelectChanged）
    if (gate != m_params.gateSelect) {
        m_params.gateSelect = gate;
        m_gateSelCombo->blockSignals(true);
        m_gateSelCombo->setCurrentIndex(gate);
        m_gateSelCombo->blockSignals(false);
    }

    // 更新 SpinBox（阻断信号，避免重复触发）
    m_gateStartSpin->blockSignals(true);
    m_gateWidthSpin->blockSignals(true);
    m_gateThreshSpin->blockSignals(true);
    m_gateStartSpin->setValue(start);
    m_gateWidthSpin->setValue(m_params.gateWidth[gate]);
    m_gateThreshSpin->setValue(threshold);
    m_gateStartSpin->blockSignals(false);
    m_gateWidthSpin->blockSignals(false);
    m_gateThreshSpin->blockSignals(false);

    // 下发硬件
    if (m_driver)
        m_driver->setGate(gn, start, m_params.gateWidth[gate], threshold,
                          m_params.gateMeasure[gate] == 0 ? "peak" : "edge");

    emit gateParamsChanged();
}

void ParamPage::getGateParams(int gate, bool &enabled, float &start, float &width,
                              float &threshold) const
{
    if (gate < 0 || gate > 2) return;
    enabled   = true;  // 默认启用，后续可通过 alarmSound 控制
    start     = m_params.gateStart[gate];
    width     = m_params.gateWidth[gate];
    threshold = m_params.gateThreshold[gate];
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
    m_params.curBeam = beam;
    if (m_beamNoSpin) {
        m_beamNoSpin->blockSignals(true);
        m_beamNoSpin->setValue(beam);
        m_beamNoSpin->blockSignals(false);
    }
    emit beamInfoChanged(beam, m_params.aGain);
}

void ParamPage::onApplyLaw()
{
    if (!m_driver) return;

    // ── 1. 同步扫查几何参数到 Driver（不单独发命令，由 setScanType 统一构造 JSON）──
    m_driver->setVelocity(static_cast<float>(m_params.lVelocity));
    m_driver->setProbeGeometry(m_params.probeCount, m_params.probeFreq, m_params.probePitch);
    m_driver->setElementGeometry(m_params.eleStart, m_params.eleEnd, m_params.eleAperture);

    if (m_params.scanType == 0)  // S 扫
        m_driver->setSscanAngles(m_params.angleFrom, m_params.angleTo);
    else                         // L 扫
        m_driver->setLscanAngle(m_params.angle);

    m_driver->setFocusMm(m_params.focus);
    m_driver->setWedgeGeometry(m_params.wedgeEnable != 0, m_params.wedgeAngle,
                               m_params.wedgeVelocity, m_params.wedgeHeight);

    // ── 2. 下发扫查类型（触发硬件计算聚焦法则）──
    // setScanType 内部会自动处理采集的停止/重启
    m_driver->setScanType(m_params.scanType);

    // ── 3. 重发独立参数（确保硬件状态与 UI 一致）──
    m_driver->setAnalogGain(m_params.aGain);
    m_driver->setDigitalGain(m_params.dGain);
    m_driver->setHighVoltage(m_params.highVoltage);
    m_driver->setPulseWidth(m_params.pulseWidth);
    m_driver->setPRF(m_params.prf);
    m_driver->setRange(m_params.range);
    m_driver->setRectify(m_params.rectify);
    m_driver->setFilter(m_params.filter);
    m_driver->setADataLen(m_params.aDataLen);

    // 视频检波 / 平滑
    if (m_params.video < 5) {
        m_driver->setASmooth(false);
        m_driver->setVideoDetect(true);
    } else {
        m_driver->setVideoDetect(false);
        m_driver->setASmooth(true);
    }

    // ── 4. 重发闸门参数 ──
    static const char gateNames[] = {'A', 'B', 'C'};
    for (int g = 0; g < 3; ++g) {
        m_driver->setGate(gateNames[g],
                          m_params.gateStart[g],
                          m_params.gateWidth[g],
                          m_params.gateThreshold[g],
                          m_params.gateMeasure[g] == 0 ? "peak" : "edge");
    }

}

// ═══════════════════════════════════════════════════════════
// 参数序列化 / 反序列化
// ═══════════════════════════════════════════════════════════

static QJsonArray floatsToJson(const float *arr, int n)
{
    QJsonArray a;
    for (int i = 0; i < n; ++i) a.append(static_cast<double>(arr[i]));
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

QJsonObject ParamPage::serializeParams() const
{
    const auto &p = m_params;
    QJsonObject j;

    // 发射
    j["highVoltage"]   = p.highVoltage;
    j["pulseWidth"]    = p.pulseWidth;
    j["prf"]           = p.prf;
    j["range"]         = static_cast<double>(p.range);
    j["tempCorrect"]   = p.tempCorrect;
    j["aDataLen"]      = p.aDataLen;

    // 接收
    j["aGain"]         = static_cast<double>(p.aGain);
    j["dGain"]         = static_cast<double>(p.dGain);
    j["curBeam"]       = p.curBeam;
    j["rectify"]       = p.rectify;
    j["filter"]        = p.filter;
    j["video"]         = p.video;

    // 闸门
    j["gateSelect"]    = p.gateSelect;
    j["gateStart"]     = floatsToJson(p.gateStart, 3);
    j["gateWidth"]     = floatsToJson(p.gateWidth, 3);
    j["gateThreshold"] = floatsToJson(p.gateThreshold, 3);
    j["gateMeasure"]   = intsToJson(p.gateMeasure, 3);
    j["alarmSound"]    = p.alarmSound;

    // 探头
    j["probeType"]     = p.probeType;
    j["probeFreq"]     = static_cast<double>(p.probeFreq);
    j["probeCount"]    = p.probeCount;
    j["probePitch"]    = static_cast<double>(p.probePitch);

    // 楔块
    j["wedgeEnable"]   = p.wedgeEnable;
    j["wedgeType"]     = p.wedgeType;
    j["wedgeAngle"]    = static_cast<double>(p.wedgeAngle);
    j["wedgeVelocity"] = p.wedgeVelocity;
    j["wedgeHeight"]   = static_cast<double>(p.wedgeHeight);

    // 工件
    j["material"]      = p.material;
    j["lVelocity"]     = p.lVelocity;
    j["traceEnable"]   = p.traceEnable;

    // 扫查
    j["scanType"]      = p.scanType;
    j["eleStart"]      = p.eleStart;
    j["eleEnd"]        = p.eleEnd;
    j["eleAperture"]   = p.eleAperture;
    j["angleFrom"]     = static_cast<double>(p.angleFrom);
    j["angleTo"]       = static_cast<double>(p.angleTo);
    j["angle"]         = static_cast<double>(p.angle);
    j["focus"]         = static_cast<double>(p.focus);

    // 成像 (C扫)
    j["imgLineX1"]     = p.imgLineX1;
    j["imgLineX2"]     = p.imgLineX2;
    j["imgLineY1"]     = p.imgLineY1;
    j["imgLineY2"]     = p.imgLineY2;
    j["degPerPoint"]   = static_cast<double>(p.degPerPoint);

    // 编码器
    j["direction"]     = p.direction;
    j["coderDeg"]      = static_cast<double>(p.coderDeg);
    j["checkDistance"] = static_cast<double>(p.checkDistance);

    // 校准 (TCG)
    j["calibItem"]     = p.calibItem;
    j["realDistance"]  = static_cast<double>(p.realDistance);
    j["beamDelay"]     = static_cast<double>(p.beamDelay);
    j["tcgCoeff"]      = static_cast<double>(p.tcgCoeff);
    j["calibEnable"]   = p.calibEnable;
    j["tcgStart"]      = p.tcgStart;
    j["tcgEnd"]        = p.tcgEnd;
    j["acgSwitch"]     = p.acgSwitch;
    j["tcgSwitch"]     = p.tcgSwitch;
    j["acgValue"]      = floatsToJson(p.acgValue, 128);
    j["tcgPointX"]     = shortsToJson(reinterpret_cast<const short*>(p.tcgPointX), 10 * 128);
    j["tcgPointValue"] = floatsToJson(reinterpret_cast<const float*>(p.tcgPointValue), 10 * 128);

    // ── 以下为 MFC 补齐字段 ──

    // 工件
    j["sVelocity"]     = p.sVelocity;
    j["diameter"]      = p.diameter;

    // 闸门独立报警/跟踪
    j["gateAlarm"]     = intsToJson(p.gateAlarm, 3);
    j["gateTrace"]     = intsToJson(p.gateTrace, 3);

    // 探头延迟
    j["probeDelay"]    = static_cast<double>(p.probeDelay);

    // TFM
    j["dimX"]          = static_cast<double>(p.dimX);
    j["dimY"]          = static_cast<double>(p.dimY);
    j["offsetX"]       = static_cast<double>(p.offsetX);
    j["offsetY"]       = static_cast<double>(p.offsetY);
    j["pixelSize"]     = static_cast<double>(p.pixelSize);
    j["pieceThickness"] = static_cast<double>(p.pieceThickness);
    j["tfmDGain"]      = static_cast<double>(p.tfmDGain);
    j["tfmSmooth"]     = p.tfmSmooth;
    j["parRestrainH16"] = p.parRestrainH16;
    j["parRestrainL16"] = p.parRestrainL16;

    // 编码器周数
    j["circleDeg"]     = p.circleDeg;

    // 成像扫描范围
    j["imgSpanStart"]  = static_cast<double>(p.imgSpanStart);
    j["imgSpanEnd"]    = static_cast<double>(p.imgSpanEnd);

    // 全局状态
    j["readNum"]       = p.readNum;
    j["beamCount"]     = p.beamCount;
    j["tempBeamCount"] = p.tempBeamCount;

    // 声束描述符（仅保存非零项以减少文件体积）
    {
        QJsonArray beamArr;
        for (int i = 0; i < 128; ++i) {
            const auto &b = p.beams[i];
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

    return j;
}

void ParamPage::deserializeParams(const QJsonObject &j)
{
    auto &p = m_params;

    // 发射
    if (j.contains("highVoltage"))   p.highVoltage   = j["highVoltage"].toInt();
    if (j.contains("pulseWidth"))    p.pulseWidth    = j["pulseWidth"].toInt();
    if (j.contains("prf"))           p.prf           = j["prf"].toInt();
    if (j.contains("range"))         p.range         = static_cast<float>(j["range"].toDouble());
    if (j.contains("tempCorrect"))   p.tempCorrect   = j["tempCorrect"].toInt();
    if (j.contains("aDataLen"))      p.aDataLen      = j["aDataLen"].toInt();

    // 接收
    if (j.contains("aGain"))         p.aGain         = static_cast<float>(j["aGain"].toDouble());
    if (j.contains("dGain"))         p.dGain         = static_cast<float>(j["dGain"].toDouble());
    if (j.contains("curBeam"))       p.curBeam       = j["curBeam"].toInt();
    if (j.contains("rectify"))       p.rectify       = j["rectify"].toInt();
    if (j.contains("filter"))        p.filter        = j["filter"].toInt();
    if (j.contains("video"))         p.video         = j["video"].toInt();

    // 闸门
    if (j.contains("gateSelect"))    p.gateSelect    = j["gateSelect"].toInt();
    if (j.contains("gateStart"))     jsonToFloats(j["gateStart"].toArray(), p.gateStart, 3);
    if (j.contains("gateWidth"))     jsonToFloats(j["gateWidth"].toArray(), p.gateWidth, 3);
    if (j.contains("gateThreshold")) jsonToFloats(j["gateThreshold"].toArray(), p.gateThreshold, 3);
    if (j.contains("gateMeasure"))   jsonToInts(j["gateMeasure"].toArray(), p.gateMeasure, 3);
    if (j.contains("alarmSound"))    p.alarmSound    = j["alarmSound"].toInt();

    // 探头
    if (j.contains("probeType"))     p.probeType     = j["probeType"].toInt();
    if (j.contains("probeFreq"))     p.probeFreq     = static_cast<float>(j["probeFreq"].toDouble());
    if (j.contains("probeCount"))    p.probeCount    = j["probeCount"].toInt();
    if (j.contains("probePitch"))    p.probePitch    = static_cast<float>(j["probePitch"].toDouble());

    // 楔块
    if (j.contains("wedgeEnable"))   p.wedgeEnable   = j["wedgeEnable"].toInt();
    if (j.contains("wedgeType"))     p.wedgeType     = j["wedgeType"].toInt();
    if (j.contains("wedgeAngle"))    p.wedgeAngle    = static_cast<float>(j["wedgeAngle"].toDouble());
    if (j.contains("wedgeVelocity")) p.wedgeVelocity = j["wedgeVelocity"].toInt();
    if (j.contains("wedgeHeight"))   p.wedgeHeight   = static_cast<float>(j["wedgeHeight"].toDouble());

    // 工件
    if (j.contains("material"))      p.material      = j["material"].toInt();
    if (j.contains("lVelocity"))     p.lVelocity     = j["lVelocity"].toInt();
    if (j.contains("traceEnable"))   p.traceEnable   = j["traceEnable"].toInt();

    // 扫查
    if (j.contains("scanType"))      p.scanType      = j["scanType"].toInt();
    if (j.contains("eleStart"))      p.eleStart      = j["eleStart"].toInt();
    if (j.contains("eleEnd"))        p.eleEnd        = j["eleEnd"].toInt();
    if (j.contains("eleAperture"))   p.eleAperture   = j["eleAperture"].toInt();
    if (j.contains("angleFrom"))     p.angleFrom     = static_cast<float>(j["angleFrom"].toDouble());
    if (j.contains("angleTo"))       p.angleTo       = static_cast<float>(j["angleTo"].toDouble());
    if (j.contains("angle"))         p.angle         = static_cast<float>(j["angle"].toDouble());
    if (j.contains("focus"))         p.focus         = static_cast<float>(j["focus"].toDouble());

    // 成像
    if (j.contains("imgLineX1"))     p.imgLineX1     = j["imgLineX1"].toInt();
    if (j.contains("imgLineX2"))     p.imgLineX2     = j["imgLineX2"].toInt();
    if (j.contains("imgLineY1"))     p.imgLineY1     = j["imgLineY1"].toInt();
    if (j.contains("imgLineY2"))     p.imgLineY2     = j["imgLineY2"].toInt();
    if (j.contains("degPerPoint"))   p.degPerPoint   = static_cast<float>(j["degPerPoint"].toDouble());

    // 编码器
    if (j.contains("direction"))     p.direction     = j["direction"].toInt();
    if (j.contains("coderDeg"))      p.coderDeg      = static_cast<float>(j["coderDeg"].toDouble());
    if (j.contains("checkDistance")) p.checkDistance = static_cast<float>(j["checkDistance"].toDouble());

    // 校准
    if (j.contains("calibItem"))     p.calibItem     = j["calibItem"].toInt();
    if (j.contains("realDistance"))  p.realDistance  = static_cast<float>(j["realDistance"].toDouble());
    if (j.contains("beamDelay"))     p.beamDelay     = static_cast<float>(j["beamDelay"].toDouble());
    if (j.contains("tcgCoeff"))      p.tcgCoeff      = static_cast<float>(j["tcgCoeff"].toDouble());
    if (j.contains("calibEnable"))   p.calibEnable   = j["calibEnable"].toInt();
    if (j.contains("tcgStart"))      p.tcgStart      = j["tcgStart"].toInt();
    if (j.contains("tcgEnd"))        p.tcgEnd        = j["tcgEnd"].toInt();
    if (j.contains("acgSwitch"))     p.acgSwitch     = j["acgSwitch"].toInt();
    if (j.contains("tcgSwitch"))     p.tcgSwitch     = j["tcgSwitch"].toInt();
    if (j.contains("acgValue"))      jsonToFloats(j["acgValue"].toArray(), p.acgValue, 128);
    if (j.contains("tcgPointX"))     jsonToShorts(j["tcgPointX"].toArray(), reinterpret_cast<short*>(p.tcgPointX), 10 * 128);
    if (j.contains("tcgPointValue")) jsonToFloats(j["tcgPointValue"].toArray(), reinterpret_cast<float*>(p.tcgPointValue), 10 * 128);

    // ── MFC 补齐字段 ──
    if (j.contains("sVelocity"))     p.sVelocity     = j["sVelocity"].toInt();
    if (j.contains("diameter"))      p.diameter      = j["diameter"].toInt();
    if (j.contains("gateAlarm"))     jsonToInts(j["gateAlarm"].toArray(), p.gateAlarm, 3);
    if (j.contains("gateTrace"))     jsonToInts(j["gateTrace"].toArray(), p.gateTrace, 3);
    if (j.contains("probeDelay"))    p.probeDelay    = static_cast<float>(j["probeDelay"].toDouble());
    if (j.contains("dimX"))          p.dimX          = static_cast<float>(j["dimX"].toDouble());
    if (j.contains("dimY"))          p.dimY          = static_cast<float>(j["dimY"].toDouble());
    if (j.contains("offsetX"))       p.offsetX       = static_cast<float>(j["offsetX"].toDouble());
    if (j.contains("offsetY"))       p.offsetY       = static_cast<float>(j["offsetY"].toDouble());
    if (j.contains("pixelSize"))     p.pixelSize     = static_cast<float>(j["pixelSize"].toDouble());
    if (j.contains("pieceThickness")) p.pieceThickness = static_cast<float>(j["pieceThickness"].toDouble());
    if (j.contains("tfmDGain"))      p.tfmDGain      = static_cast<float>(j["tfmDGain"].toDouble());
    if (j.contains("tfmSmooth"))     p.tfmSmooth     = j["tfmSmooth"].toInt();
    if (j.contains("parRestrainH16")) p.parRestrainH16 = j["parRestrainH16"].toInt();
    if (j.contains("parRestrainL16")) p.parRestrainL16 = j["parRestrainL16"].toInt();
    if (j.contains("circleDeg"))     p.circleDeg     = j["circleDeg"].toInt();
    if (j.contains("imgSpanStart"))  p.imgSpanStart  = static_cast<float>(j["imgSpanStart"].toDouble());
    if (j.contains("imgSpanEnd"))    p.imgSpanEnd    = static_cast<float>(j["imgSpanEnd"].toDouble());
    if (j.contains("readNum"))       p.readNum       = j["readNum"].toInt();
    if (j.contains("beamCount"))     p.beamCount     = j["beamCount"].toInt();
    if (j.contains("tempBeamCount")) p.tempBeamCount = j["tempBeamCount"].toInt();

    if (j.contains("beams")) {
        QJsonArray arr = j["beams"].toArray();
        for (const auto &v : arr) {
            QJsonObject bo = v.toObject();
            int i = bo["i"].toInt();
            if (i < 0 || i >= 128) continue;
            p.beams[i].x0 = bo["x0"].toInt();
            p.beams[i].y0 = bo["y0"].toInt();
            p.beams[i].x1 = bo["x1"].toInt();
            p.beams[i].y1 = bo["y1"].toInt();
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
    QSignalBlocker bi(m_alarmCombo);
    QSignalBlocker bj(m_probeTypeCombo);
    QSignalBlocker bk(m_probeFreqSpin);
    QSignalBlocker bl(m_probeCountSpin);
    QSignalBlocker bm(m_probePitchSpin);
    QSignalBlocker bn(m_wedgeEnableCombo);
    QSignalBlocker bo(m_wedgeTypeCombo);
    QSignalBlocker bp(m_wedgeAngleSpin);
    QSignalBlocker bq(m_wedgeVelSpin);
    QSignalBlocker br(m_wedgeHeightSpin);
    QSignalBlocker bs(m_materialCombo);
    QSignalBlocker bt(m_lVelSpin);
    QSignalBlocker bu(m_traceEnableCombo);

    // ── 发射 ──
    m_voltCombo->setCurrentIndex(p.highVoltage);
    m_pulseWidthSpin->setValue(p.pulseWidth);
    m_prfSpin->setValue(p.prf);
    m_rangeSpin->setValue(static_cast<double>(p.range));
    m_tempCorrectCombo->setCurrentIndex(p.tempCorrect);
    m_aDataLenCombo->setCurrentIndex(p.aDataLen);

    // ── 接收 ──
    m_aGainSpin->setValue(static_cast<double>(p.aGain));
    m_dGainSpin->setValue(static_cast<double>(p.dGain));
    m_beamNoSpin->setValue(p.curBeam);
    m_rectifyCombo->setCurrentIndex(p.rectify);
    m_filterCombo->setCurrentIndex(p.filter);
    m_videoCombo->setCurrentIndex(p.video);

    // ── 闸门 ──
    m_gateSelCombo->setCurrentIndex(p.gateSelect);
    m_gateStartSpin->setValue(static_cast<double>(p.gateStart[p.gateSelect]));
    m_gateWidthSpin->setValue(static_cast<double>(p.gateWidth[p.gateSelect]));
    m_gateThreshSpin->setValue(static_cast<double>(p.gateThreshold[p.gateSelect]));
    m_gateMeasureCombo->setCurrentIndex(p.gateMeasure[p.gateSelect]);
    m_alarmCombo->setCurrentIndex(p.alarmSound);

    // ── 探头 ──
    m_probeTypeCombo->setCurrentIndex(p.probeType);
    m_probeFreqSpin->setValue(static_cast<double>(p.probeFreq));
    m_probeCountSpin->setValue(p.probeCount);
    m_probePitchSpin->setValue(static_cast<double>(p.probePitch));

    // ── 楔块 ──
    m_wedgeEnableCombo->setCurrentIndex(p.wedgeEnable);
    m_wedgeTypeCombo->setCurrentIndex(p.wedgeType);
    m_wedgeAngleSpin->setValue(static_cast<double>(p.wedgeAngle));
    m_wedgeVelSpin->setValue(p.wedgeVelocity);
    m_wedgeHeightSpin->setValue(static_cast<double>(p.wedgeHeight));

    // ── 工件 ──
    m_materialCombo->setCurrentIndex(p.material);
    m_lVelSpin->setValue(p.lVelocity);
    m_traceEnableCombo->setCurrentIndex(p.traceEnable);

    // ── 扫查（重建动态槽位控件）──
    if (m_scanTypeCombo) {
        m_scanTypeCombo->blockSignals(true);
        m_scanTypeCombo->setCurrentIndex(p.scanType);
        m_scanTypeCombo->blockSignals(false);
    }
    onScanTypeChanged(p.scanType);

    // 通知闸门同步
    emit gateParamsChanged();
}

// ── 文件目录辅助 ──
static QString paramsDir()  { QString d = QCoreApplication::applicationDirPath() + "/params";  QDir().mkpath(d); return d; }
static QString dataDir()    { QString d = QCoreApplication::applicationDirPath() + "/data";    QDir().mkpath(d); return d; }

void ParamPage::onSaveParams()
{
    QString path = QFileDialog::getSaveFileName(
        this, "保存参数", paramsDir(),
        "参数文件 (*.json);;所有文件 (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;

    QJsonDocument doc(serializeParams());
    file.write(doc.toJson(QJsonDocument::Indented));
}

void ParamPage::onLoadParams()
{
    QString path = QFileDialog::getOpenFileName(
        this, "调用参数", paramsDir(),
        "参数文件 (*.json *.ini *.param);;所有文件 (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return;
    if (!doc.isObject()) return;

    deserializeParams(doc.object());
    syncUiFromParams();

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
        this, "保存C扫数据", dataDir(),
        "C扫数据 (*.dat);;所有文件 (*)");
    if (path.isEmpty()) return;
    emit saveDataRequested(path);
}

void ParamPage::onReplayData()
{
    QString path = QFileDialog::getOpenFileName(
        this, "回放C扫数据", dataDir(),
        "C扫数据 (*.dat);;所有文件 (*)");
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
    auto *topLabel = new QLabel("参数设置");
    topLabel->setObjectName("SidebarTitle");
    topLabel->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(topLabel);

    auto *subLabel = new QLabel("常规相控阵参数");
    subLabel->setObjectName("PageLabel");
    subLabel->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(subLabel);

    // 导航列表（11项：8项参数 + 3项C扫）
    m_nav = new QListWidget;
    m_nav->setObjectName("ParamNav");
    const QStringList cats = {"发射", "接收", "闸门", "探头", "楔块", "工件", "扫查", "校准",
                               "成像", "编码器", "分析"};
    for (const auto &c : cats)
        m_nav->addItem(c);
    m_nav->setCurrentRow(0);
    sideLayout->addWidget(m_nav, 1);

    // ── 底部操作按钮 ──
    auto *btnLayout = new QVBoxLayout;
    btnLayout->setContentsMargins(8, 6, 8, 8);
    btnLayout->setSpacing(5);

    auto *applyBtn = new QPushButton("应用法则");
    applyBtn->setObjectName("ApplyLawButton");
    applyBtn->setCursor(Qt::PointingHandCursor);
    applyBtn->setFixedHeight(34);
    connect(applyBtn, &QPushButton::clicked, this, &ParamPage::onApplyLaw);

    auto *saveBtn = new QPushButton("保存参数");
    saveBtn->setObjectName("SaveButton");
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setFixedHeight(30);
    connect(saveBtn, &QPushButton::clicked, this, &ParamPage::onSaveParams);

	// ── C扫数据按钮（默认置灰，在成像/编码器/分析页解锁）──
	m_saveDataBtn = new QPushButton("保存数据");
	m_saveDataBtn->setObjectName("SaveDataButton");
	m_saveDataBtn->setCursor(Qt::PointingHandCursor);
	m_saveDataBtn->setFixedHeight(30);
	m_saveDataBtn->setEnabled(false);
	connect(m_saveDataBtn, &QPushButton::clicked, this, &ParamPage::onSaveData);

	m_replayDataBtn = new QPushButton("回放数据");
	m_replayDataBtn->setObjectName("ReplayDataButton");
	m_replayDataBtn->setCursor(Qt::PointingHandCursor);
	m_replayDataBtn->setFixedHeight(30);
	m_replayDataBtn->setEnabled(false);
	connect(m_replayDataBtn, &QPushButton::clicked, this, &ParamPage::onReplayData);

    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(saveBtn);
	btnLayout->addWidget(m_saveDataBtn);
	btnLayout->addWidget(m_replayDataBtn);
    sideLayout->addLayout(btnLayout);

    root->addWidget(sidebar);

    // ── 右侧内容区 ──
    m_stack = new QStackedWidget;
    m_stack->setObjectName("ParamStack");
    m_stack->setFixedWidth(kStackWidth);
    m_stack->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

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

void ParamPage::setDriver(IDriver *driver)
{
    m_driver = driver;
}
