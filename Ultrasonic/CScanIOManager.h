#pragma once
#include <QObject>

class CalibrationController;
class CScanEngine;
class HomePage;
class ParamPage;

/// C扫数据 I/O 管理器 — 从 MainWindow 分离的保存/加载/回放/翻页逻辑
class CScanIOManager : public QObject
{
    Q_OBJECT
public:
    CScanIOManager(HomePage *homePage, ParamPage *paramPage,
                   CScanEngine *cScanEngine, CalibrationController *calController,
                   QObject *parent = nullptr);

    /// 回放状态（替代原 AppState::replayState）
    void setReplayActive(bool active);
    bool isReplayActive() const { return m_replayActive; }

public slots:
    void onSaveDataRequested(const QString &path);
    void onReplayDataRequested(const QString &path);
    void onCScanPositionSelected(int line, int column);
    void onCScanViewParamsChanged();
    void onCScanPageRequested();
    void onExitReplayRequested();

signals:
    void statusMessage(const QString &msg);
    void replayStateChanged(bool active);

private:
    HomePage              *m_homePage;
    ParamPage             *m_paramPage;
    CScanEngine           *m_cScanEngine;
    CalibrationController *m_calController;

    bool m_replayActive = false;
    int  m_replayCurPos = 0;
};
