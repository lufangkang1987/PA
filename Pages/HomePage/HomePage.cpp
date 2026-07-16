#include "HomePage.h"
#include "AScanWidget.h"
#include "BScanWidget.h"
#include "CScanWidget.h"
#include "IDriver.h"
#include "CTSPA22SDriver.h"
#include "PAParams.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QCoreApplication>
#include <QDir>
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
	const QString full = QString::fromUtf8("%1： %2").arg(label, value);
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
	connect(m_cScan, &CScanWidget::positionSelected,
	        this, &HomePage::cScanPositionSelected);
	connect(m_cScan, &CScanWidget::analysisRectChanged,
	        this, &HomePage::cScanAnalysisRectChanged);
	connect(m_cScan, &CScanWidget::analysisMeasured, this,
	        [this](float maximum, float average, int maxLine, int maxColumn) {
		m_analysisStatus->setFullText(QString("Analysis: max %1%, avg %2%, pos (%3, %4)")
		    .arg(maximum * 100.0f, 0, 'f', 1).arg(average * 100.0f, 0, 'f', 1)
		    .arg(maxLine * m_cScan->property("scanStepMm").toFloat(), 0, 'f', 1)
		    .arg(maxColumn));
		emit cScanAnalysisMeasured(maximum, average, maxLine, maxColumn);
	});

	// ── 默认闸门参数 ──
	setGateParams(0, true,  2.5f, 4.0f, 40.0f, QColor(255, 30, 30));
	setGateParams(1, true,  6.2f, 3.0f, 30.0f, QColor(255, 200, 0));
	setGateParams(2, true,  0.5f, 3.0f, 30.0f, QColor(200, 50, 255));

	grid->addWidget(makeCard(QString::fromUtf8("A扫波形"), m_aScan, QString::fromUtf8("通道：1 | 增益：18.0 dB")), 0, 0);
	grid->addWidget(makeCard(QString::fromUtf8("B扫图像"), m_bScan, QString::fromUtf8("扫描范围：200.0 mm")), 0, 1);
	grid->addWidget(makeCard(QString::fromUtf8("C扫图像"), m_cScan), 1, 0, 1, 2);

	grid->setColumnStretch(0, 5);
	grid->setColumnStretch(1, 6);
	grid->setRowStretch(0, 13);
	grid->setRowStretch(1, 10);

	// ── 底部状态栏 ──
	auto* statusBar = new QFrame;
	statusBar->setObjectName("BottomStatus");
	statusBar->setMinimumHeight(28);
	statusBar->setMaximumHeight(36);
	auto* statusLayout = new QHBoxLayout(statusBar);
	statusLayout->setContentsMargins(12, 2, 12, 2);
	statusLayout->setSpacing(16);
	m_scanLengthStatus = statusItem(QString::fromUtf8("扫描长度"), "0.00 mm");
	m_scannedLengthStatus = statusItem(QString::fromUtf8("已扫长度"), "0.00 mm");
	m_progressStatus = statusItem(QString::fromUtf8("完成进度"), "0%");
	m_speedStatus = statusItem(QString::fromUtf8("速度"), "0.0 mm/s");
	m_analysisStatus = statusItem(QString::fromUtf8("分析"), "--");
	m_frameDiffStatus = statusItem(QString::fromUtf8("帧差"), "0");
	m_droppedFrameStatus = statusItem(QString::fromUtf8("漏帧"), "0");
	statusLayout->addWidget(m_scanLengthStatus);
	statusLayout->addWidget(m_scannedLengthStatus);
	statusLayout->addWidget(m_progressStatus);
	statusLayout->addWidget(m_speedStatus);
	statusLayout->addWidget(m_analysisStatus);
	statusLayout->addWidget(m_frameDiffStatus);
	statusLayout->addWidget(m_droppedFrameStatus);
	statusLayout->addStretch();
		{
			const QString dataDir = QCoreApplication::applicationDirPath() + "/data";
			auto* pathItem = new ElidedLabel(QString::fromUtf8("数据: %1").arg(QDir::toNativeSeparators(dataDir)));
			pathItem->setObjectName("StatusItem");
			pathItem->setMinimumWidth(0);
			pathItem->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			statusLayout->addWidget(pathItem);
		}

	root->addLayout(grid, 1);
	root->addWidget(statusBar);


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


	connect(m_aScan, &AScanWidget::gateDragged, this, &HomePage::gateDragged);
	connect(m_aScan, &AScanWidget::beamChangeRequested, this, &HomePage::beamChangeRequested);

	{
		connect(driver, &IDriver::waveformReady, m_aScan, &AScanWidget::setWaveform);
		connect(driver, &IDriver::frameStatisticsChanged,
		        this, &HomePage::updateFrameStatistics);
		connect(driver, &IDriver::multiBeamWaveformsReady, this, [this](const QVector<QVector<double>> &waves) {
			m_bScan->setMultiBeamData(waves, /*isRF*/ false);
			bool alarm = false;
			for (const auto &w : waves)
				for (const auto &v : w) { if (v > 0.8) { alarm = true; break; } }
			if (m_aScan) m_aScan->setAlarm(alarm);
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

void HomePage::setAScanCalibrationGuide(bool visible, int targetPercent)
{
    if (m_aScan)
        m_aScan->setCalibrationGuide(visible, targetPercent);
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
        m_cScan->setPageStart(0);
    }
}

void HomePage::setCScanReplayMode(bool on)
{
    if (m_cScan)
        m_cScan->setReplayMode(on);
}

void HomePage::setCScanData(const QVector<float> &data, int w, int h)
{
    if (m_cScan) {
        m_cScan->setReplayMode(false);
        m_cScan->setData(data, w, h);
        m_cScan->setPageStart(qMax(0, h - CScanLinesPerPage));
    }
}

void HomePage::updateCScanMetrics(int lines, int totalLines, double scannedMm,
                                  double speedMmPerSec, double averageMmPerSec)
{
    const double totalMm = totalLines > 0 && lines > 0
        ? scannedMm * totalLines / lines : 0.0;
    const int percent = totalLines > 0 ? qBound(0, lines * 100 / totalLines, 100) : 0;
    m_scanLengthStatus->setFullText(QString::fromUtf8("扫描长度：%1 mm").arg(totalMm, 0, 'f', 2));
    m_scannedLengthStatus->setFullText(QString::fromUtf8("已扫长度：%1 mm").arg(scannedMm, 0, 'f', 2));
    m_progressStatus->setFullText(QString::fromUtf8("完成进度：%1%").arg(percent));
    m_speedStatus->setFullText(QString::fromUtf8("速度：%1 mm/s (平均 %2)")
        .arg(speedMmPerSec, 0, 'f', 1).arg(averageMmPerSec, 0, 'f', 1));
}

void HomePage::updateFrameStatistics(int frameDiff, quint64 droppedFrames)
{
    if (m_frameDiffStatus)
        m_frameDiffStatus->setFullText(QString::fromUtf8("帧差：%1").arg(frameDiff));
    if (m_droppedFrameStatus)
        m_droppedFrameStatus->setFullText(QString::fromUtf8("漏帧：%1").arg(droppedFrames));
}

void HomePage::showReplayPacket(const DataPacket &packet, int line,
                                int beamIndex, int rectifyMode)
{
    if (packet.beamCount <= 0) return;
    const int beam = qBound(0, beamIndex, packet.beamCount - 1);
    const bool isRf = rectifyMode == 3;
    QVector<double> selected(WaveSampleCount);
    QVector<QVector<double>> all(packet.beamCount);
    for (int b = 0; b < packet.beamCount; ++b) {
        all[b].resize(WaveSampleCount);
        for (int i = 0; i < WaveSampleCount; ++i)
            all[b][i] = isRf ? (int(packet.beams[b].waveP[i]) - 128) / 128.0
                             : packet.beams[b].waveP[i] / 255.0;
    }
    selected = all[beam];
    m_aScan->setWaveform(selected, beam, packet.frameIndex, rectifyMode);
    m_bScan->setMultiBeamData(all, isRf);
    m_cScan->setSelectedLine(line);
}

void HomePage::selectCScanLine(int line)
{
    m_cScan->setSelectedLine(line);
}

void HomePage::configureCScanView(const PAParams &params)
{
    if (!m_cScan) return;
    m_cScan->setAnalysisRect(params.ana.anaLineX1, params.ana.anaLineX2,
                             params.ana.anaLineY1, params.ana.anaLineY2);
    m_cScan->setImageColumnRange(params.img.imgLineX1, params.img.imgLineX2);
    m_cScan->setPhysicalScale(params.img.degPerPoint,
                              params.img.imgSpanStart, params.img.imgSpanEnd);
    m_cScan->setProperty("scanStepMm", params.img.degPerPoint);
}

void HomePage::setCScanImageSpan(float startMm, float endMm)
{
    if (!m_cScan || !m_cScan->hasData()) return;
    m_cScan->setPhysicalScale(m_cScan->property("scanStepMm").toFloat(),
                              startMm, endMm);
}

void HomePage::setCScanPageStart(int line)
{
    if (m_cScan) m_cScan->setPageStart(line);
}

int HomePage::cScanPageStart() const
{
    return m_cScan ? m_cScan->pageStart() : 0;
}
