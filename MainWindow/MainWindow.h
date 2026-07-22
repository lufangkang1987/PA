#pragma once
#include <QMainWindow>
#include "DataTypes.h"
#include <memory>

class QVBoxLayout;
class QThread;
class HomePage;
class ParamPage;
class MeasurePage;
class IDriver;
class CalibrationController;
class ConnectionManager;
class CScanIOManager;
class CScanEngine;
class DataPacketProcessor;
class HeaderBar;

/// 应用工作模式——确保扫描/校准/回放互斥
enum class AppMode { Idle, Scanning, Calibrating, Replaying };

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void buildHeader(QVBoxLayout *root);
    void buildCentral(QVBoxLayout *root);
    void buildDriverAndEngine();
    void wirePageSignals();
    void wireCScanIO();
    void wireCalibration();
    void applyGlobalStyleSheet();
    void wireDriverSignals();
    void syncGateDisplay();
    bool loadParamsFile(const QString &path);
    bool enterMode(AppMode mode);
    void leaveMode(AppMode mode);

    AppMode m_appMode = AppMode::Idle;

    HomePage     *m_homePage = nullptr;
    ParamPage    *m_paramPage = nullptr;
    MeasurePage  *m_measurePage = nullptr;
    IDriver      *m_driver = nullptr;
    CScanEngine  *m_cScanEngine = nullptr;
    QThread      *m_cScanThread = nullptr;

    CalibrationController *m_calController = nullptr;
    ConnectionManager     *m_connManager = nullptr;
    CScanIOManager        *m_ioManager = nullptr;
    DataPacketProcessor   *m_dataProcessor = nullptr;

    HeaderBar *m_headerBar = nullptr;
};
