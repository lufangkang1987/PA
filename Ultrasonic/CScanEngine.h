#pragma once
#include "DataTypes.h"
#include "PAParams.h"
#include <QElapsedTimer>
#include <QObject>
#include <QVector>
#include <memory>

class CScanEngine : public QObject
{
    Q_OBJECT
public:
    explicit CScanEngine(QObject *parent = nullptr);

    void configure(const PAParams &params);
    void start();
    void stop();
    void clear();

    bool isScanning() const;
    int capturedLines() const;
    float imgSpanStart() const { return m_imgSpanStart; }
    float imgSpanEnd()   const { return m_imgSpanEnd; }
    QVector<float> image() const;
    QVector<DataPacket> archivedPackets() const;
    void setArchivedPackets(const QVector<DataPacket> &packets);
    void setRulePositions(const QVector<double> &positions);
    void setScanRules(const QVector<ScanRule> &rules);
    QVector<ScanRule> currentScanRules(int beamCount);

public slots:
    void processPacket(std::shared_ptr<DataPacket> packet);

signals:
    void imageUpdated(const QVector<float> &image, int width, int height);
    void progressChanged(int capturedLines, int totalLines);
    void metricsChanged(int capturedLines, int totalLines, double scannedMm,
                        double speedMmPerSec, double averageMmPerSec);
    void scanCompleted();

private:
    int encoderLine(const BeamWaveform &beam) const;
    void computeScanRules(int beamCount, ScanRule *rules);
    QVector<uint8_t> softwareImaging(const DataPacket &packet);
    QVector<float> buildCScanRow(const QVector<uint8_t> &sImage);
    void initializeTrace(const DataPacket &packet);
    void applyTrace(DataPacket &packet);
    QVector<float> buildTraceRow(const DataPacket &packet) const;
    int gateStartSample(int gate) const;
    int gateWidthSamples(int gate) const;

    PAParams m_params;
    QVector<float> m_image;
    bool m_scanning = false;
    int m_capturedLines = 0;
    int m_lastLine = -1;
    QVector<DataPacket> m_archivedPackets;
    QElapsedTimer m_scanTimer;
    qint64 m_lastMetricMs = 0;
    qint64 m_lastImageMs = 0;
    int m_lastMetricLines = 0;
    int m_traceBaseB = 0;
    int m_traceBaseC = 0;
    int m_shiftA1[MaxBeams] = {};
    int m_shiftA2[MaxBeams] = {};
    QVector<double> m_rulePositions;
    QVector<ScanRule> m_explicitRules;
    float m_imgSpanStart = 0.0f;
    float m_imgSpanEnd   = 0.0f;
};
