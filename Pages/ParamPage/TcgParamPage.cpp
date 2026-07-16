#include "TcgParamPage.h"
#include "ParamPageUiHelpers.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

TcgParamPage::TcgParamPage(PAParams *params, QWidget *parent)
    : QWidget(parent), m_params(params)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("校准内容"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    auto *calibItem = makeParamCombo({QString::fromUtf8("声速"), QString::fromUtf8("声束延迟"), "ACG", "TCG"}, m_params->tcg.calibItem);
    f->addRow(QString::fromUtf8("校准项目"), calibItem);

    auto *realDist = makeParamDoubleSpin(10.0, 1000.0, m_params->tcg.realDistance, 0.1, "mm", 1);
    f->addRow(QString::fromUtf8("实际距离"), wrapWithStepSelector(realDist, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    auto *beamDelay = makeParamDoubleSpin(0.0, 100.0, m_params->tcg.beamDelay, 0.1, "us", 1);
    f->addRow(QString::fromUtf8("声束延迟"), wrapWithStepSelector(beamDelay, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    auto *tcgCoeff = makeParamDoubleSpin(0.0, 0.5, m_params->tcg.tcgCoeff, 0.001, "", 3);
    f->addRow(QString::fromUtf8("TCG系数"), wrapWithStepSelector(tcgCoeff, {"0.001", "0.01", "0.1"}, {0.001, 0.01, 0.1}, 1));

    auto *refLabel = new QLabel(QString::fromUtf8("共 9 个参考点"));
    f->addRow(QString::fromUtf8("TCG参考点"), refLabel);

    auto *calibEnable = makeParamCombo({QString::fromUtf8("关闭"), "ACG"}, m_params->tcg.calibEnable);
    f->addRow(QString::fromUtf8("校准启用"), calibEnable);

    layout->addWidget(form);
    calibrationBtn = new QPushButton(QString::fromUtf8("开始 / 完成校准"));
    calibrationBtn->setFixedHeight(36);
    layout->addWidget(calibrationBtn);
    layout->addStretch();

    connect(calibItem, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int value) { m_params->tcg.calibItem = value; });
    connect(realDist, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) { m_params->tcg.realDistance = float(value); });
    connect(beamDelay, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) { m_params->tcg.beamDelay = float(value); });
    connect(tcgCoeff, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) { m_params->tcg.tcgCoeff = float(value); });
    connect(calibEnable, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int value) { m_params->tcg.calibEnable = value; });
    connect(calibrationBtn, &QPushButton::clicked, this, [this] { emit calibrationRequested(m_params->tcg.calibItem); });
}
