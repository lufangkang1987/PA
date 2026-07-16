#include "ParamPage.h"
#include "WorkpieceParamPage.h"

#include <QStackedWidget>
#include <QtGlobal>

void ParamPage::buildWorkpiecePage()
{
    m_workpiecePage = new WorkpieceParamPage(&m_params, this);
    m_materialCombo = m_workpiecePage->materialCombo;
    m_lVelSpin = m_workpiecePage->lVelSpin;
    m_traceEnableCombo = m_workpiecePage->traceEnableCombo;
    m_stack->addWidget(m_workpiecePage);
}

void ParamPage::setCalibratedVelocity(int velocity)
{
    if (m_workpiecePage)
        m_workpiecePage->setVelocity(velocity);
    else
        m_params.wp.lVelocity = qBound(m_params.wedge.wedgeVelocity, velocity, 9000);
}
