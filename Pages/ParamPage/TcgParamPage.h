#pragma once

#include "PAParams.h"
#include <QWidget>

class QDoubleSpinBox;
class QLabel;

class TcgParamPage : public QWidget
{
    Q_OBJECT
public:
    explicit TcgParamPage(PAParams *params, QWidget *parent = nullptr);
    void syncFromParams();

signals:
    void tcgPointsChanged();

private:
    static constexpr int PointCount = 6;
    void updateDistanceRanges();
    void updatePercent(int point);

    PAParams *m_params = nullptr;
    QDoubleSpinBox *m_distanceSpin[PointCount] = {};
    QDoubleSpinBox *m_ratioSpin[PointCount] = {};
    QLabel *m_percentLabel[PointCount] = {};
};
