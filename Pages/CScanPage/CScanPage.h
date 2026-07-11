#pragma once
#include <QWidget>
#include "PAParams.h"

class QStackedWidget;
class QListWidget;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QPushButton;
class IDriver;

class CScanPage : public QWidget
{
    Q_OBJECT
public:
    explicit CScanPage(QWidget *parent = nullptr);
    void setDriver(IDriver *driver);

signals:
    void scanStarted();
    void scanStopped();

private:
    void setupUi();
    void buildImagingPage();
    void buildEncoderPage();
    void buildAnalysisPage();

    QWidget* createCategoryPage(const QString &title);
    QDoubleSpinBox* makeDoubleSpin(double min, double max, double val,
                                    double step, const QString &suffix, int decimals = -1);
    QSpinBox* makeIntSpin(int min, int max, int val, int step);
    QComboBox* makeCombo(const QStringList &items, int currentIdx);

    void onScanButtonClicked();

    QListWidget    *m_nav      = nullptr;
    QStackedWidget *m_stack    = nullptr;
    QPushButton    *m_scanBtn  = nullptr;

    bool m_scanning = false;

    PAParams m_params;
    IDriver *m_driver = nullptr;
};
