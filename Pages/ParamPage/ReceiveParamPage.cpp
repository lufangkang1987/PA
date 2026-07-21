#include "ReceiveParamPage.h"
#include "ParamPageUiHelpers.h"
#include "ParameterDispatcher.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QSpinBox>

ReceiveParamPage::ReceiveParamPage(PAParams *params, ParameterDispatcher *dispatcher, QWidget *parent)
    : QWidget(parent), m_params(params), m_dispatcher(dispatcher)
{
    auto *layout = makeParamSubPageLayout(this, QString::fromUtf8("接收参数"));
    auto *form = new QGroupBox;
    auto *f = makeForm(form);

    aGainSpin = makeParamDoubleSpin(0.0, 80.0, m_params->rx.aGain, 0.1, "dB");
    f->addRow(QString::fromUtf8("模拟增益"), wrapWithStepSelector(aGainSpin, {"0.1", "1.0", "6.0"}, {0.1, 1.0, 6.0}, 1));

    dGainSpin = makeParamDoubleSpin(-12.0, 12.0, m_params->rx.dGain, 0.1, "dB");
    f->addRow(QString::fromUtf8("数字增益"), wrapWithStepSelector(dGainSpin, {"0.1", "1.0", "6.0"}, {0.1, 1.0, 6.0}, 1));

    // 界面沿用 MFC 的 1~128 编号；内部及设备数据仍使用 0~127 下标。
    beamNoSpin = makeParamIntSpin(1, 128, m_params->rx.curBeam + 1, 1);
    f->addRow(QString::fromUtf8("声束号"), wrapWithStepSelector(beamNoSpin, {"1", "10"}, {1.0, 10.0}, 0));

    rectifyCombo = makeParamCombo({QString::fromUtf8("全波"), QString::fromUtf8("正半波"), QString::fromUtf8("负半波")}, m_params->rx.rectify);
    f->addRow(QString::fromUtf8("检波方式"), rectifyCombo);

    filterCombo = makeParamCombo({
        "0.5-20.0 MHz", "0.5-15.0 MHz", "0.5-10.0 MHz", "0.5-5.0 MHz",
        "1.0-20.0 MHz", "3.0-20.0 MHz", "5.0-20.0 MHz", "7.0-20.0 MHz",
        "10.0-20.0 MHz", "1.0 MHz", "2.5 MHz", "4.0 MHz",
        "5.0 MHz", "7.5 MHz", "10.0 MHz", "15.0 MHz"
    }, m_params->rx.filter);
    f->addRow(QString::fromUtf8("滤波器"), filterCombo);

    videoCombo = makeParamCombo({QString::fromUtf8("无"), "1", "2", "3", "4", QString::fromUtf8("平滑")}, m_params->rx.video);
    f->addRow(QString::fromUtf8("视频检波"), videoCombo);

    connect(aGainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_params->rx.aGain = static_cast<float>(v);
        if (m_dispatcher) m_dispatcher->setAnalogGain(static_cast<float>(v));
        emit beamInfoChanged(m_params->rx.curBeam, v);
    });
    connect(dGainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_params->rx.dGain = static_cast<float>(v);
        if (m_dispatcher) m_dispatcher->setDigitalGain(static_cast<float>(v));
    });
    connect(beamNoSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        const int beamIndex = v - 1;
        m_params->rx.curBeam = beamIndex;
        if (m_dispatcher) m_dispatcher->setCurrentBeam(beamIndex);
        emit beamInfoChanged(beamIndex, m_params->rx.aGain);
    });
    connect(rectifyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params->rx.rectify = v;
        if (m_dispatcher) m_dispatcher->setRectify(v);
    });
    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params->rx.filter = v;
        if (m_dispatcher) m_dispatcher->setFilter(v);
    });
    connect(videoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int v) {
        m_params->rx.video = v;
        if (!m_dispatcher) return;
        if (v < 5) {
            m_dispatcher->setASmooth(false);
            m_dispatcher->setVideoDetect(true);
        } else {
            m_dispatcher->setVideoDetect(false);
            m_dispatcher->setASmooth(true);
        }
    });

    layout->addWidget(form);
    layout->addStretch();
}
