#pragma once
#include <QFrame>

class QComboBox;
class QLabel;
class QPushButton;

/// 顶部栏控件 — 从 MainWindow::buildHeader() 抽取
///
/// 包含：Logo、标题、版本号、连接模式选择、设备状态、
/// 遥测标签（温度/PC电量/PA电量）、操作按钮（连接/采集）。
/// 提供 widget 访问器供 ConnectionManager 绑定信号。
class HeaderBar : public QFrame
{
    Q_OBJECT
public:
    explicit HeaderBar(QWidget *parent = nullptr);

    QComboBox   *modeCombo()   const { return m_modeCombo; }
    QLabel      *deviceLabel() const { return m_deviceLabel; }
    QLabel      *ipLabel()     const { return m_ipLabel; }
    QPushButton *connectBtn()  const { return m_connectBtn; }
    QPushButton *acquireBtn()  const { return m_acquireBtn; }

    void setTemperature(double celsius);
    void setPaBattery(int percent);
    void updatePcBattery();

private:
    void setupUi();

    QComboBox   *m_modeCombo   = nullptr;
    QLabel      *m_deviceLabel = nullptr;
    QLabel      *m_ipLabel     = nullptr;
    QLabel      *m_temperatureLabel = nullptr;
    QLabel      *m_pcBatteryLabel   = nullptr;
    QLabel      *m_paBatteryLabel   = nullptr;
    QPushButton *m_connectBtn  = nullptr;
    QPushButton *m_acquireBtn  = nullptr;
};
