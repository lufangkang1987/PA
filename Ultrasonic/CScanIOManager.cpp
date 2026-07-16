#include "CScanIOManager.h"
#include "CalibrationController.h"
#include "CScanDataCodec.h"
#include "CScanEngine.h"
#include "HomePage.h"
#include "ParamPage.h"
#include <QJsonObject>
#include <QString>

CScanIOManager::CScanIOManager(HomePage *homePage, ParamPage *paramPage,
        CScanEngine *cScanEngine, CalibrationController *calController, QObject *parent)
    : QObject(parent), m_homePage(homePage), m_paramPage(paramPage),
      m_cScanEngine(cScanEngine), m_calController(calController)
{
}

void CScanIOManager::setReplayActive(bool active)
{
    if (m_replayActive != active) {
        m_replayActive = active;
        emit replayStateChanged(active);
    }
}

void CScanIOManager::onSaveDataRequested(const QString &path)
{
    if (!m_homePage || !m_paramPage) return;
    int w = 0, h = 0;
    QVector<float> data = m_homePage->getCScanData(w, h);
    if (data.isEmpty() || w <= 0 || h <= 0) {
        emit statusMessage("无C扫数据可保存");
        return;
    }
    QJsonObject paramsJson = m_paramPage->serializeParams();
    if (saveCScanFile(path, data, w, h, paramsJson,
                      m_cScanEngine->archivedPackets())) {
        emit statusMessage(QString("C扫数据已保存: %1 (%2×%3)")
            .arg(path).arg(w).arg(h));
    } else {
        emit statusMessage("保存C扫数据失败");
    }
}

void CScanIOManager::onReplayDataRequested(const QString &path)
{
    if (!m_homePage || !m_paramPage) return;
    if (m_calController->isCalibrating() || m_calController->isEncoderCalibrating()
            || m_cScanEngine->isScanning()) {
        emit statusMessage("扫查或校准期间不能进入回放");
        return;
    }
    int w = 0, h = 0;
    QJsonObject paramsJson;
    QVector<DataPacket> packets;
    QVector<ScanRule> loadedRules;
    QVector<float> data = loadCScanFile(path, w, h, paramsJson, packets, &loadedRules);
    if (data.isEmpty() || w <= 0 || h <= 0) {
        emit statusMessage("加载C扫数据失败");
        return;
    }
    m_homePage->setCScanReplayData(data, w, h, true);
    m_cScanEngine->setArchivedPackets(packets);
    if (!loadedRules.isEmpty()) m_cScanEngine->setScanRules(loadedRules);
    setReplayActive(true);
    m_replayCurPos = 0;
    m_paramPage->deserializeParams(paramsJson);
    m_paramPage->syncUiFromParams();
    m_homePage->configureCScanView(m_paramPage->params());
    emit statusMessage(QString("C扫回放: %1 (%2×%3)").arg(path).arg(w).arg(h));
}

void CScanIOManager::onCScanPositionSelected(int line, int)
{
    const auto &packets = m_cScanEngine->archivedPackets();
    if (line < 0 || line >= packets.size()) return;
    m_replayCurPos = line;
    m_homePage->showReplayPacket(packets[line], line,
        m_paramPage->params().rx.curBeam, m_paramPage->params().rx.rectify);
}

void CScanIOManager::onCScanViewParamsChanged()
{
    m_homePage->configureCScanView(m_paramPage->params());
}

void CScanIOManager::onCScanPageRequested()
{
    const int count = m_cScanEngine->archivedPackets().size();
    if (count <= 0) return;
    const int maximumStart = qMax(0, count - CScanLinesPerPage);
    int pageStart = m_homePage->cScanPageStart() + CScanLinesPerPage;
    if (pageStart > maximumStart) pageStart = 0;
    m_homePage->setCScanPageStart(pageStart);
    const int line = qBound(0, pageStart + m_paramPage->params().ana.anaLineX1, count - 1);
    m_replayCurPos = line;
    m_homePage->showReplayPacket(m_cScanEngine->archivedPackets()[line], line,
        m_paramPage->params().rx.curBeam, m_paramPage->params().rx.rectify);
}

void CScanIOManager::onExitReplayRequested()
{
    setReplayActive(false);
    m_replayCurPos = 0;
    m_homePage->setCScanReplayMode(false);
    m_homePage->setCScanPageStart(0);
    m_homePage->selectCScanLine(-1);
    emit statusMessage("已退出 C 扫回放");
}
