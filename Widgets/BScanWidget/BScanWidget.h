#pragma once

#include "DataTypes.h"

#include <QImage>
#include <QVector>
#include <QWidget>
#include <array>
#include <cstdint>
#include <vector>

struct PAParams;

class BScanWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BScanWidget(QWidget *parent = nullptr);

    void setParamsSource(const PAParams *params);
    void setMultiBeamData(const QVector<QVector<double>> &waves, bool isRF = false);

    void setScanParams(int scanType, float angleFrom, float angleTo,
                       int beamCount, float range,
                       int probeCount = 64, int eleStart = 1,
                       int eleEnd = 64, int eleAperture = 16);

    // Kept for source compatibility. When a PAParams source is bound, it is the authority.
    void setAcousticParams(float velocity, float range, int sampleRate);

    void setRulePositions(const QVector<double> &positions);

    void setFrozen(bool frozen);
    bool isFrozen() const { return m_frozen; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct ScanConfig
    {
        int   scanType     = 0;
        float angleFrom    = -30.0f;
        float angleTo      = 30.0f;
        int   beamCount    = MaxBeams;
        float range        = 100.0f;
        int   probeCount   = 64;
        int   eleStart     = 1;
        int   eleEnd       = 64;
        int   eleAperture  = 16;
    };

    void buildColorLUT();
    void computeScanRules(int beamCount);
    void rebuildImagingMap(int beamCount);
    void softwareImaging(const std::vector<std::array<uint8_t, WaveSampleCount>> &waveforms,
                         int beamCount, uint8_t *img);
    QImage buildDisplayImage() const;

    QVector<QRgb> m_colorLUT;
    std::vector<uint8_t> m_sImage;
    QImage m_displayImage;
    ScanRule m_rules[MaxBeams];
    QVector<quint8> m_mapBeam0;
    QVector<quint8> m_mapBeam1;
    QVector<quint16> m_mapSample0;
    QVector<quint16> m_mapSample1;
    QVector<quint8> m_mapBlend;
    int m_ruleBeamCount = 0;
    float m_imgSpanStart = 0.0f;
    float m_imgSpanEnd = 0.0f;
    QVector<double> m_rulePositions;
    ScanConfig m_scan;
    const PAParams *m_params = nullptr;

    bool m_frozen = false;
    bool m_hasData = false;
};
