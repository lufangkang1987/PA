#include "ProbeParamPage.h"
#include "ParamPageUiHelpers.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QSpinBox>

ProbeParamPage::ProbeParamPage(PAParams *params, QWidget *parent)
    : QWidget(parent), m_params(params)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("探头参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    probeTypeCombo = makeParamCombo({QString::fromUtf8("自定义"), "2.5L16", "5.0S64"}, m_params->probe.probeType);
    connect(probeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProbeParamPage::onProbeTypeChanged);
    f->addRow(QString::fromUtf8("探头型号"), probeTypeCombo);

    probeFreqSpin = makeParamDoubleSpin(0.2, 20.0, m_params->probe.probeFreq, 0.1, "MHz");
    f->addRow(QString::fromUtf8("探头频率"), wrapWithStepSelector(probeFreqSpin, {"0.1", "1.0"}, {0.1, 1.0}, 0));

    probeCountSpin = makeParamIntSpin(1, 128, m_params->probe.probeCount, 1);
    f->addRow(QString::fromUtf8("阵元数"), wrapWithStepSelector(probeCountSpin, {"1", "10"}, {1.0, 10.0}, 0));

    probePitchSpin = makeParamDoubleSpin(0.10, 15.00, m_params->probe.probePitch, 0.01, "mm", 2);
    f->addRow(QString::fromUtf8("阵元间距"), wrapWithStepSelector(probePitchSpin, {"0.01", "0.1", "1.0"}, {0.01, 0.1, 1.0}, 1));

    connect(probeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) { m_params->probe.probeType = v; });
    connect(probeFreqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params->probe.probeFreq = static_cast<float>(v); });
    connect(probeCountSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_params->probe.probeCount = v; });
    connect(probePitchSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) { m_params->probe.probePitch = static_cast<float>(v); });

    layout->addWidget(form);
    layout->addStretch();
}

void ProbeParamPage::onProbeTypeChanged(int idx)
{
    if (idx == 1) {
        probeFreqSpin->setValue(2.5);
        probeCountSpin->setValue(16);
        probePitchSpin->setValue(1.00);
    } else if (idx == 2) {
        probeFreqSpin->setValue(5.0);
        probeCountSpin->setValue(64);
        probePitchSpin->setValue(1.00);
    }
}
