#pragma once

#include "PAParams.h"
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class ParameterDispatcher;

class GateParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit GateParamPage(PAParams *params, ParameterDispatcher *dispatcher, QWidget *parent = nullptr);

    QComboBox *gateSelCombo = nullptr;
    QDoubleSpinBox *gateStartSpin = nullptr;
    QDoubleSpinBox *gateWidthSpin = nullptr;
    QDoubleSpinBox *gateThreshSpin = nullptr;
    QComboBox *gateMeasureCombo = nullptr;
    QComboBox *gateAlarmCombo = nullptr;
    QComboBox *gateTraceCombo = nullptr;
    QComboBox *alarmSoundCombo = nullptr;

signals:
    void gateParamsChanged();

private:
    void onGateSelectChanged(int idx);
    void onGateParamChanged();

    PAParams *m_params = nullptr;
    ParameterDispatcher *m_dispatcher = nullptr;
};
