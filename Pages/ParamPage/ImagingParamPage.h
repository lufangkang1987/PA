#pragma once

#include "PAParams.h"
#include <QWidget>

class QDoubleSpinBox;
class QPushButton;
class QSpinBox;

class ImagingParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit ImagingParamPage(PAParams *params, QWidget *parent = nullptr);
    void setScanning(bool scanning);

    QSpinBox *imagingLineSpin[4] = {};
    QDoubleSpinBox *degPerPointSpin = nullptr;
    QPushButton *scanBtn = nullptr;

signals:
    void scanButtonClicked();
    void cScanViewParamsChanged();

private:
    PAParams *m_params = nullptr;
};
