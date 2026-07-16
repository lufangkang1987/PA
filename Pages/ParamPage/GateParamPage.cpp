#include "GateParamPage.h"
#include "ParamPageUiHelpers.h"
#include "ParameterDispatcher.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QtGlobal>

GateParamPage::GateParamPage(PAParams *params, ParameterDispatcher *dispatcher, QWidget *parent)
    : QWidget(parent), m_params(params), m_dispatcher(dispatcher)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("闸门参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);
    const int g = qBound(0, m_params->gate.gateSelect, 2);

    gateSelCombo = makeParamCombo({QString::fromUtf8("A 闸门"), QString::fromUtf8("B 闸门"), QString::fromUtf8("C 闸门")}, g);
    connect(gateSelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GateParamPage::onGateSelectChanged);
    f->addRow(QString::fromUtf8("闸门选择"), gateSelCombo);

    gateStartSpin = makeParamDoubleSpin(0.0, 999.0, m_params->gate.gateStart[g], 0.1, "mm");
    connect(gateStartSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GateParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("闸门起位"), wrapWithStepSelector(gateStartSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    gateWidthSpin = makeParamDoubleSpin(0.0, 999.0, m_params->gate.gateWidth[g], 0.1, "mm");
    connect(gateWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GateParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("闸门宽度"), wrapWithStepSelector(gateWidthSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    gateThreshSpin = makeParamDoubleSpin(0.0, 99.0, m_params->gate.gateThreshold[g], 0.1, "%");
    connect(gateThreshSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GateParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("闸门高度"), wrapWithStepSelector(gateThreshSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    gateMeasureCombo = makeParamCombo({QString::fromUtf8("峰值"), QString::fromUtf8("前沿")}, m_params->gate.gateMeasure[g]);
    connect(gateMeasureCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GateParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("测量方式"), gateMeasureCombo);

    gateAlarmCombo = makeParamCombo({QString::fromUtf8("关"), QString::fromUtf8("开")}, m_params->gate.gateAlarm[g]);
    connect(gateAlarmCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GateParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("报警开关"), gateAlarmCombo);

    gateTraceCombo = makeParamCombo({QString::fromUtf8("关"), QString::fromUtf8("开")}, m_params->gate.gateTrace[g]);
    connect(gateTraceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GateParamPage::onGateParamChanged);
    f->addRow(QString::fromUtf8("跟踪开关"), gateTraceCombo);

    alarmSoundCombo = makeParamCombo({QString::fromUtf8("关"), QString::fromUtf8("A 门"), QString::fromUtf8("B 门"), QString::fromUtf8("AB 门")}, m_params->gate.alarmSound);
    connect(alarmSoundCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) { m_params->gate.alarmSound = idx; });
    f->addRow(QString::fromUtf8("报警声"), alarmSoundCombo);

    layout->addWidget(form);
    layout->addStretch();
}

void GateParamPage::onGateSelectChanged(int idx)
{
    if (idx < 0 || idx > 2) return;
    m_params->gate.gateSelect = idx;
    gateStartSpin->blockSignals(true);
    gateWidthSpin->blockSignals(true);
    gateThreshSpin->blockSignals(true);
    gateMeasureCombo->blockSignals(true);
    gateAlarmCombo->blockSignals(true);
    gateTraceCombo->blockSignals(true);
    gateStartSpin->setValue(m_params->gate.gateStart[idx]);
    gateWidthSpin->setValue(m_params->gate.gateWidth[idx]);
    gateThreshSpin->setValue(m_params->gate.gateThreshold[idx]);
    gateMeasureCombo->setCurrentIndex(m_params->gate.gateMeasure[idx]);
    gateAlarmCombo->setCurrentIndex(m_params->gate.gateAlarm[idx]);
    gateTraceCombo->setCurrentIndex(m_params->gate.gateTrace[idx]);
    gateStartSpin->blockSignals(false);
    gateWidthSpin->blockSignals(false);
    gateThreshSpin->blockSignals(false);
    gateMeasureCombo->blockSignals(false);
    gateAlarmCombo->blockSignals(false);
    gateTraceCombo->blockSignals(false);
    emit gateParamsChanged();
}

void GateParamPage::onGateParamChanged()
{
    const int g = m_params->gate.gateSelect;
    const char gateName = 'A' + g;
    m_params->gate.gateStart[g] = static_cast<float>(gateStartSpin->value());
    m_params->gate.gateWidth[g] = static_cast<float>(gateWidthSpin->value());
    m_params->gate.gateThreshold[g] = static_cast<float>(gateThreshSpin->value());
    m_params->gate.gateMeasure[g] = gateMeasureCombo->currentIndex();
    m_params->gate.gateAlarm[g] = gateAlarmCombo->currentIndex();
    m_params->gate.gateTrace[g] = gateTraceCombo->currentIndex();
    if (m_dispatcher)
        m_dispatcher->setGate(gateName, m_params->gate.gateStart[g],
                              m_params->gate.gateWidth[g],
                              m_params->gate.gateThreshold[g],
                              m_params->gate.gateMeasure[g]);
    emit gateParamsChanged();
}
