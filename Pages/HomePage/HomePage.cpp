#include "HomePage.h"
#include "AScanWidget.h"
#include "BScanWidget.h"
#include "CScanWidget.h"
#include "IDriver.h"
#include "CTSPA22SDriver.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QTimer>

static QFrame *makeCard(const QString &title, QWidget *content, const QString &meta = QString())
{
    auto *card = new QFrame;
    card->setObjectName("DataCard");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(7);

    if (!title.isEmpty() || !meta.isEmpty()) {
        auto *header = new QHBoxLayout;
        header->setSpacing(8);
        auto *titleLabel = new QLabel(title);
        titleLabel->setObjectName("CardTitle");
        titleLabel->setMinimumWidth(0);
        header->addWidget(titleLabel, 1);
        if (!meta.isEmpty()) {
            auto *metaLabel = new QLabel(meta);
            metaLabel->setObjectName("CardMeta");
            metaLabel->setMinimumWidth(0);
            metaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            header->addWidget(metaLabel, 2);
        }
        layout->addLayout(header);
    }

    layout->addWidget(content, 1);
    return card;
}

static QLabel *statusItem(const QString &label, const QString &value, bool ok = false)
{
    auto *item = new QLabel(QString("%1： <span style='color:%2'>%3</span>")
                            .arg(label, ok ? "#1eea36" : "#d7e8f2", value));
    item->setTextFormat(Qt::RichText);
    item->setObjectName("StatusItem");
    item->setMinimumWidth(0);
    item->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return item;
}

HomePage::HomePage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 10);
    root->setSpacing(10);

    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    m_aScan = new AScanWidget;
    m_bScan = new BScanWidget;
    m_cScan = new CScanWidget;

    grid->addWidget(makeCard("A扫波形实时显示", m_aScan, "通道：1    增益：40.0 dB    采样率：100 MHz"), 0, 0, 1, 2);
    grid->addWidget(makeCard("B扫图像", m_bScan, "扫描范围：200.0 mm"), 0, 2);
    grid->addWidget(makeCard("C扫图像（平面扫描）", m_cScan), 1, 0, 1, 3);
    grid->setColumnStretch(0, 10);
    grid->setColumnStretch(1, 10);
    grid->setColumnStretch(2, 14);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    leftLayout->addLayout(grid, 1);

    auto *statusBar = new QFrame;
    statusBar->setObjectName("BottomStatus");
    auto *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    statusLayout->setSpacing(14);
    statusLayout->addWidget(statusItem("设备状态", "正常", true));
    statusLayout->addWidget(statusItem("探头连接", "正常", true));
    statusLayout->addWidget(statusItem("编码器", "正常", true));
    statusLayout->addWidget(statusItem("扫描长度", "1250.00 mm"));
    statusLayout->addWidget(statusItem("已扫长度", "320.00 mm"));
    statusLayout->addWidget(statusItem("完成进度", "25%"));
    statusLayout->addStretch();
    statusLayout->addWidget(statusItem("数据保存路径", "D:\\TOFD\\Data\\"));
    leftLayout->addWidget(statusBar);

    root->addWidget(left);

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
            min-height:42px;
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

void HomePage::setDriver(IDriver *driver)
{
    if (!driver) return;

    QObject *qobj = driver->asQObject();
    // 不改变父对象归属 —— Driver 由 MainWindow 统一管理生命周期

    // ── CTSPA22SDriver 信号全连接 ──
    if (auto *ct = qobject_cast<CTSPA22SDriver*>(qobj)) {
        connect(ct, &CTSPA22SDriver::waveformReady, m_aScan, &AScanWidget::setWaveform);
        connect(ct, &CTSPA22SDriver::statusChanged, this, [this](const QString &s) {
            m_status->setText(QString::fromUtf8("\u7CFB\u7EDF\u72B6\u6001\uFF1A") + s);
        });
        connect(ct, &CTSPA22SDriver::multiBeamWaveformsReady, this, [this](const QVector<QVector<double>> &) {
            m_bScan->update();  // TODO: 后续接 BScanWidget::setMultiBeamWaveforms
        });
        connect(ct, &CTSPA22SDriver::tfmImageReady, this, [this](const QVector<int> &) {
            // TODO: 后续接 TFM 显示控件
        });
        connect(ct, &CTSPA22SDriver::gateReadingsReady, this, [](char gate, double amp, double path) {
            Q_UNUSED(gate); Q_UNUSED(amp); Q_UNUSED(path);
            // TODO: 后续接闸门读数 UI
        });
        return;
    }
}
