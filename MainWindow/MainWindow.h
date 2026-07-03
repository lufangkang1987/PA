#pragma once
#include <QMainWindow>

class QTabWidget;
class QComboBox;
class QLabel;
class QPushButton;
class HomePage;
class ParamPage;
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

    QTabWidget   *m_tabs = nullptr;
    HomePage     *m_homePage = nullptr;
    ParamPage    *m_paramPage = nullptr;
    IDriver      *m_driver = nullptr;

    // 连接模式 UI
    QComboBox    *m_modeCombo = nullptr;
    QLabel       *m_deviceLabel = nullptr;
    QLabel       *m_ipLabel = nullptr;

    // 操作按钮
    QPushButton  *m_connectBtn = nullptr;
    QPushButton  *m_acquireBtn = nullptr;
};
