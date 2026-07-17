#include "DataPacketProcessor.h"
#include "CalibrationController.h"
#include "PAParams.h"

DataPacketProcessor::DataPacketProcessor(QObject *parent)
    : QObject(parent)
{
}

void DataPacketProcessor::process(std::shared_ptr<DataPacket> packet)
{
    m_latestPacket = packet;
    if (m_calController)
        m_calController->setLatestPacket(packet);
    m_hasLatestPacket = packet && packet->beamCount > 0;
    if (!m_hasLatestPacket || !m_params)
        return;

    const GateReadings r = calculateReadings(*m_params, *packet, m_scanRulePositions);
    emit gateReadingsReady(r);
    if (r.alarmTriggered)
        emit alarmTriggered();
}
