#include "ParamPage.h"
#include "ParamPageUiHelpers.h"
#include "ParameterDispatcher.h"
#include "ReceiveParamPage.h"
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

// ──────────────────────────────────────────────
// 辅助函数
// ──────────────────────────────────────────────

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

void ParamPage::setBeamNo(int beam)
{
    if (beam < 0 || beam > 127) return;
    m_params.rx.curBeam = beam;
    if (m_dispatcher) m_dispatcher->setCurrentBeam(beam);
    if (m_beamNoSpin) {
        m_beamNoSpin->blockSignals(true);
        m_beamNoSpin->setValue(beam + 1);
        m_beamNoSpin->blockSignals(false);
    }
    emit beamInfoChanged(beam, m_params.rx.aGain);
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
    if (m_receivePage)
        m_receivePage->setDispatcher(dispatcher);
}
