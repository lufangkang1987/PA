#pragma once

#include "PAParams.h"
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

class ProbeParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit ProbeParamPage(PAParams *params, QWidget *parent = nullptr);

    QComboBox *probeTypeCombo = nullptr;
    QDoubleSpinBox *probeFreqSpin = nullptr;
    QSpinBox *probeCountSpin = nullptr;
    QDoubleSpinBox *probePitchSpin = nullptr;

private:
    void onProbeTypeChanged(int idx);

    PAParams *m_params = nullptr;
};
