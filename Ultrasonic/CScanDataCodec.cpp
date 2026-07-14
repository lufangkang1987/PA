#include "CScanDataCodec.h"
#include <QDataStream>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cstring>

static const quint32 kCScanMagic = 0x54444150; // "PADT" LE
static const quint32 kCScanVersion = 3;

#pragma pack(push, 1)
struct DiskBeamWaveform {
    quint8 waveP[WaveSampleCount];
    quint16 frame;
    quint8 channel;
    quint16 path0; quint8 amp0;
    quint16 path1; quint8 amp1;
    quint16 path2; quint8 amp2;
    quint32 encFwd;
    quint32 encRvs;
};
#pragma pack(pop)

static DiskBeamWaveform toDiskBeam(const BeamWaveform &source)
{
    DiskBeamWaveform result{};
    std::memcpy(result.waveP, source.waveP, WaveSampleCount);
    result.frame = source.frame;
    result.channel = source.channel;
    result.path0 = source.path0; result.amp0 = source.amp0;
    result.path1 = source.path1; result.amp1 = source.amp1;
    result.path2 = source.path2; result.amp2 = source.amp2;
    result.encFwd = source.encFwd;
    result.encRvs = source.encRvs;
    return result;
}

static BeamWaveform fromDiskBeam(const DiskBeamWaveform &source)
{
    BeamWaveform result{};
    std::memcpy(result.waveP, source.waveP, WaveSampleCount);
    result.frame = source.frame;
    result.channel = source.channel;
    result.path0 = source.path0; result.amp0 = source.amp0;
    result.path1 = source.path1; result.amp1 = source.amp1;
    result.path2 = source.path2; result.amp2 = source.amp2;
    result.encFwd = source.encFwd;
    result.encRvs = source.encRvs;
    return result;
}

bool saveCScanFile(const QString &path, const QVector<float> &data,
                   int w, int h, const QJsonObject &params,
                   const QVector<DataPacket> &packets,
                   const QVector<ScanRule> &scanRules)
{
    if (w <= 0 || h <= 0 || data.size() < w * h) return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    const QByteArray jsonBytes = QJsonDocument(params).toJson(QJsonDocument::Compact);
    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << kCScanMagic << kCScanVersion
       << quint32(w) << quint32(h) << quint32(jsonBytes.size());
    if (file.write(jsonBytes) != jsonBytes.size()) return false;
    if (ds.writeRawData(reinterpret_cast<const char *>(data.constData()),
                        w * h * int(sizeof(float))) != w * h * int(sizeof(float)))
        return false;

    ds << quint32(packets.size());
    for (const DataPacket &packet : packets) {
        const int beamCount = qBound(0, packet.beamCount, MaxBeams);
        ds << quint16(beamCount) << packet.frameIndex;
        for (int beam = 0; beam < beamCount; ++beam) {
            const DiskBeamWaveform disk = toDiskBeam(packet.beams[beam]);
            if (ds.writeRawData(reinterpret_cast<const char *>(&disk), sizeof(disk)) != sizeof(disk))
                return false;
        }
    }

    const int ruleCount = qMin(scanRules.size(), MaxBeams);
    ds << quint32(ruleCount);
    for (int i = 0; i < ruleCount; ++i) {
        const ScanRule &rule = scanRules[i];
        ds << rule.x << rule.ang;
    }

    return ds.status() == QDataStream::Ok && file.error() == QFile::NoError;
}

QVector<float> loadCScanFile(const QString &path, int &w, int &h,
                             QJsonObject &params, QVector<DataPacket> &packets,
                             QVector<ScanRule> *scanRules)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);
    quint32 magic = 0, version = 0, width = 0, height = 0, jsonLen = 0;
    ds >> magic >> version >> width >> height >> jsonLen;
    if (magic != kCScanMagic || version < 1 || version > kCScanVersion
            || width == 0 || width > CScanWidth
            || height == 0 || height > MaxCScanFrames
            || jsonLen > 4 * 1024 * 1024)
        return {};

    const qint64 imageBytes = qint64(width) * height * qint64(sizeof(float));
    if (file.bytesAvailable() < qint64(jsonLen) + imageBytes) return {};

    const QByteArray jsonBytes = file.read(jsonLen);
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (doc.isObject()) params = doc.object();

    w = int(width);
    h = int(height);
    QVector<float> data(w * h);
    if (ds.readRawData(reinterpret_cast<char *>(data.data()), int(imageBytes)) != imageBytes)
        return {};

    packets.clear();
    if (version >= 2 && !ds.atEnd()) {
        quint32 packetCount = 0;
        ds >> packetCount;
        if (packetCount > MaxCScanFrames) return {};
        packets.resize(int(packetCount));
        for (quint32 line = 0; line < packetCount; ++line) {
            quint16 beamCount = 0;
            ds >> beamCount >> packets[int(line)].frameIndex;
            if (beamCount > MaxBeams) return {};
            packets[int(line)].beamCount = beamCount;
            for (int beam = 0; beam < beamCount; ++beam) {
                DiskBeamWaveform disk{};
                if (ds.readRawData(reinterpret_cast<char *>(&disk), sizeof(disk)) != sizeof(disk))
                    return {};
                packets[int(line)].beams[beam] = fromDiskBeam(disk);
            }
        }
    }

    if (scanRules) scanRules->clear();
    if (version >= 3 && !ds.atEnd()) {
        quint32 ruleCount = 0;
        ds >> ruleCount;
        if (ruleCount > MaxBeams) return {};
        if (scanRules) scanRules->resize(int(ruleCount));
        for (quint32 i = 0; i < ruleCount; ++i) {
            ScanRule rule{};
            ds >> rule.x >> rule.ang;
            if (scanRules) (*scanRules)[int(i)] = rule;
        }
    }

    if (ds.status() != QDataStream::Ok) return {};
    return data;
}

bool runCScanCodecSelfTest(QString *errorMessage)
{
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage) *errorMessage = message;
        return false;
    };

    QTemporaryDir directory;
    if (!directory.isValid()) return fail("temporary directory creation failed");

    QJsonObject params;
    params["range"] = 100.0;
    params["scanType"] = 0;

    QVector<float> image{0.1f, 0.2f, 0.3f, 0.4f};
    QVector<DataPacket> packets(1);
    packets[0].beamCount = 1;
    packets[0].frameIndex = 7;
    packets[0].beams[0].waveP[0] = 42;
    packets[0].beams[0].frame = 7;
    packets[0].beams[0].channel = 3;

    QVector<ScanRule> rules(1);
    rules[0].x = 12.5;
    rules[0].ang = 45.0;

    const QString path = directory.filePath("roundtrip.padt");
    if (!saveCScanFile(path, image, 2, 2, params, packets, rules))
        return fail("PADT save failed");

    int width = 0, height = 0;
    QJsonObject restoredParams;
    QVector<DataPacket> restoredPackets;
    QVector<ScanRule> restoredRules;
    const QVector<float> restoredImage = loadCScanFile(
        path, width, height, restoredParams, restoredPackets, &restoredRules);

    if (width != 2 || height != 2 || restoredImage != image)
        return fail("PADT image mismatch");
    if (restoredParams["scanType"].toInt() != 0)
        return fail("PADT params mismatch");
    if (restoredPackets.size() != 1 || restoredPackets[0].beamCount != 1
            || restoredPackets[0].beams[0].waveP[0] != 42
            || restoredPackets[0].beams[0].channel != 3)
        return fail("PADT packet mismatch");
    if (restoredRules.size() != 1 || qAbs(restoredRules[0].x - 12.5) > 0.001
            || qAbs(restoredRules[0].ang - 45.0) > 0.001)
        return fail("PADT scan rule mismatch");

    return true;
}
