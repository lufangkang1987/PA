#pragma once
#include <QMainWindow>

class QComboBox;
class QLabel;
class QPushButton;
class HomePage;
class ParamPage;
class MeasurePage;
class IDriver;

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

    HomePage     *m_homePage = nullptr;
    ParamPage    *m_paramPage = nullptr;
    MeasurePage  *m_measurePage = nullptr;
    IDriver      *m_driver = nullptr;

    // 连接模式 UI
    QComboBox    *m_modeCombo = nullptr;
    QLabel       *m_deviceLabel = nullptr;
    QLabel       *m_ipLabel = nullptr;

    // 操作按钮
    QPushButton  *m_connectBtn = nullptr;
    QPushButton  *m_acquireBtn = nullptr;
};
