#include "ScanParamPage.h"
#include "ParamPageUiHelpers.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>

ScanParamPage::ScanParamPage(PAParams *params, QWidget *parent)
    : QWidget(parent), m_params(params)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("扫查参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    scanTypeCombo = makeParamCombo({QString::fromUtf8("S 扫"), QString::fromUtf8("L 扫")}, m_params->scan.scanType);
    connect(scanTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_params->scan.scanType = idx;
        rebuildForScanType(idx);
    });
    f->addRow(QString::fromUtf8("扫查方式"), scanTypeCombo);

    for (int i = 1; i <= 6; ++i) {
        scanLabels[i] = new QLabel;
        scanWidgets[i] = new QWidget;
        f->addRow(scanLabels[i], scanWidgets[i]);
    }

    rebuildForScanType(m_params->scan.scanType);

    layout->addWidget(form);
    layout->addStretch();
}

void ScanParamPage::rebuildForScanType(int idx)
{
    struct SlotDef { const char *label; const char *unit; bool visible; };
    SlotDef defs[2][6] = {
        {{"起始阵元","",true},{"结束阵元","",true},{"孔径大小","",true},{"起始角度","\u00B0",true},{"结束角度","\u00B0",true},{"焦距","mm",true}},
        {{"起始阵元","",true},{"结束阵元","",true},{"孔径大小","",true},{"角度","\u00B0",true},{"焦距","mm",true},{nullptr,nullptr,false}}
    };
    idx = qBound(0, idx, 1);

    for (int i = 1; i <= 6; ++i) {
        const auto &d = defs[idx][i - 1];
        if (!d.label) {
            scanLabels[i]->setText("");
            scanWidgets[i]->setVisible(false);
            continue;
        }
        scanLabels[i]->setText(QString::fromUtf8(d.label));
        scanWidgets[i]->setVisible(d.visible);

        delete scanWidgets[i]->layout();
        auto *row = new QHBoxLayout(scanWidgets[i]);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);

        QWidget *wrappedField = nullptr;
        if (idx == 0) {
            switch (i) {
            case 1: wrappedField = wrapWithStepSelector(makeParamIntSpin(1, 128, m_params->scan.eleStart, 1), {"1"}, {1.0}, 0); break;
            case 2: wrappedField = wrapWithStepSelector(makeParamIntSpin(1, 128, m_params->scan.eleEnd, 1), {"1"}, {1.0}, 0); break;
            case 3: wrappedField = wrapWithStepSelector(makeParamIntSpin(1, 16, m_params->scan.eleAperture, 1), {"1"}, {1.0}, 0); break;
            case 4: wrappedField = wrapWithStepSelector(makeParamDoubleSpin(-89.0, 89.0, m_params->scan.angleFrom, 1.0, "\u00B0"), {"1.0", "10.0"}, {1.0, 10.0}, 0); break;
            case 5: wrappedField = wrapWithStepSelector(makeParamDoubleSpin(-89.0, 89.0, m_params->scan.angleTo, 1.0, "\u00B0"), {"1.0", "10.0"}, {1.0, 10.0}, 0); break;
            case 6: wrappedField = wrapWithStepSelector(makeParamDoubleSpin(2.0, 1000.0, m_params->scan.focus, 0.1, "mm"), {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1); break;
            }
        } else {
            switch (i) {
            case 1: wrappedField = wrapWithStepSelector(makeParamIntSpin(1, 128, m_params->scan.eleStart, 1), {"1"}, {1.0}, 0); break;
            case 2: wrappedField = wrapWithStepSelector(makeParamIntSpin(1, 128, m_params->scan.eleEnd, 1), {"1"}, {1.0}, 0); break;
            case 3: wrappedField = wrapWithStepSelector(makeParamIntSpin(1, 16, m_params->scan.eleAperture, 1), {"1"}, {1.0}, 0); break;
            case 4: wrappedField = wrapWithStepSelector(makeParamDoubleSpin(-89.0, 89.0, m_params->scan.angle, 1.0, "\u00B0"), {"1.0", "10.0"}, {1.0, 10.0}, 0); break;
            case 5: wrappedField = wrapWithStepSelector(makeParamDoubleSpin(2.0, 1000.0, m_params->scan.focus, 0.1, "mm"), {"0.1", "1.0", "10.0"}, {0.1, 1.0, 10.0}, 1); break;
            }
        }

        if (!wrappedField) continue;
        if (auto *intField = wrappedField->findChild<QSpinBox *>()) {
            connect(intField, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, i](int value) {
                if (i == 1) m_params->scan.eleStart = value;
                else if (i == 2) m_params->scan.eleEnd = value;
                else if (i == 3) m_params->scan.eleAperture = value;
            });
        } else if (auto *doubleField = wrappedField->findChild<QDoubleSpinBox *>()) {
            connect(doubleField, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, idx, i](double value) {
                if (idx == 0 && i == 4) m_params->scan.angleFrom = static_cast<float>(value);
                else if (idx == 0 && i == 5) m_params->scan.angleTo = static_cast<float>(value);
                else if (idx == 0 && i == 6) m_params->scan.focus = static_cast<float>(value);
                else if (idx == 1 && i == 4) m_params->scan.angle = static_cast<float>(value);
                else if (idx == 1 && i == 5) m_params->scan.focus = static_cast<float>(value);
            });
        }
        row->addWidget(wrappedField);
    }
}
