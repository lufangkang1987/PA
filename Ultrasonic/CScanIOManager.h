#pragma once
#include "DataTypes.h"
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

class CScanEngine;
struct PAParams;

/// C扫数据 I/O + 回放状态管理 — 通过信号与 UI 通信
///
/// 依赖：CScanEngine（同层）+ PAParams（数据层）
/// 不依赖任何 Pages/ 或 Widgets/ 层
class CScanIOManager : public QObject
{
    Q_OBJECT
public:
    explicit CScanIOManager(CScanEngine *cScanEngine, QObject *parent = nullptr);

    void setParams(const PAParams *params) { m_params = params; }
    bool isReplayActive() const { return m_replayActive; }
    void setReplayActive(bool active);

    // 纯文件 I/O（无 UI 依赖）
    bool saveToFile(const QString &path, const QVector<float> &data,
                    int w, int h, const QJsonObject &params,
                    const QVector<DataPacket> &packets) const;

    // 由 MainWindow 在 HomePage 翻页后同步回来
    void setPageStart(int pageStart) { m_cScanPageStart = pageStart; }
    int  pageStart() const { return m_cScanPageStart; }

public slots:
    void onReplayDataRequested(const QString &path);
    void onCScanPositionSelected(int line, int column);
    void onCScanPageRequested();
    void onExitReplayRequested();

signals:
    void statusMessage(const QString &msg);
    void replayStateChanged(bool active);

    // UI 操作请求 → MainWindow 连接
    void replayDataReady(const QVector<float> &data, int w, int h,
                         const QJsonObject &params,
                         const QVector<DataPacket> &packets,
                         const QVector<ScanRule> &rules);
    void replayPacketRequested(const DataPacket &packet, int line,
                               int beam, int rectifyMode);
    void replayViewConfigNeeded(const PAParams &params);
    void replayUISetMode(bool replayOn);
    void replayUISetPageStart(int pageStart);
    void replayUISelectLine(int line);

private:
    CScanEngine   *m_cScanEngine = nullptr;
    const PAParams *m_params = nullptr;

    bool m_replayActive = false;
    int  m_replayCurPos = 0;
    int  m_cScanPageStart = 0;
};
