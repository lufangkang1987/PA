#include "ParamPage.h"
#include "ProbeParamPage.h"
#include "ReceiveParamPage.h"

#include <QStackedWidget>
#include <QSpinBox>

void ParamPage::buildProbePage()
{
    m_probePage = new ProbeParamPage(&m_params, this);
    m_probeTypeCombo = m_probePage->probeTypeCombo;
    m_probeFreqSpin = m_probePage->probeFreqSpin;
    m_probeCountSpin = m_probePage->probeCountSpin;
    m_probePitchSpin = m_probePage->probePitchSpin;
    // CL扫声束数依赖阵元数，变化时同步更新声束号上限
    connect(m_probeCountSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (m_receivePage) m_receivePage->updateBeamNoRange();
    });
    m_stack->addWidget(m_probePage);
}
