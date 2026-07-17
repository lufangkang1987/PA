#pragma once
#include "DataTypes.h"
#include "ReadingCalculator.h"
#include <memory>
#include <QObject>
#include <QVector>

class CalibrationController;
struct PAParams;

/// 数据包处理器 — 从 MainWindow 的 dataPacketReady lambda 抽取
///
/// 职责：接收 DataPacket → 缓存 + 转发校准 + 计算读数 + 发射信号
/// 将原来一个 lambda 中的 5 件事拆分为独立的可测试单元。
class DataPacketProcessor : public QObject
{
    Q_OBJECT
public:
    explicit DataPacketProcessor(QObject *parent = nullptr);

    void setCalibrationController(CalibrationController *cal) { m_calController = cal; }
    void setParams(const PAParams *params) { m_params = params; }
    void setScanRulePositions(const QVector<double> &positions) { m_scanRulePositions = positions; }

    std::shared_ptr<DataPacket> latestPacket() const { return m_latestPacket; }
    bool hasLatestPacket() const { return m_hasLatestPacket; }

public slots:
    void process(std::shared_ptr<DataPacket> packet);

signals:
    void gateReadingsReady(const GateReadings &readings);
    void alarmTriggered();

private:
    CalibrationController *m_calController = nullptr;
    const PAParams *m_params = nullptr;
    QVector<double> m_scanRulePositions;
    std::shared_ptr<DataPacket> m_latestPacket;
    bool m_hasLatestPacket = false;
};
