#pragma once

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

inline constexpr int kSidebarWidth = 170;
inline constexpr int kStackWidth = 215;
inline constexpr int kPanelBorderWidth = 1;
inline constexpr int kPanelPadding = 1;
inline constexpr int kFrameInset = kPanelBorderWidth + kPanelPadding;
inline constexpr int kCollapsedWidth = kSidebarWidth + kFrameInset * 2;
inline constexpr int kExpandedWidth = kSidebarWidth + kStackWidth + kFrameInset * 2;
inline constexpr qint64 kCollapseDebounceMs = 180;

inline void polishField(QWidget *w)
{
    w->setFixedHeight(30);
    w->setMinimumWidth(0);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

inline QFormLayout *makeForm(QGroupBox *box)
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

inline QVBoxLayout *makeParamSubPageLayout(QWidget *page, const QString &title)
{
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

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(scroll);
    scroll->setWidget(content);
    return layout;
}

inline QComboBox *makeParamCombo(const QStringList &items, int currentIdx)
{
    auto *c = new QComboBox;
    c->addItems(items);
    c->setCurrentIndex(currentIdx);
    polishField(c);
    return c;
}

inline QSpinBox *makeParamIntSpin(int min, int max, int val, int step)
{
    auto *s = new QSpinBox;
    s->setRange(min, max);
    s->setValue(val);
    s->setSingleStep(step);
    polishField(s);
    return s;
}

inline QDoubleSpinBox *makeParamDoubleSpin(double min, double max, double val,
                                           double step, const QString &suffix,
                                           int decimals = -1)
{
    auto *s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setValue(val);
    s->setSingleStep(step);
    s->setDecimals(decimals >= 0 ? decimals : (step < 0.1 ? 2 : 1));
    if (!suffix.isEmpty()) s->setSuffix(" " + suffix);
    polishField(s);
    return s;
}

inline QWidget *wrapWithStepSelector(QWidget *spinBox, const QStringList &stepLabels,
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

    const double initStep = stepValues[defaultIdx];
    if (auto *ds = qobject_cast<QDoubleSpinBox *>(spinBox))
        ds->setSingleStep(initStep);
    else if (auto *is = qobject_cast<QSpinBox *>(spinBox))
        is->setSingleStep(static_cast<int>(qRound(initStep)));

    QObject::connect(stepCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), spinBox,
        [spinBox, stepValues](int idx) {
            if (idx < 0 || idx >= stepValues.size()) return;
            const double step = stepValues[idx];
            if (auto *ds = qobject_cast<QDoubleSpinBox *>(spinBox))
                ds->setSingleStep(step);
            else if (auto *is = qobject_cast<QSpinBox *>(spinBox))
                is->setSingleStep(static_cast<int>(qRound(step)));
        });

    layout->addWidget(stepCombo);
    return wrapper;
}
