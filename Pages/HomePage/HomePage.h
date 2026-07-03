#pragma once
#include <QWidget>
class AScanWidget;
class BScanWidget;
class CScanWidget;
class QLabel;
class IDriver;

class HomePage : public QWidget
{
    Q_OBJECT
public:
    explicit HomePage(QWidget *parent = nullptr);
    void setDriver(IDriver *driver);

private:
    AScanWidget *m_aScan;
    BScanWidget *m_bScan;
    CScanWidget *m_cScan;
    QLabel *m_status;
};
