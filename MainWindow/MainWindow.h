#pragma once
#include <QMainWindow>
#include "DataTypes.h"
#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class HomePage;
class ParamPage;
class MeasurePage;
class IDriver;
class CalibrationController;
class ConnectionManager;
class CScanIOManager;
class CScanEngine;
class QThread;

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
    void buildHeader(QWidget *shell, QVBoxLayout *root);
    void buildCentral(QVBoxLayout *root);
    void buildDriverAndEngine();
    void wirePageSignals();
    void wireCScanIO();
    void wireCalibration();
    void applyGlobalStyleSheet();
    void wireDriverSignals();
    bool loadParamsFile(const QString &path);
    void updatePcBattery();

    HomePage     *m_homePage = nullptr;
    ParamPage    *m_paramPage = nullptr;
    MeasurePage  *m_measurePage = nullptr;
    IDriver      *m_driver = nullptr;
    CScanEngine  *m_cScanEngine = nullptr;
    QThread      *m_cScanThread = nullptr;
    std::shared_ptr<DataPacket> m_latestPacket;
    QVector<double> m_scanRulePositions;
    bool          m_hasLatestPacket = false;

    CalibrationController *m_calController = nullptr;
    ConnectionManager     *m_connManager = nullptr;
    CScanIOManager        *m_ioManager = nullptr;

    // 顶部栏控件（UI 属 MainWindow，信号委托给 ConnectionManager）
    QComboBox *m_modeCombo = nullptr;
    QLabel    *m_deviceLabel = nullptr;
    QLabel    *m_ipLabel = nullptr;

    // 遥测标签
    QLabel *m_temperatureLabel = nullptr;
    QLabel *m_pcBatteryLabel = nullptr;
    QLabel *m_paBatteryLabel = nullptr;
};
