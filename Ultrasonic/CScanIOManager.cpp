#include "CScanIOManager.h"
#include "CScanDataCodec.h"
#include "CScanEngine.h"
#include "PAParams.h"
#include <QJsonObject>
#include <QString>

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
    return ::saveCScanFile(path, data, w, h, params, packets);
}

void CScanIOManager::onReplayDataRequested(const QString &path)
{
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
    const int maximumStart = qMax(0, count - CScanLinesPerPage);
    int ps = m_cScanPageStart + CScanLinesPerPage;
    if (ps > maximumStart) ps = 0;
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
