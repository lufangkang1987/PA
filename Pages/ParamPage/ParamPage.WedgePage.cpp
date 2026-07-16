#include "ParamPage.h"
#include "WedgeParamPage.h"

#include <QStackedWidget>

void ParamPage::buildWedgePage()
{
    m_wedgePage = new WedgeParamPage(&m_params, this);
    m_wedgeEnableCombo = m_wedgePage->wedgeEnableCombo;
    m_wedgeTypeCombo = m_wedgePage->wedgeTypeCombo;
    m_wedgeAngleSpin = m_wedgePage->wedgeAngleSpin;
    m_wedgeVelSpin = m_wedgePage->wedgeVelSpin;
    m_wedgeHeightSpin = m_wedgePage->wedgeHeightSpin;
    m_stack->addWidget(m_wedgePage);
}
