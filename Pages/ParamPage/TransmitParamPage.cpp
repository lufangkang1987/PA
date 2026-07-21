#include "TransmitParamPage.h"
#include "ParamPageUiHelpers.h"
#include "ParameterDispatcher.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QSpinBox>

TransmitParamPage::TransmitParamPage(PAParams *params, ParameterDispatcher *dispatcher, QWidget *parent)
    : QWidget(parent), m_params(params), m_dispatcher(dispatcher)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("发射参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    voltCombo = makeParamCombo({"110 V", "40 V", "20 V"}, m_params->tx.highVoltage);
    f->addRow(QString::fromUtf8("发射电压"), voltCombo);

    pulseWidthSpin = makeParamIntSpin(30, 1250, m_params->tx.pulseWidth, 10);
    pulseWidthSpin->setSuffix(" ns");
    f->addRow(QString::fromUtf8("脉冲宽度"), wrapWithStepSelector(pulseWidthSpin, {"10", "50", "100"}, {10.0, 50.0, 100.0}, 1));

    prfSpin = makeParamIntSpin(25, 20000, m_params->tx.prf, 100);
    prfSpin->setSuffix(" Hz");
    f->addRow(QString::fromUtf8("重复频率"), wrapWithStepSelector(prfSpin, {"5", "100", "1000"}, {5.0, 100.0, 1000.0}, 1));

    rangeSpin = makeParamDoubleSpin(5.0, 1000.0, m_params->tx.range, 0.1, "mm", 1);
    f->addRow(QString::fromUtf8("检测范围"), wrapWithStepSelector(rangeSpin, {"0.1", "1.0", "10.0", "100.0"}, {0.1, 1.0, 10.0, 100.0}, 1));

    tempCorrectCombo = makeParamCombo({QString::fromUtf8("关"), QString::fromUtf8("开")}, m_params->tx.tempCorrect);
    f->addRow(QString::fromUtf8("温度补偿"), tempCorrectCombo);

    aDataLenCombo = makeParamCombo({QString::fromUtf8("100 点"), QString::fromUtf8("200 点"), QString::fromUtf8("400 点")}, m_params->tx.aDataLen);
    f->addRow(QString::fromUtf8("A波长度"), aDataLenCombo);

    connect(voltCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params->tx.highVoltage = v;
        if (m_dispatcher) m_dispatcher->setHighVoltage(v);
    });
    connect(pulseWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_params->tx.pulseWidth = v;
        if (m_dispatcher) m_dispatcher->setPulseWidth(v);
    });
    connect(prfSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_params->tx.prf = v;
        if (m_dispatcher) m_dispatcher->setPRF(v);
    });
    connect(rangeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_params->tx.range = static_cast<float>(v);
        if (m_dispatcher) m_dispatcher->setRange(static_cast<float>(v));
        emit rangeChanged(static_cast<float>(v));
    });
    connect(tempCorrectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params->tx.tempCorrect = v;
        if (m_dispatcher) m_dispatcher->setTemperatureCompensation(v != 0);
    });
    connect(aDataLenCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params->tx.aDataLen = v;
        if (m_dispatcher) m_dispatcher->setADataLen(v);
    });

    layout->addWidget(form);
    layout->addStretch();
}
