#pragma once
#include <QWidget>
#include <QImage>
class BScanWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BScanWidget(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    void rebuildCache(const QSize &size);
    QImage m_cache;
};
