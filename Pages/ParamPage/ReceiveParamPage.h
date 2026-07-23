#pragma once

#include "PAParams.h"
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class ParameterDispatcher;

class ReceiveParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit ReceiveParamPage(PAParams *params, ParameterDispatcher *dispatcher, QWidget *parent = nullptr);
    void setDispatcher(ParameterDispatcher *dispatcher) { m_dispatcher = dispatcher; }
    void updateBeamNoRange();

    QDoubleSpinBox *aGainSpin = nullptr;
    QDoubleSpinBox *dGainSpin = nullptr;
    QSpinBox *beamNoSpin = nullptr;
    QComboBox *rectifyCombo = nullptr;
    QComboBox *filterCombo = nullptr;
    QComboBox *videoCombo = nullptr;

signals:
    void beamInfoChanged(int beamNo, double gainDb);

private:
    PAParams *m_params = nullptr;
    ParameterDispatcher *m_dispatcher = nullptr;
};
