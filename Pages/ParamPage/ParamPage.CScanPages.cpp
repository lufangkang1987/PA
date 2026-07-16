#include "ParamPage.h"
#include "AnalysisParamPage.h"
#include "EncoderParamPage.h"
#include "ImagingParamPage.h"

#include <QStackedWidget>

void ParamPage::buildImagingPage()
{
    m_imagingPage = new ImagingParamPage(&m_params, this);
    for (int i = 0; i < 4; ++i)
        m_imagingLineSpin[i] = m_imagingPage->imagingLineSpin[i];
    m_degPerPointSpin = m_imagingPage->degPerPointSpin;
    m_scanBtn = m_imagingPage->scanBtn;
    connect(m_imagingPage, &ImagingParamPage::scanButtonClicked,
            this, &ParamPage::onScanButtonClicked);
    connect(m_imagingPage, &ImagingParamPage::cScanViewParamsChanged,
            this, &ParamPage::cScanViewParamsChanged);
    m_stack->addWidget(m_imagingPage);
}

void ParamPage::buildEncoderPage()
{
    m_encoderPage = new EncoderParamPage(&m_params, this);
    m_encoderCalibrationBtn = m_encoderPage->encoderCalibrationBtn;
    connect(m_encoderPage, &EncoderParamPage::encoderCalibrationRequested,
            this, &ParamPage::encoderCalibrationRequested);
    m_stack->addWidget(m_encoderPage);
}

void ParamPage::buildAnalysisPage()
{
    m_analysisPage = new AnalysisParamPage(&m_params, this);
    for (int i = 0; i < 4; ++i)
        m_analysisLineSpin[i] = m_analysisPage->analysisLineSpin[i];
    connect(m_analysisPage, &AnalysisParamPage::cScanViewParamsChanged,
            this, &ParamPage::cScanViewParamsChanged);
    connect(m_analysisPage, &AnalysisParamPage::cScanPageRequested,
            this, &ParamPage::cScanPageRequested);
    connect(m_analysisPage, &AnalysisParamPage::exitReplayRequested,
            this, &ParamPage::exitReplayRequested);
    m_stack->addWidget(m_analysisPage);
}

void ParamPage::setAnalysisRect(int line1, int line2, int column1, int column2)
{
    if (m_analysisPage)
        m_analysisPage->setAnalysisRect(line1, line2, column1, column2);
}
