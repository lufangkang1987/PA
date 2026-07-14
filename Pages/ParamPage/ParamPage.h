#pragma once
#include <QFrame>
#include <QJsonObject>
#include "PAParams.h"

class QStackedWidget;
class QListWidget;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QPushButton;
class IDriver;

class ParamPage : public QFrame
{
    Q_OBJECT
public:
    explicit ParamPage(QWidget *parent = nullptr);
    void setDriver(IDriver *driver);
    bool initializeParams();
    void applyCurrentParams();

    /// 获取闸门参数（供外部读取后更新 AScanWidget 闸门显示）
    void getGateParams(int gate, bool &enabled, float &start, float &width,
                       float &threshold) const;
    int  activeGate() const { return m_params.gateSelect; }
    const PAParams &params() const { return m_params; }
    void setBeamNo(int beam);
    void setAnalysisRect(int line1, int line2, int column1, int column2);
    void setCalibratedVelocity(int velocity);
    void setCalibratedProbeDelay(float delayUs);
    void setCalibratedACG(const QVector<float> &values);
    void setCalibratedCoderDeg(float mmPerPulse);

private:
    void setupUi();
    void buildTransmitPage();
    void buildReceivePage();
    void buildGatePage();
    void buildProbePage();
    void buildWedgePage();
    void buildWorkpiecePage();
    void buildScanPage();
    void buildTcgPage();

    QWidget* createCategoryPage(const QString &title);
    QDoubleSpinBox* makeDoubleSpin(double min, double max, double val,
                                    double step, const QString &suffix, int decimals = -1);
    QSpinBox* makeIntSpin(int min, int max, int val, int step);
    QComboBox* makeCombo(const QStringList &items, int currentIdx);

    void buildImagingPage();
    void buildEncoderPage();
    void buildAnalysisPage();

    void onScanTypeChanged(int idx);
    void onProbeTypeChanged(int idx);
    void onWedgeTypeChanged(int idx);
    void onMaterialChanged(int idx);
    void onApplyLaw();
    void onSaveParams();
    void onLoadParams();
    void onGateSelectChanged(int idx);
    void onGateParamChanged();
    void onScanButtonClicked();
    void onSaveData();
    void onReplayData();

    /// 根据当前导航页更新 C扫数据按钮的 enabled 状态
    void updateCScanButtons();
public:
    /// 将当前参数序列化为 JSON（供外部保存 C扫数据时写入文件头）
    QJsonObject serializeParams() const;
    /// 从 JSON 恢复参数 + 同步到 UI 控件（供回放 C扫数据时恢复采集参数）
    void deserializeParams(const QJsonObject &json);
    void syncUiFromParams();

public:
    /// A扫拖拽闸门回调（由 MainWindow 连接）
    void onGateDragged(int gate, float start, float threshold);
    void finishScan();

signals:
    /// 闸门参数变化（任一门的 start/width/threshold 变化时发出）
    void gateParamsChanged();
    /// C扫成像页"开始扫描"/"停止扫描"
    void scanStarted();
    void scanStopped();
    /// 保存数据 / 回放数据（由 MainWindow 连接处理，携带文件路径）
    void saveDataRequested(const QString &filePath);
    void saveLegacyDataRequested(const QString &filePath);
    void replayDataRequested(const QString &filePath);
    void cScanPageRequested();
    void exitReplayRequested();
    /// 当前声束号 / 增益变化 → 右侧 MeasurePage 读数
    void beamInfoChanged(int beamNo, double gainDb);
    void calibrationRequested(int item);
    void encoderCalibrationRequested();
    void cScanViewParamsChanged();
    void legacyParamsLoadRequested(const QString &filePath);
    void legacyParamsSaveRequested(const QString &filePath);

private:
    QListWidget   *m_nav      = nullptr;
    QStackedWidget *m_stack   = nullptr;

    // 闸门页控件
    QComboBox      *m_gateSelCombo    = nullptr;
    QDoubleSpinBox *m_gateStartSpin   = nullptr;
    QDoubleSpinBox *m_gateWidthSpin   = nullptr;
    QDoubleSpinBox *m_gateThreshSpin  = nullptr;
    QComboBox      *m_gateMeasureCombo = nullptr;
    QComboBox      *m_gateAlarmCombo  = nullptr;  // 逐闸门报警开关 (gateAlarm[g])
    QComboBox      *m_gateTraceCombo  = nullptr;  // 逐闸门跟踪开关 (gateTrace[g])
    QComboBox      *m_alarmSoundCombo = nullptr;  // 全局蜂鸣报警声 (alarmSound)

    // 扫查页面动态控件
    QLabel        *m_scanLabels[7]   = {};
    QWidget       *m_scanWidgets[7]  = {};
    QComboBox     *m_scanTypeCombo   = nullptr;

    // 探头联动
    QComboBox     *m_probeTypeCombo  = nullptr;
    QDoubleSpinBox *m_probeFreqSpin  = nullptr;
    QSpinBox      *m_probeCountSpin  = nullptr;
    QDoubleSpinBox *m_probePitchSpin = nullptr;

    // 楔块联动
    QComboBox     *m_wedgeEnableCombo = nullptr;
    QComboBox     *m_wedgeTypeCombo  = nullptr;
    QDoubleSpinBox *m_wedgeAngleSpin = nullptr;
    QSpinBox      *m_wedgeVelSpin    = nullptr;
    QDoubleSpinBox *m_wedgeHeightSpin = nullptr;

    // 工件联动
    QComboBox     *m_materialCombo   = nullptr;
    QSpinBox      *m_lVelSpin        = nullptr;
    QComboBox     *m_traceEnableCombo = nullptr;

    // 发射页
    QComboBox     *m_voltCombo       = nullptr;
    QSpinBox      *m_pulseWidthSpin  = nullptr;  // makeIntSpin
    QSpinBox      *m_prfSpin         = nullptr;  // makeIntSpin
    QDoubleSpinBox *m_rangeSpin      = nullptr;
    QComboBox     *m_tempCorrectCombo = nullptr;
    QComboBox     *m_aDataLenCombo   = nullptr;

    // 接收页
    QDoubleSpinBox *m_aGainSpin      = nullptr;
    QDoubleSpinBox *m_dGainSpin      = nullptr;
    QSpinBox      *m_beamNoSpin      = nullptr;
    QComboBox     *m_rectifyCombo    = nullptr;
    QComboBox     *m_filterCombo     = nullptr;
    QComboBox     *m_videoCombo      = nullptr;

    // 硬件下发
    void syncToDriver(char category);

    // C扫扫描按钮
    QPushButton   *m_scanBtn         = nullptr;
    bool           m_scanning        = false;

    // C扫数据按钮（仅在成像/编码器/分析页可用）
    QPushButton   *m_saveDataBtn     = nullptr;
    QPushButton   *m_replayDataBtn   = nullptr;
    QPushButton   *m_calibrationBtn  = nullptr;
    QPushButton   *m_encoderCalibrationBtn = nullptr;
    QSpinBox      *m_imagingLineSpin[4] = {};
    QDoubleSpinBox *m_degPerPointSpin = nullptr;
    QSpinBox      *m_analysisLineSpin[4] = {};

    // 导航折叠状态
    int            m_activeRow       = -1;  // 当前展开的导航项，-1=全部折叠

    PAParams m_params;
    IDriver *m_driver = nullptr;
};
