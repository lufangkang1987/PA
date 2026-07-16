#include "ImagingParamPage.h"
#include "ParamPageUiHelpers.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ImagingParamPage::ImagingParamPage(PAParams *params, QWidget *parent)
    : QWidget(parent), m_params(params)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("C扫成像参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);
    f->setContentsMargins(5, 8, 5, 8);

    auto compactField = [](QWidget *field) {
        field->setMinimumWidth(0);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        if (auto *step = field->findChild<QComboBox *>("StepCombo"))
            step->setFixedWidth(48);
        return field;
    };

    imagingLineSpin[0] = makeParamIntSpin(0, 511, m_params->img.imgLineX1, 10);
    imagingLineSpin[1] = makeParamIntSpin(0, 511, m_params->img.imgLineX2, 10);
    imagingLineSpin[2] = makeParamIntSpin(0, 399, m_params->img.imgLineY1, 10);
    imagingLineSpin[3] = makeParamIntSpin(0, 399, m_params->img.imgLineY2, 10);
    degPerPointSpin = makeParamDoubleSpin(0.1, 5.0, m_params->img.degPerPoint, 0.1, "mm/d");
    f->addRow(QString::fromUtf8("采集线 X1"), compactField(wrapWithStepSelector(imagingLineSpin[0], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1)));
    f->addRow(QString::fromUtf8("采集线 X2"), compactField(wrapWithStepSelector(imagingLineSpin[1], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1)));
    f->addRow(QString::fromUtf8("采集线 Y1"), compactField(wrapWithStepSelector(imagingLineSpin[2], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1)));
    f->addRow(QString::fromUtf8("采集线 Y2"), compactField(wrapWithStepSelector(imagingLineSpin[3], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1)));
    f->addRow(QString::fromUtf8("C扫增量"), compactField(wrapWithStepSelector(degPerPointSpin, {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1)));

    for (int i = 0; i < 4; ++i)
        connect(imagingLineSpin[i], QOverload<int>::of(&QSpinBox::valueChanged), this, [this, i](int value) {
            int *fields[] = {&m_params->img.imgLineX1, &m_params->img.imgLineX2,
                             &m_params->img.imgLineY1, &m_params->img.imgLineY2};
            *fields[i] = value;
            emit cScanViewParamsChanged();
        });
    connect(degPerPointSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { m_params->img.degPerPoint = float(value); emit cScanViewParamsChanged(); });

    layout->addWidget(form);

    auto *btnFrame = new QFrame;
    btnFrame->setObjectName("ScanButtonFrame");
    auto *btnLayout = new QVBoxLayout(btnFrame);
    btnLayout->setContentsMargins(6, 10, 6, 10);
    btnLayout->setSpacing(8);

    scanBtn = new QPushButton(QString::fromUtf8("开始扫描"));
    scanBtn->setObjectName("StartScanButton");
    scanBtn->setFixedHeight(52);
    scanBtn->setCursor(Qt::PointingHandCursor);
    scanBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setScanning(false);
    connect(scanBtn, &QPushButton::clicked, this, &ImagingParamPage::scanButtonClicked);
    btnLayout->addWidget(scanBtn);

    auto *hintLabel = new QLabel(QString::fromUtf8("点击按钮开始/停止 C 扫描数据采集"));
    hintLabel->setObjectName("HintLabel");
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setMinimumWidth(0);
    hintLabel->setWordWrap(true);
    btnLayout->addWidget(hintLabel);

    layout->addWidget(btnFrame);
    layout->addStretch();
}

void ImagingParamPage::setScanning(bool scanning)
{
    if (!scanBtn) return;
    if (scanning) {
        scanBtn->setText(QString::fromUtf8("停止扫描"));
        scanBtn->setStyleSheet(
            "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #c23a0a, stop:1 #e85020);color:white;border:1px solid #e85020;border-radius:8px;font-size:18px;font-weight:700;}"
            "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #d04818, stop:1 #f86030);}"
            "QPushButton:pressed{background:#a03008;}");
    } else {
        scanBtn->setText(QString::fromUtf8("开始扫描"));
        scanBtn->setStyleSheet(
            "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0a8a3a, stop:1 #0bc050);color:white;border:1px solid #0bc050;border-radius:8px;font-size:18px;font-weight:700;}"
            "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0ba048, stop:1 #14e060);}"
            "QPushButton:pressed{background:#087030;}");
    }
}
