#pragma once

#include "PAParams.h"
#include <QWidget>

class QComboBox;
class QLabel;

class ScanParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit ScanParamPage(PAParams *params, QWidget *parent = nullptr);
    void rebuildForScanType(int idx);

    QComboBox *scanTypeCombo = nullptr;
    QLabel *scanLabels[7] = {};
    QWidget *scanWidgets[7] = {};

signals:
    void beamGeometryChanged();

private:
    PAParams *m_params = nullptr;
};
