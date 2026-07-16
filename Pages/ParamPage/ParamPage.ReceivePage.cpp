#include "ParamPage.h"
#include "ReceiveParamPage.h"
#include <QStackedWidget>

void ParamPage::buildReceivePage()
{
    m_receivePage = new ReceiveParamPage(&m_params, m_dispatcher, this);
    m_aGainSpin = m_receivePage->aGainSpin;
    m_dGainSpin = m_receivePage->dGainSpin;
    m_beamNoSpin = m_receivePage->beamNoSpin;
    m_rectifyCombo = m_receivePage->rectifyCombo;
    m_filterCombo = m_receivePage->filterCombo;
    m_videoCombo = m_receivePage->videoCombo;
    connect(m_receivePage, &ReceiveParamPage::beamInfoChanged,
            this, &ParamPage::beamInfoChanged);
    m_stack->addWidget(m_receivePage);
}
