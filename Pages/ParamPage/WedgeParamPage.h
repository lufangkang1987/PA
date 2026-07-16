#pragma once

#include "PAParams.h"
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

class WedgeParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit WedgeParamPage(PAParams *params, QWidget *parent = nullptr);

    QComboBox *wedgeEnableCombo = nullptr;
    QComboBox *wedgeTypeCombo = nullptr;
    QDoubleSpinBox *wedgeAngleSpin = nullptr;
    QSpinBox *wedgeVelSpin = nullptr;
    QDoubleSpinBox *wedgeHeightSpin = nullptr;

private:
    void onWedgeTypeChanged(int idx);

    PAParams *m_params = nullptr;
};
