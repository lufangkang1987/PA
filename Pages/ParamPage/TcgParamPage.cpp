#include "TcgParamPage.h"
#include "ParamPageUiHelpers.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>

TcgParamPage::TcgParamPage(PAParams *params, QWidget *parent)
    : QWidget(parent), m_params(params)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("TCG控制点"));

    for (int i = 0; i < PointCount; ++i) {
        auto *box = new QGroupBox(QString::fromUtf8("控制点 %1").arg(i + 1));
        auto *form = makeForm(box);

        m_distanceSpin[i] = makeParamDoubleSpin(0.5, 150.0,
            m_params->tcg.tcgX[i], 0.1, "mm", 1);
        m_ratioSpin[i] = makeParamDoubleSpin(0.1, 20.0,
            m_params->tcg.tcgRatio[i], 0.1, QString(), 1);
        m_percentLabel[i] = new QLabel;
        m_percentLabel[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        form->addRow(QString::fromUtf8("距离 / 步长"),
            wrapWithStepSelector(m_distanceSpin[i],
                {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));
        form->addRow(QString::fromUtf8("补偿倍率"), m_ratioSpin[i]);
        form->addRow(QString::fromUtf8("对应波幅"), m_percentLabel[i]);
        layout->addWidget(box);

        connect(m_distanceSpin[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, i](double value) {
            m_params->tcg.tcgX[i] = float(value);
            updateDistanceRanges();
            emit tcgPointsChanged();
        });
        connect(m_ratioSpin[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, i](double value) {
            m_params->tcg.tcgRatio[i] = float(value);
            updatePercent(i);
            emit tcgPointsChanged();
        });
    }

    layout->addStretch();
    updateDistanceRanges();
    for (int i = 0; i < PointCount; ++i)
        updatePercent(i);
}

void TcgParamPage::syncFromParams()
{
    for (int i = 0; i < PointCount; ++i) {
        const QSignalBlocker distanceBlocker(m_distanceSpin[i]);
        const QSignalBlocker ratioBlocker(m_ratioSpin[i]);
        m_distanceSpin[i]->setValue(m_params->tcg.tcgX[i]);
        m_ratioSpin[i]->setValue(m_params->tcg.tcgRatio[i]);
        updatePercent(i);
    }
    updateDistanceRanges();
}

void TcgParamPage::updateDistanceRanges()
{
    for (int i = 0; i < PointCount; ++i) {
        const double minimum = i == 0 ? 0.5 : m_distanceSpin[i - 1]->value() + 0.1;
        const double maximum = i == PointCount - 1
            ? 150.0 : m_distanceSpin[i + 1]->value() - 0.1;
        const QSignalBlocker blocker(m_distanceSpin[i]);
        m_distanceSpin[i]->setRange(minimum, qMax(minimum, maximum));
    }
}

void TcgParamPage::updatePercent(int point)
{
    const double ratio = m_ratioSpin[point]->value();
    m_percentLabel[point]->setText(QStringLiteral("%1 %")
        .arg(80.0 / qMax(0.1, ratio), 0, 'f', 1));
}
