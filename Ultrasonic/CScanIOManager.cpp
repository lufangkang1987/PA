#include "CScanIOManager.h"
#include "CScanDataCodec.h"
#include "CScanEngine.h"
#include "PAParams.h"
#include <QJsonObject>
#include <QString>
#include "Logging/Logger.h"

CScanIOManager::CScanIOManager(CScanEngine *cScanEngine, QObject *parent)
    : QObject(parent), m_cScanEngine(cScanEngine)
{
}

void CScanIOManager::setReplayActive(bool active)
{
    if (m_replayActive != active) {
        m_replayActive = active;
        emit replayStateChanged(active);
    }
}

bool CScanIOManager::saveToFile(const QString &path, const QVector<float> &data,
                                 int w, int h, const QJsonObject &params,
                                 const QVector<DataPacket> &packets) const
{
    PA_LOG_INFO("STORAGE", QStringLiteral("Saving C-scan path=%1 width=%2 height=%3 packets=%4")
                .arg(path).arg(w).arg(h).arg(packets.size()));
    const bool ok = ::saveCScanFile(path, data, w, h, params, packets);
    if (ok)
        PA_LOG_INFO("STORAGE", QStringLiteral("C-scan saved path=%1").arg(path));
    else
        PA_LOG_ERROR("STORAGE", QStringLiteral("C-scan save failed path=%1").arg(path));
    return ok;
}

void CScanIOManager::onReplayDataRequested(const QString &path)
{
    PA_LOG_INFO("STORAGE", QStringLiteral("Loading replay path=%1").arg(path));
    if (m_cScanEngine->isScanning()) {
        emit statusMessage("扫查期间不能进入回放");
        return;
    }
    int w = 0, h = 0;
    QJsonObject paramsJson;
    QVector<DataPacket> packets;
    QVector<ScanRule> loadedRules;
    QVector<float> data = loadCScanFile(path, w, h, paramsJson, packets, &loadedRules);
    if (data.isEmpty() || w <= 0 || h <= 0) {
        PA_LOG_ERROR("STORAGE", QStringLiteral("Replay load failed path=%1").arg(path));
        emit statusMessage("加载C扫数据失败");
        return;
    }
    m_cScanEngine->setArchivedPackets(packets);
    if (!loadedRules.isEmpty())
        m_cScanEngine->setScanRules(loadedRules);
    setReplayActive(true);
    m_replayCurPos = 0;
    m_cScanPageStart = 0;
    emit replayDataReady(data, w, h, paramsJson, packets, loadedRules);
    PA_LOG_INFO("STORAGE", QStringLiteral("Replay loaded path=%1 width=%2 height=%3 packets=%4 rules=%5")
                .arg(path).arg(w).arg(h).arg(packets.size()).arg(loadedRules.size()));
    emit statusMessage(QString("C扫回放: %1 (%2×%3)").arg(path).arg(w).arg(h));
}

void CScanIOManager::onCScanPositionSelected(int line, int)
{
    auto packets = m_cScanEngine->archivedPackets();
    if (!packets || line < 0 || line >= packets->size()) return;
    m_replayCurPos = line;
    emit replayPacketRequested((*packets)[line], line,
                               m_params->rx.curBeam, m_params->rx.rectify);
}

void CScanIOManager::onCScanPageRequested()
{
    auto packets = m_cScanEngine->archivedPackets();
    if (!packets) return;
    const int count = packets->size();
    if (count <= 0) return;
    int ps = m_cScanPageStart + CScanLinesPerPage;
    // MFC CScanShift advances by a complete 925-line page.  The final page
    // may contain fewer than 925 lines; only wrap after its start passes the
    // last captured line.
    if (ps > count - 1) ps = 0;
    const int line = qBound(0, ps + m_params->ana.anaLineX1, count - 1);
    m_replayCurPos = line;
    m_cScanPageStart = ps;
    emit replayUISetPageStart(ps);
    emit replayPacketRequested((*packets)[line], line,
                               m_params->rx.curBeam, m_params->rx.rectify);
}

void CScanIOManager::onExitReplayRequested()
{
    setReplayActive(false);
    m_replayCurPos = 0;
    m_cScanPageStart = 0;
    emit replayUISetMode(false);
    emit replayUISetPageStart(0);
    emit replayUISelectLine(-1);
    emit statusMessage("已退出 C 扫回放");
}
