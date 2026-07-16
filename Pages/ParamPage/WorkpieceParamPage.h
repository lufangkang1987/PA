#pragma once

#include "PAParams.h"
#include <QWidget>

class QComboBox;
class QSpinBox;

class WorkpieceParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit WorkpieceParamPage(PAParams *params, QWidget *parent = nullptr);
    void setVelocity(int velocity);

    QComboBox *materialCombo = nullptr;
    QSpinBox *lVelSpin = nullptr;
    QComboBox *traceEnableCombo = nullptr;

private:
    void onMaterialChanged(int idx);

    PAParams *m_params = nullptr;
};
