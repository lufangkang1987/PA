#include "WorkpieceParamPage.h"
#include "ParamPageUiHelpers.h"

#include <QComboBox>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QtGlobal>

WorkpieceParamPage::WorkpieceParamPage(PAParams *params, QWidget *parent)
    : QWidget(parent), m_params(params)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("工件参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    materialCombo = makeParamCombo({QString::fromUtf8("钢纵波"), QString::fromUtf8("钢横波")}, m_params->wp.material);
    connect(materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WorkpieceParamPage::onMaterialChanged);
    f->addRow(QString::fromUtf8("材料"), materialCombo);

    lVelSpin = makeParamIntSpin(1000, 9000, m_params->wp.lVelocity, 10);
    lVelSpin->setSuffix(" m/s");
    f->addRow(QString::fromUtf8("声速"), wrapWithStepSelector(lVelSpin, {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));

    traceEnableCombo = makeParamCombo({QString::fromUtf8("否"), QString::fromUtf8("是")}, m_params->wp.traceEnable);
    f->addRow(QString::fromUtf8("跟踪启用"), traceEnableCombo);

    connect(materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params->wp.material = v; });
    connect(lVelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_params->wp.lVelocity = v; });
    connect(traceEnableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params->wp.traceEnable = v; });

    layout->addWidget(form);
    layout->addStretch();
}

void WorkpieceParamPage::onMaterialChanged(int idx)
{
    lVelSpin->setValue(idx == 0 ? 5900 : 3230);
}

void WorkpieceParamPage::setVelocity(int velocity)
{
    m_params->wp.lVelocity = qBound(m_params->wedge.wedgeVelocity, velocity, 9000);
    if (!lVelSpin) return;
    QSignalBlocker blocker(lVelSpin);
    lVelSpin->setValue(m_params->wp.lVelocity);
}
