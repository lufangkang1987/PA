#include "EncoderParamPage.h"
#include "ParamPageUiHelpers.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>

EncoderParamPage::EncoderParamPage(PAParams *params, QWidget *parent)
    : QWidget(parent), m_params(params)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("编码器参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    auto *direction = makeParamCombo({QString::fromUtf8("正向"), QString::fromUtf8("反向")}, m_params->enc.direction);
    auto *coderDeg = makeParamDoubleSpin(0.001, 10.0, m_params->enc.coderDeg, 0.01, "mm/p", 3);
    auto *checkDistance = makeParamDoubleSpin(1.0, 200.0, m_params->enc.checkDistance, 0.1, "mm");
    f->addRow(QString::fromUtf8("成像方向"), direction);
    f->addRow(QString::fromUtf8("编码精度"), wrapWithStepSelector(coderDeg, {"0.001", "0.01", "0.1"}, {0.001, 0.01, 0.1}, 1));
    f->addRow(QString::fromUtf8("校准距离"), wrapWithStepSelector(checkDistance, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1));

    encoderCalibrationBtn = new QPushButton(QString::fromUtf8("开始 / 结束校准"));
    encoderCalibrationBtn->setFixedHeight(36);
    encoderCalibrationBtn->setCursor(Qt::PointingHandCursor);
    encoderCalibrationBtn->setStyleSheet(
        "QPushButton{background:#18536e;color:white;border:1px solid #3b7893;border-radius:4px;padding:0 16px;font-weight:600;font-size:14px;}"
        "QPushButton:hover{background:#126aa0;}");
    f->addRow("", encoderCalibrationBtn);

    connect(direction, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int value) { m_params->enc.direction = value; });
    connect(coderDeg, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) { m_params->enc.coderDeg = float(value); });
    connect(checkDistance, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) { m_params->enc.checkDistance = float(value); });
    connect(encoderCalibrationBtn, &QPushButton::clicked, this, &EncoderParamPage::encoderCalibrationRequested);

    layout->addWidget(form);
    layout->addStretch();
}
