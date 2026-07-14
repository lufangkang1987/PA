#pragma once
#include <QMainWindow>
#include "DataTypes.h"

class QComboBox;
class QLabel;
class QPushButton;
class HomePage;
class ParamPage;
class MeasurePage;
class IDriver;
class CScanEngine;
class QThread;

bool runCScanCodecSelfTest(QString *errorMessage = nullptr);

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
    void wireDriverSignals();
    bool loadParamsFile(const QString &path);
    void updatePcBattery();

    HomePage     *m_homePage = nullptr;
    ParamPage    *m_paramPage = nullptr;
    MeasurePage  *m_measurePage = nullptr;
    IDriver      *m_driver = nullptr;
    CScanEngine  *m_cScanEngine = nullptr;
    QThread      *m_cScanThread = nullptr;
    DataPacket    m_latestPacket;
    QVector<double> m_scanRulePositions;
    bool          m_hasLatestPacket = false;
    bool          m_calibrating = false;
    int           m_calibrationItem = -1;
    int           m_calibrationTargetPercent = 80;
    bool          m_encoderCalibrating = false;
    int           m_encoderCalibrationStart = 0;

    // 连接模式 UI
    QComboBox    *m_modeCombo = nullptr;
    QLabel       *m_deviceLabel = nullptr;
    QLabel       *m_ipLabel = nullptr;
    QLabel       *m_temperatureLabel = nullptr;
    QLabel       *m_pcBatteryLabel = nullptr;
    QLabel       *m_paBatteryLabel = nullptr;

    // 操作按钮
    QPushButton  *m_connectBtn = nullptr;
    QPushButton  *m_acquireBtn = nullptr;
};
