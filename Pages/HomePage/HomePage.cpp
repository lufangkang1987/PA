#include "HomePage.h"
#include "AScanWidget.h"
#include "BScanWidget.h"
#include "CScanWidget.h"
#include "IDriver.h"
#include "CTSPA22SDriver.h"
#include "AppState.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QTimer>

// ─── ElidedLabel 实现 ───
ElidedLabel::ElidedLabel(const QString& text, QWidget* parent)
	: QLabel(parent), m_fullText(text)
{
	setMinimumWidth(0);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setText(text);
}

void ElidedLabel::setFullText(const QString& text)
{
	m_fullText = text;
	updateElidedText();
}

void ElidedLabel::resizeEvent(QResizeEvent* e)
{
	QLabel::resizeEvent(e);
	updateElidedText();
}

void ElidedLabel::updateElidedText()
{
	const QFontMetrics fm(font());
	const int avail = qMax(0, width() - 4);
	const QString elided = fm.elidedText(m_fullText, Qt::ElideRight, avail);
	QLabel::setText(elided);
}

// ─── ReadingValueWidget 实现 ───
ReadingValueWidget::ReadingValueWidget(const QString& valueColor, const QString& unit,
                                         const QString& defaultValue, QWidget *parent)
    : QWidget(parent), m_text(defaultValue), m_unit(unit), m_valueColor(valueColor)
{
    setMinimumHeight(28);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ReadingValueWidget::setValue(const QString& val)
{
    m_text = val;
    update();
}

void ReadingValueWidget::setValue(double val, int decimals)
{
    m_text = QString::number(val, 'f', decimals);
    update();
}

void ReadingValueWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);

    int cx = width() / 2;

    QFont vFont = font();
    vFont.setPixelSize(22);
    vFont.setBold(true);
    p.setFont(vFont);
    QFontMetrics vfm(p.fontMetrics());
    int vw = vfm.horizontalAdvance(m_text);
    int vx = cx - vw / 2;
    int vy = (height() + vfm.ascent() - vfm.descent()) / 2;
    p.setPen(QColor(m_valueColor));
    p.drawText(vx, vy, m_text);

    if (!m_unit.isEmpty()) {
        QFont uFont = font();
        uFont.setPixelSize(12);
        p.setFont(uFont);
        QFontMetrics ufm(p.fontMetrics());
        p.setPen(QColor("#6a9ab0"));
        int uy = (height() + ufm.ascent() - ufm.descent()) / 2;
        p.drawText(vx + vw + 2, uy, m_unit);
    }
}

// ─── ReadingItem 实现 ───
ReadingItem::ReadingItem(const QString& label, const QString& unit,
                          const QString& defaultValue, const QString& valueColor,
                          QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 2);
    layout->setSpacing(2);

    auto *name = new QLabel(label);
    name->setObjectName("ReadingName");
    name->setAlignment(Qt::AlignCenter);

    m_valueWidget = new ReadingValueWidget(valueColor, unit, defaultValue);

    layout->addWidget(name);
    layout->addWidget(m_valueWidget, 1);
}

void ReadingItem::setValue(const QString& val)
{
    m_valueWidget->setValue(val);
}

void ReadingItem::setValue(double val, int decimals)
{
    m_valueWidget->setValue(val, decimals);
}

// ─── makeCard ───
static QFrame* makeCard(const QString& title, QWidget* content, const QString& meta = QString())
{
	auto* card = new QFrame;
	card->setObjectName("DataCard");
	card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	auto* layout = new QVBoxLayout(card);
	layout->setContentsMargins(12, 10, 12, 12);
	layout->setSpacing(7);

	if (!title.isEmpty() || !meta.isEmpty()) {
		auto* header = new QHBoxLayout;
		header->setSpacing(8);
		auto* titleLabel = new ElidedLabel(title);
		titleLabel->setObjectName("CardTitle");
		header->addWidget(titleLabel, 1);
		if (!meta.isEmpty()) {
			auto* metaLabel = new ElidedLabel(meta);
			metaLabel->setObjectName("CardMeta");
			metaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			header->addWidget(metaLabel, 2);
		}
		layout->addLayout(header);
	}

	layout->addWidget(content, 1);
	return card;
}

static ElidedLabel* statusItem(const QString& label, const QString& value, bool ok = false)
{
	const QString full = QString("%1： %2").arg(label, value);
	auto* item = new ElidedLabel(full);
	item->setObjectName("StatusItem");
	item->setStyleSheet(QString("color:%1;").arg(ok ? "#1eea36" : "#d7e8f2"));
	item->setMinimumWidth(0);
	item->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	return item;
}

HomePage::HomePage(QWidget* parent) : QWidget(parent)
{
	auto* root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	auto* grid = new QGridLayout;
	grid->setHorizontalSpacing(0);
	grid->setVerticalSpacing(0);

	m_aScan = new AScanWidget;
	m_bScan = new BScanWidget;
	m_cScan = new CScanWidget;

	// ── 默认闸门参数 ──
	setGateParams(0, true,  2.5f, 4.0f, 40.0f, QColor(255, 30, 30));
	setGateParams(1, true,  6.2f, 3.0f, 30.0f, QColor(255, 200, 0));
	setGateParams(2, true,  0.5f, 3.0f, 30.0f, QColor(200, 50, 255));

	grid->addWidget(makeCard("A扫波形", m_aScan, "通道：1 | 增益：18.0 dB"), 0, 0);
	grid->addWidget(makeCard("B扫图像", m_bScan, "扫描范围：200.0 mm"), 0, 1);
	grid->addWidget(makeCard("C扫图像", m_cScan), 1, 0, 1, 2);

	grid->setColumnStretch(0, 5);
	grid->setColumnStretch(1, 6);
	grid->setRowStretch(0, 13);
	grid->setRowStretch(1, 10);

	// ── 底部状态栏 ──
	auto* statusBar = new QFrame;
	statusBar->setObjectName("BottomStatus");
	statusBar->setMinimumHeight(36);
	statusBar->setMaximumHeight(42);
	auto* statusLayout = new QHBoxLayout(statusBar);
	statusLayout->setContentsMargins(12, 0, 12, 0);
	statusLayout->setSpacing(10);
	statusLayout->addWidget(statusItem("设备状态", "正常", true));
	statusLayout->addWidget(statusItem("探头连接", "正常", true));
	statusLayout->addWidget(statusItem("编码器", "正常", true));
	statusLayout->addWidget(statusItem("扫描长度", "1250.00 mm"));
	statusLayout->addWidget(statusItem("已扫长度", "320.00 mm"));
	statusLayout->addWidget(statusItem("完成进度", "25%"));
	statusLayout->addStretch();
	statusLayout->addWidget(statusItem("数据路径", "D:\\TOFD\\Data\\"));

	root->addLayout(grid, 1);
	root->addWidget(statusBar);

	m_status = new QLabel("系统状态：待机");
	m_status->hide();

	setStyleSheet(R"(
	        QWidget { background:#07121d; color:#cfe7f4; font-family:"Microsoft YaHei"; }
	        #DataCard {
	            background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #0b1b2a, stop:1 #071522);
	            border:1px solid #16425f;
	            border-radius:6px;
	        }
	        #CardTitle { color:#f1fbff; font-size:15px; font-weight:700; background:transparent; }
	        #CardMeta { color:#d8e4ec; font-size:12px; background:transparent; }
	        #BottomStatus {
	            min-height:36px;
	            max-height:42px;
	            background:#0a1c2d;
	            border:1px solid #10334e;
	            border-radius:5px;
	        }
	        #StatusItem { color:#d7e8f2; font-size:12px; background:transparent; }
	    )");

	QTimer::singleShot(0, this, [this] {
		if (m_aScan) m_aScan->update();
		if (m_bScan) m_bScan->update();
		if (m_cScan) m_cScan->update();
	});
}

void HomePage::setDriver(IDriver* driver)
{
	if (!driver) return;

	QObject* qobj = driver->asQObject();

	connect(m_aScan, &AScanWidget::gateDragged, this, &HomePage::gateDragged);
	connect(m_aScan, &AScanWidget::beamChangeRequested, this, &HomePage::beamChangeRequested);

	if (auto* ct = qobject_cast<CTSPA22SDriver*>(qobj)) {
		connect(ct, &CTSPA22SDriver::waveformReady, m_aScan, &AScanWidget::setWaveform);
		connect(ct, &CTSPA22SDriver::statusChanged, this, [this](const QString& s) {
			m_status->setText(QString::fromUtf8("系统状态：") + s);
		});
		connect(ct, &CTSPA22SDriver::multiBeamWaveformsReady, this, [this](const QVector<QVector<double>> &waves) {
			m_bScan->setMultiBeamData(waves, /*isRF*/ false);
			bool alarm = false;
			for (const auto &w : waves)
				for (const auto &v : w) { if (v > 0.8) { alarm = true; break; } }
			if (m_aScan) m_aScan->setAlarm(alarm);
		});
		connect(ct, &CTSPA22SDriver::tfmImageReady, this, [this](const QVector<int>&) {});
		connect(ct, &CTSPA22SDriver::gateReadingsReady, this, [](char gate, double amp, double path) {
			Q_UNUSED(gate); Q_UNUSED(amp); Q_UNUSED(path);
			// 闸门读数 → MeasurePage（由 MainWindow 连接）
		});

		auto *st = AppState::instance();
		m_bScan->setScanParams(st->scanType(), -30.0f, 30.0f, st->beamCount(), 100.0f);
		m_bScan->setAcousticParams(st->velocity(), st->range(), st->sampleRate());

		connect(st, &AppState::scanTypeChanged, m_bScan, [this](int type) {
			auto *s = AppState::instance();
			m_bScan->setScanParams(type, -30.0f, 30.0f, s->beamCount(), 100.0f);
		});
		connect(st, &AppState::velocityChanged, m_bScan, [this](float v) {
			auto *s = AppState::instance();
			m_bScan->setAcousticParams(v, s->range(), s->sampleRate());
		});
		connect(st, &AppState::rangeChanged, m_bScan, [this](float r) {
			auto *s = AppState::instance();
			m_bScan->setAcousticParams(s->velocity(), r, s->sampleRate());
		});

		return;
	}
}

void HomePage::setGateParams(int gate, bool enabled, float start, float width,
                             float threshold, const QColor &color)
{
    if (m_aScan)
        m_aScan->setGate(gate, enabled, start, width, threshold, color);
}

void HomePage::setActiveGate(int gate)
{
    if (m_aScan)
        m_aScan->setActiveGate(gate);
}

void HomePage::setFrozen(bool frozen)
{
    if (m_aScan)
        m_aScan->setLive(!frozen);
    if (m_bScan)
        m_bScan->setFrozen(frozen);
}

QVector<float> HomePage::getCScanData(int &w, int &h) const
{
    if (m_cScan) {
        w = m_cScan->dataWidth();
        h = m_cScan->dataHeight();
        return m_cScan->data();
    }
    w = 0; h = 0;
    return {};
}

void HomePage::setCScanReplayData(const QVector<float> &data, int w, int h, bool replayOn)
{
    if (m_cScan) {
        m_cScan->setData(data, w, h);
        m_cScan->setReplayMode(replayOn);
    }
}

void HomePage::setCScanReplayMode(bool on)
{
    if (m_cScan)
        m_cScan->setReplayMode(on);
}
