#pragma once

#include "PAParams.h"
#include <QWidget>

class QPushButton;

class TcgParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit TcgParamPage(PAParams *params, QWidget *parent = nullptr);

    QPushButton *calibrationBtn = nullptr;

signals:
    void calibrationRequested(int item);

private:
    PAParams *m_params = nullptr;
};
