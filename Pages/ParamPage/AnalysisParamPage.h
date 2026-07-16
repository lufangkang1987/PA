#pragma once

#include "PAParams.h"
#include <QWidget>

class QSpinBox;

class AnalysisParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit AnalysisParamPage(PAParams *params, QWidget *parent = nullptr);
    void setAnalysisRect(int line1, int line2, int column1, int column2);

    QSpinBox *analysisLineSpin[4] = {};

signals:
    void cScanViewParamsChanged();
    void cScanPageRequested();
    void exitReplayRequested();

private:
    PAParams *m_params = nullptr;
};
