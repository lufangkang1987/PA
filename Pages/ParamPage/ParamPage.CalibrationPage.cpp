#include "ParamPage.h"
#include "ParameterDispatcher.h"
#include "TcgParamPage.h"

#include <QStackedWidget>
#include <QtGlobal>

void ParamPage::buildTcgPage()
{
    m_tcgPage = new TcgParamPage(&m_params, this);
    m_stack->addWidget(m_tcgPage);
}

void ParamPage::setCalibratedProbeDelay(float delayUs)
{
    m_params.probe.probeDelay = qBound(0.0f, delayUs, 100.0f);
    m_params.tcg.beamDelay = m_params.probe.probeDelay;
}

void ParamPage::setCalibratedACG(const QVector<float> &values)
{
    const int count = qMin(values.size(), MaxBeams);
    for (int i = 0; i < count; ++i)
        m_params.tcg.acgValue[i] = qBound(0.0f, values[i], 256.0f);
    m_params.tcg.acgSwitch = 1;
}

void ParamPage::setCalibratedCoderDeg(float mmPerPulse)
{
    if (mmPerPulse > 0.0f) m_params.enc.coderDeg = mmPerPulse;
}

void ParamPage::onApplyLaw()
{
    if (m_dispatcher) m_dispatcher->applyLaw(m_params);
}

void ParamPage::applyCurrentParams()
{
    onApplyLaw();
}
