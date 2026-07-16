#pragma once

#include "PAParams.h"
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class ParameterDispatcher;

class TransmitParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit TransmitParamPage(PAParams *params, ParameterDispatcher *dispatcher, QWidget *parent = nullptr);

    QComboBox *voltCombo = nullptr;
    QSpinBox *pulseWidthSpin = nullptr;
    QSpinBox *prfSpin = nullptr;
    QDoubleSpinBox *rangeSpin = nullptr;
    QComboBox *tempCorrectCombo = nullptr;
    QComboBox *aDataLenCombo = nullptr;

private:
    PAParams *m_params = nullptr;
    ParameterDispatcher *m_dispatcher = nullptr;
};
