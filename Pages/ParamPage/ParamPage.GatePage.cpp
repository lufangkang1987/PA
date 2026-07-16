#include "ParamPage.h"
#include "GateParamPage.h"
#include "ParameterDispatcher.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>

void ParamPage::buildGatePage()
{
    m_gatePage = new GateParamPage(&m_params, m_dispatcher, this);
    m_gateSelCombo = m_gatePage->gateSelCombo;
    m_gateStartSpin = m_gatePage->gateStartSpin;
    m_gateWidthSpin = m_gatePage->gateWidthSpin;
    m_gateThreshSpin = m_gatePage->gateThreshSpin;
    m_gateMeasureCombo = m_gatePage->gateMeasureCombo;
    m_gateAlarmCombo = m_gatePage->gateAlarmCombo;
    m_gateTraceCombo = m_gatePage->gateTraceCombo;
    m_alarmSoundCombo = m_gatePage->alarmSoundCombo;
    connect(m_gatePage, &GateParamPage::gateParamsChanged,
            this, &ParamPage::gateParamsChanged);
    m_stack->addWidget(m_gatePage);
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


