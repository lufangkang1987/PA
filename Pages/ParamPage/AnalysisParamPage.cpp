#include "AnalysisParamPage.h"
#include "ParamPageUiHelpers.h"

#include <QGroupBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>

AnalysisParamPage::AnalysisParamPage(PAParams *params, QWidget *parent)
    : QWidget(parent), m_params(params)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("C扫分析参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    analysisLineSpin[0] = makeParamIntSpin(0, 924, m_params->ana.anaLineX1, 10);
    analysisLineSpin[1] = makeParamIntSpin(0, 924, m_params->ana.anaLineX2, 10);
    analysisLineSpin[2] = makeParamIntSpin(0, 249, m_params->ana.anaLineY1, 10);
    analysisLineSpin[3] = makeParamIntSpin(0, 249, m_params->ana.anaLineY2, 10);
    const char *labels[] = {"测量线 X1", "测量线 X2", "测量线 Y1", "测量线 Y2"};
    for (int i = 0; i < 4; ++i) {
        f->addRow(QString::fromUtf8(labels[i]), wrapWithStepSelector(analysisLineSpin[i], {"1", "10", "100"}, {1.0, 10.0, 100.0}, 1));
        connect(analysisLineSpin[i], QOverload<int>::of(&QSpinBox::valueChanged), this, [this, i](int value) {
            int *fields[] = {&m_params->ana.anaLineX1, &m_params->ana.anaLineX2,
                             &m_params->ana.anaLineY1, &m_params->ana.anaLineY2};
            *fields[i] = value;
            emit cScanViewParamsChanged();
        });
    }

    const QString btnStyle =
        "QPushButton{background:#18536e;color:white;border:1px solid #3b7893;border-radius:4px;padding:0 16px;font-weight:600;font-size:14px;}"
        "QPushButton:hover{background:#126aa0;}";

    auto *pageBtn = new QPushButton(QString::fromUtf8("C扫翻页"));
    pageBtn->setFixedHeight(36);
    pageBtn->setCursor(Qt::PointingHandCursor);
    pageBtn->setStyleSheet(btnStyle);
    connect(pageBtn, &QPushButton::clicked, this, &AnalysisParamPage::cScanPageRequested);
    f->addRow("", pageBtn);

    auto *exitBtn = new QPushButton(QString::fromUtf8("退出回放"));
    exitBtn->setFixedHeight(36);
    exitBtn->setCursor(Qt::PointingHandCursor);
    exitBtn->setStyleSheet(btnStyle);
    connect(exitBtn, &QPushButton::clicked, this, &AnalysisParamPage::exitReplayRequested);
    f->addRow("", exitBtn);

    layout->addWidget(form);
    layout->addStretch();
}

void AnalysisParamPage::setAnalysisRect(int line1, int line2, int column1, int column2)
{
    m_params->ana.anaLineX1 = line1;
    m_params->ana.anaLineX2 = line2;
    m_params->ana.anaLineY1 = column1;
    m_params->ana.anaLineY2 = column2;
    const int values[] = {line1, line2, column1, column2};
    for (int i = 0; i < 4; ++i) {
        if (!analysisLineSpin[i]) continue;
        QSignalBlocker blocker(analysisLineSpin[i]);
        analysisLineSpin[i]->setValue(values[i]);
    }
}
