#pragma once

#include "PAParams.h"
#include <QWidget>

class QPushButton;

class EncoderParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit EncoderParamPage(PAParams *params, QWidget *parent = nullptr);

    QPushButton *encoderCalibrationBtn = nullptr;

signals:
    void encoderCalibrationRequested();

private:
    PAParams *m_params = nullptr;
};
