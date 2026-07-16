#include "ParamPage.h"
#include "ProbeParamPage.h"

#include <QStackedWidget>

void ParamPage::buildProbePage()
{
    m_probePage = new ProbeParamPage(&m_params, this);
    m_probeTypeCombo = m_probePage->probeTypeCombo;
    m_probeFreqSpin = m_probePage->probeFreqSpin;
    m_probeCountSpin = m_probePage->probeCountSpin;
    m_probePitchSpin = m_probePage->probePitchSpin;
    m_stack->addWidget(m_probePage);
}
