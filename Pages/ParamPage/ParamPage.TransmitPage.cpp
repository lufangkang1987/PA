#include "ParamPage.h"
#include "TransmitParamPage.h"
#include <QStackedWidget>

void ParamPage::buildTransmitPage()
{
    m_transmitPage = new TransmitParamPage(&m_params, m_dispatcher, this);
    m_voltCombo = m_transmitPage->voltCombo;
    m_pulseWidthSpin = m_transmitPage->pulseWidthSpin;
    m_prfSpin = m_transmitPage->prfSpin;
    m_rangeSpin = m_transmitPage->rangeSpin;
    m_tempCorrectCombo = m_transmitPage->tempCorrectCombo;
    m_aDataLenCombo = m_transmitPage->aDataLenCombo;
    m_stack->addWidget(m_transmitPage);
}
