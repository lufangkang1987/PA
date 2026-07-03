#include "ParamPage.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

constexpr int kFieldWidth = 240;
constexpr int kWideFieldWidth = 420;

void polishField(QWidget *field, int width = kFieldWidth)
{
    field->setFixedHeight(32);
    field->setMinimumWidth(width);
    field->setMaximumWidth(width);
    field->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QFormLayout *makeForm(QGroupBox *group)
{
    auto *form = new QFormLayout(group);
    form->setContentsMargins(18, 20, 18, 18);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    return form;
}

}

ParamPage::ParamPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(14);

    auto *content = new QWidget;
    content->setObjectName("ParamContent");
    content->setMaximumWidth(980);
    content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(14);

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(14);

    auto *ultra = new QGroupBox("超声参数");
    auto *uf = makeForm(ultra);
    auto *gain = new QDoubleSpinBox;
    gain->setRange(0, 120);
    gain->setValue(32.0);
    gain->setSuffix(" dB");
    polishField(gain);

    auto *velocity = new QDoubleSpinBox;
    velocity->setRange(1000, 9000);
    velocity->setValue(5900);
    velocity->setSuffix(" m/s");
    polishField(velocity);

    auto *range = new QDoubleSpinBox;
    range->setRange(1, 1000);
    range->setValue(100);
    range->setSuffix(" mm");
    polishField(range);

    uf->addRow("增益", gain);
    uf->addRow("声速", velocity);
    uf->addRow("检测范围", range);

    auto *motion = new QGroupBox("运动参数");
    auto *mf = makeForm(motion);
    auto *speed = new QDoubleSpinBox;
    speed->setRange(0, 500);
    speed->setValue(50);
    speed->setSuffix(" mm/s");
    polishField(speed);

    auto *distance = new QDoubleSpinBox;
    distance->setRange(0, 100000);
    distance->setValue(1000);
    distance->setSuffix(" mm");
    polishField(distance);

    auto *mode = new QComboBox;
    mode->addItems({"手动", "自动扫描", "激光循迹"});
    polishField(mode);

    mf->addRow("速度", speed);
    mf->addRow("扫描距离", distance);
    mf->addRow("运行模式", mode);

    grid->addWidget(ultra, 0, 0);
    grid->addWidget(motion, 0, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    contentLayout->addLayout(grid);

    auto *camera = new QGroupBox("摄像头参数");
    auto *cf = makeForm(camera);
    auto *frontCamera = new QLineEdit("camera/front");
    auto *rearCamera = new QLineEdit("camera/rear");
    polishField(frontCamera, kWideFieldWidth);
    polishField(rearCamera, kWideFieldWidth);
    cf->addRow("前置摄像头地址", frontCamera);
    cf->addRow("后置摄像头地址", rearCamera);
    contentLayout->addWidget(camera);

    auto *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 2, 0, 0);
    actions->addStretch();
    auto *save = new QPushButton("保存参数");
    save->setFixedSize(150, 34);
    actions->addWidget(save);
    contentLayout->addLayout(actions);

    root->addWidget(content, 0, Qt::AlignHCenter | Qt::AlignTop);
    root->addStretch();

    setStyleSheet(R"(
        QWidget {
            background:#08131d;
            color:#cfe7f4;
            font-size:13px;
        }
        #ParamContent {
            background:transparent;
        }
        QGroupBox {
            background:#0a1b2b;
            border:1px solid #29445c;
            border-radius:8px;
            margin-top:12px;
            font-weight:700;
        }
        QGroupBox::title {
            subcontrol-origin:margin;
            left:12px;
            padding:0 6px;
            color:#dff5ff;
        }
        QLabel {
            color:#b9d3e2;
            background:transparent;
        }
        QLineEdit, QDoubleSpinBox, QSpinBox, QComboBox {
            background:#101d2a;
            color:white;
            border:1px solid #355a73;
            border-radius:5px;
            padding:4px 8px;
        }
        QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus {
            border-color:#1688d8;
        }
        QPushButton {
            background:#18536e;
            color:white;
            border:1px solid #3b7893;
            border-radius:5px;
            padding:7px 16px;
            font-weight:600;
        }
        QPushButton:hover {
            background:#126aa0;
            border-color:#2595d3;
        }
        QPushButton:pressed {
            background:#075daf;
        }
    )");
}
