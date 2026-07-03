#pragma once
#include <QWidget>
#include <QImage>
class CScanWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CScanWidget(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    void rebuildCache(const QSize &size);
    QImage m_cache;
};
