#include "WedgeParamPage.h"
#include "ParamPageUiHelpers.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QSpinBox>

WedgeParamPage::WedgeParamPage(PAParams *params, QWidget *parent)
    : QWidget(parent), m_params(params)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("楔块参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    wedgeEnableCombo = makeParamCombo({QString::fromUtf8("否"), QString::fromUtf8("是")}, m_params->wedge.wedgeEnable);
    f->addRow(QString::fromUtf8("楔块启用"), wedgeEnableCombo);

    wedgeTypeCombo = makeParamCombo({QString::fromUtf8("自定义"), "GW-PA"}, m_params->wedge.wedgeType);
    connect(wedgeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WedgeParamPage::onWedgeTypeChanged);
    f->addRow(QString::fromUtf8("楔块型号"), wedgeTypeCombo);

    wedgeAngleSpin = makeParamDoubleSpin(0.0, 89.0, m_params->wedge.wedgeAngle, 0.1, QString::fromUtf8("\u00B0"));
    f->addRow(QString::fromUtf8("楔块角度"), wrapWithStepSelector(wedgeAngleSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    wedgeVelSpin = makeParamIntSpin(1000, 9000, m_params->wedge.wedgeVelocity, 10);
    wedgeVelSpin->setSuffix(" m/s");
    f->addRow(QString::fromUtf8("楔块声速"), wrapWithStepSelector(wedgeVelSpin, {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));

    wedgeHeightSpin = makeParamDoubleSpin(0.1, 100.0, m_params->wedge.wedgeHeight, 0.1, "mm");
    f->addRow(QString::fromUtf8("楔块高度"), wrapWithStepSelector(wedgeHeightSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    connect(wedgeEnableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params->wedge.wedgeEnable = v; });
    connect(wedgeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params->wedge.wedgeType = v; });
    connect(wedgeAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params->wedge.wedgeAngle = static_cast<float>(v); });
    connect(wedgeVelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_params->wedge.wedgeVelocity = v; });
    connect(wedgeHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params->wedge.wedgeHeight = static_cast<float>(v); });

    layout->addWidget(form);
    layout->addStretch();
}

void WedgeParamPage::onWedgeTypeChanged(int idx)
{
    if (idx == 1) {
        wedgeAngleSpin->setValue(41.0);
        wedgeVelSpin->setValue(2337);
        wedgeHeightSpin->setValue(12.5);
    }
}
