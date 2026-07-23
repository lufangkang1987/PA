#include "ParamPage.h"
#include "ImagingParamPage.h"
#include "ScanParamPage.h"
#include "ReceiveParamPage.h"

#include <QStackedWidget>

void ParamPage::buildScanPage()
{
    m_scanPage = new ScanParamPage(&m_params, this);
    m_scanTypeCombo = m_scanPage->scanTypeCombo;
    for (int i = 1; i <= 6; ++i) {
        m_scanLabels[i] = m_scanPage->scanLabels[i];
        m_scanWidgets[i] = m_scanPage->scanWidgets[i];
    }
    connect(m_scanPage, &ScanParamPage::beamGeometryChanged, this, [this] {
        if (m_receivePage)
            m_receivePage->updateBeamNoRange();
    });
    m_stack->addWidget(m_scanPage);
}

void ParamPage::onScanTypeChanged(int idx)
{
    if (m_scanPage)
        m_scanPage->rebuildForScanType(idx);
    if (m_receivePage)
        m_receivePage->updateBeamNoRange();
}

void ParamPage::onScanButtonClicked()
{
    m_scanning = !m_scanning;
    if (m_imagingPage)
        m_imagingPage->setScanning(m_scanning);

    if (m_scanning)
        emit scanStarted();
    else
        emit scanStopped();
}

void ParamPage::finishScan()
{
    if (m_scanning)
        onScanButtonClicked();
}
