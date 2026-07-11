#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QPainter>
#include <QVector>
class AScanWidget;
class BScanWidget;
class CScanWidget;
class IDriver;

/// @brief 自动省略文本的标签
class ElidedLabel : public QLabel
{
	Q_OBJECT
public:
	explicit ElidedLabel(const QString& text, QWidget* parent = nullptr);
	void setFullText(const QString& text);
	QString fullText() const { return m_fullText; }
protected:
	void resizeEvent(QResizeEvent* e) override;
	void updateElidedText();
private:
	QString m_fullText;
};

/// @brief 数值显示控件：数值居中绘制，单位紧跟数值右侧（小字）
class ReadingValueWidget : public QWidget
{
	Q_OBJECT
public:
	explicit ReadingValueWidget(const QString& valueColor, const QString& unit,
		const QString& defaultValue, QWidget* parent = nullptr);
	void setValue(const QString& val);
	void setValue(double val, int decimals = 1);
protected:
	void paintEvent(QPaintEvent*) override;
private:
	QString m_text;
	QString m_unit;
	QString m_valueColor;
};

/// @brief 测量读数项：标签在上、数值居中、单位紧跟数值右侧
class ReadingItem : public QWidget
{
	Q_OBJECT
public:
	explicit ReadingItem(const QString& label, const QString& unit,
		const QString& defaultValue, const QString& valueColor = "#f2fbff",
		QWidget* parent = nullptr);
	void setValue(const QString& val);
	void setValue(double val, int decimals = 1);
private:
	ReadingValueWidget* m_valueWidget;
};

class HomePage : public QWidget
{
	Q_OBJECT
public:
	explicit HomePage(QWidget* parent = nullptr);
	void setDriver(IDriver* driver);

	// 闸门可视化
	void setGateParams(int gate, bool enabled, float start, float width,
	                   float threshold, const QColor &color);
	void setActiveGate(int gate);

	// 冻结控制（转发到 AScanWidget）
	void setFrozen(bool frozen);

	// C扫数据存取（供 MainWindow 保存/回放）
	QVector<float> getCScanData(int &w, int &h) const;
	void setCScanReplayData(const QVector<float> &data, int w, int h, bool replayOn);
	void setCScanReplayMode(bool on);

signals:
	void gateDragged(int gate, float start, float threshold);
	void beamChangeRequested(int beamIndex);

private:
	AScanWidget* m_aScan;
	BScanWidget* m_bScan;
	CScanWidget* m_cScan;
	QLabel*      m_status;
};
