#include "CScanPage.h"
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

QDoubleSpinBox *CScanPage::makeDoubleSpin(double min, double max, double val,
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

QSpinBox *CScanPage::makeIntSpin(int min, int max, int val, int step)
{
    auto *s = new QSpinBox;
    s->setRange(min, max);
    s->setValue(val);
    s->setSingleStep(step);
    polishField(s);
    return s;
}

QComboBox *CScanPage::makeCombo(const QStringList &items, int currentIdx)
{
    auto *c = new QComboBox;
    c->addItems(items);
    c->setCurrentIndex(currentIdx);
    polishField(c);
    return c;
}

/// Wrap a SpinBox with a step selector (combo showing available step levels).
static QWidget* wrapWithStepSelector(QWidget *spinBox, const QStringList &stepLabels,
                                      const QList<double> &stepValues, int defaultIdx)
{
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

    double initStep = stepValues[defaultIdx];
    if (auto *ds = qobject_cast<QDoubleSpinBox*>(spinBox))
        ds->setSingleStep(initStep);
    else if (auto *is = qobject_cast<QSpinBox*>(spinBox))
        is->setSingleStep(static_cast<int>(qRound(initStep)));

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

QWidget *CScanPage::createCategoryPage(const QString &title)
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

void CScanPage::buildImagingPage()
{
    auto *page = createCategoryPage("C扫成像参数");
    auto *content = page->property("contentWidget").value<QWidget*>();
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    // 参数表单
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    // 采集线: 步进 [1, 10, 100], 默认10
    f->addRow("采集线 X1", wrapWithStepSelector(makeIntSpin(0, 511, m_params.imgLineX1, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("采集线 X2", wrapWithStepSelector(makeIntSpin(0, 511, m_params.imgLineX2, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("采集线 Y1", wrapWithStepSelector(makeIntSpin(0, 399, m_params.imgLineY1, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    f->addRow("采集线 Y2", wrapWithStepSelector(makeIntSpin(0, 399, m_params.imgLineY2, 10), {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
    // C扫增量: 步进 [0.1, 1.0, 10.0] mm/d, 默认1.0
    f->addRow("C扫增量", wrapWithStepSelector(makeDoubleSpin(0.1, 5.0, m_params.degPerPoint, 0.1, "mm/d"), {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    layout->addWidget(form);

    // ── 开始扫描按钮（醒目大按钮）──
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
    connect(m_scanBtn, &QPushButton::clicked, this, &CScanPage::onScanButtonClicked);
    btnLayout->addWidget(m_scanBtn);

    // 提示信息
    auto *hintLabel = new QLabel("点击按钮开始/停止 C 扫描数据采集");
    hintLabel->setObjectName("HintLabel");
    hintLabel->setAlignment(Qt::AlignCenter);
    btnLayout->addWidget(hintLabel);

    layout->addWidget(btnFrame);
    layout->addStretch();
    m_stack->addWidget(page);
}

void CScanPage::buildEncoderPage()
{
    auto *page = createCategoryPage("编码器参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    f->addRow("成像方向", makeCombo({"正向", "反向"}, m_params.direction));
    // 编码精度: 步进 [0.001, 0.01, 0.1] mm/p, 默认0.01
    f->addRow("编码精度", wrapWithStepSelector(makeDoubleSpin(0.001, 10.0, m_params.coderDeg, 0.01, "mm/p", 3), {"0.001", "0.01", "0.1"}, {0.001, 0.01, 0.1}, 1));
    // 校准距离: 步进 [0.1, 1.0, 10.0] mm, 默认1.0
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

void CScanPage::buildAnalysisPage()
{
    auto *page = createCategoryPage("C扫分析参数");
    auto *layout = qobject_cast<QVBoxLayout*>(page->property("contentLayout").value<QLayout*>());

    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    // 测量线: 步进 [1, 10, 100], 默认10
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

// ──────────────────────────────────────────────
// 扫描按钮逻辑
// ──────────────────────────────────────────────

void CScanPage::onScanButtonClicked()
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
// 主UI构建
// ──────────────────────────────────────────────

void CScanPage::setupUi()
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 左侧导航栏 ──
    auto *sidebar = new QFrame;
    sidebar->setObjectName("ParamSidebar");
    sidebar->setFixedWidth(170);
    sidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    // 顶部标题
    auto *topLabel = new QLabel("C 扫参数");
    topLabel->setObjectName("SidebarTitle");
    topLabel->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(topLabel);

    auto *subLabel = new QLabel("成像 / 编码器 / 分析");
    subLabel->setObjectName("PageLabel");
    subLabel->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(subLabel);

    // 导航列表
    m_nav = new QListWidget;
    m_nav->setObjectName("ParamNav");
    const QStringList cats = {"成像", "编码器", "分析"};
    for (const auto &c : cats)
        m_nav->addItem(c);
    m_nav->setCurrentRow(0);
    sideLayout->addWidget(m_nav, 1);

    root->addWidget(sidebar);

    // ── 右侧内容区 ──
    m_stack = new QStackedWidget;
    m_stack->setObjectName("ParamStack");
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 构建所有页面
    buildImagingPage();    // 0
    buildEncoderPage();    // 1
    buildAnalysisPage();   // 2

    // 导航联动
    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_stack->count())
            m_stack->setCurrentIndex(row);
    });

    root->addWidget(m_stack, 1);

    // ── 样式表（与 ParamPage 保持一致）──
    setStyleSheet(R"(
        QWidget {
            background:#08131d;
            color:#cfe7f4;
            font-family:"Microsoft YaHei";
            font-size:13px;
        }
        #ParamSidebar {
            background:#0a1a28;
            border-right:1px solid #1a3a52;
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
        #ParamStack {
            background:#08131d;
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
        #ScanButtonFrame {
            background:transparent;
            border:0;
        }
        #HintLabel {
            color:#6a8aa0;
            font-size:12px;
            background:transparent;
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
}

CScanPage::CScanPage(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void CScanPage::setDriver(IDriver *driver)
{
    m_driver = driver;
}
