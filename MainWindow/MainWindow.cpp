#include "MainWindow.h"
#include "HomePage.h"
#include "ParamPage.h"
#include "MeasurePage.h"
#include "IDriver.h"
#include "CTSPA22SDriver.h"
#include "AppState.h"
#include "CScanEngine.h"
#include "LegacyMfcData.h"
#include <QWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QComboBox>
#include <QCloseEvent>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QApplication>
#include <QDir>
#include <QTextStream>
#include <QSplitter>
#include <QMessageBox>
#include <QDateTime>
#include <QTemporaryDir>
#include <QThread>
#include <cstring>
#include <memory>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

// ═══════════════════════════════════════════════════════════
// C扫数据 .dat 文件格式 (PADT)
//   Header: magic(4) version(4) width(4) height(4) jsonLen(4)
//           + json(jsonLen)  — PAParams 序列化
//   Body:   float32[width*height]  — 振幅数据 (0.0~1.0)
// ═══════════════════════════════════════════════════════════

static const quint32 kCScanMagic   = 0x54444150;  // "PADT" LE
static const quint32 kCScanVersion = 2;

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

static bool saveMfcCsv(const QString &path, const PAParams &params,
                       const QVector<DataPacket> &packets)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    const int beamCount = params.scanType == 1
        ? qBound(1, params.eleEnd - params.eleStart + 2 - params.eleAperture, MaxBeams)
        : qBound(1, params.tempBeamCount, MaxBeams);
    out << QString::fromUtf8("采样点数, 400, 采样频率, 100MHz, 声束个数, %1, 声束距离, %2mm, C扫长度, %3, C扫步进, %4mm\n")
        .arg(beamCount).arg(params.probePitch, 0, 'f', 1)
        .arg(packets.size()).arg(params.degPerPoint, 0, 'f', 1);
    for (const DataPacket &packet : packets) {
        const int available = qMin(beamCount, packet.beamCount);
        for (int beam = 0; beam < available; ++beam) {
            for (int sample = 0; sample < WaveSampleCount; ++sample) {
                if (sample) out << ", ";
                out << int(packet.beams[beam].waveP[sample]);
            }
            out << '\n';
        }
    }
    return out.status() == QTextStream::Ok;
}

static bool saveCScanFile(const QString &path, const QVector<float> &data,
                          int w, int h, const QJsonObject &params,
                          const QVector<DataPacket> &packets)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QByteArray jsonBytes = QJsonDocument(params).toJson(QJsonDocument::Compact);
    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << kCScanMagic << kCScanVersion
       << quint32(w) << quint32(h) << quint32(jsonBytes.size());
    file.write(jsonBytes);
    ds.writeRawData(reinterpret_cast<const char *>(data.constData()),
                    data.size() * int(sizeof(float)));
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
    return ds.status() == QDataStream::Ok;
}

static QJsonObject legacyParamsToJson(const LegacyParaNode &legacy)
{
    const LegacyPartNode &p = legacy.part;
    QJsonObject j;
    j["dGain"] = p.dGain; j["aGain"] = p.aGain;
    j["highVoltage"] = p.highVoltage; j["pulseWidth"] = p.pulseWidth;
    j["prf"] = p.prf; j["range"] = p.range; j["rectify"] = p.rectify;
    j["video"] = p.video; j["filter"] = p.filter; j["material"] = p.material;
    j["lVelocity"] = p.lVelocity; j["sVelocity"] = p.sVelocity;
    j["probeType"] = p.probeType; j["probeCount"] = p.probeCount;
    j["probeFreq"] = p.probeFreq; j["probePitch"] = p.probePitch;
    j["probeDelay"] = p.probeDelay; j["wedgeEnable"] = p.wedgeEnable;
    j["wedgeType"] = p.wedgeType; j["wedgeAngle"] = p.wedgeAngle;
    j["wedgeVelocity"] = p.wedgeVelocity; j["wedgeHeight"] = p.wedgeHeight;
    j["eleStart"] = p.eleStart; j["eleEnd"] = p.eleEnd;
    j["eleAperture"] = p.eleAperture; j["angleFrom"] = p.angleFrom;
    j["angleTo"] = p.angleTo; j["innerR"] = p.innerR; j["outerR"] = p.outerR;
    j["focus"] = p.focus; j["scanType"] = p.scanType;
    j["imgLineX1"] = p.lineX1; j["imgLineX2"] = p.lineX2;
    j["imgLineY1"] = p.lineY1; j["imgLineY2"] = p.lineY2;
    j["diameter"] = p.diameter; j["checkDistance"] = p.checkDistance;
    j["aDataLen"] = p.aDataLen; j["tempCorrect"] = p.tempCorrect;
    j["direction"] = legacy.direction; j["circleDeg"] = legacy.circleDeg;
    j["coderDeg"] = legacy.coderDeg; j["degPerPoint"] = legacy.degPerPoint;
    j["imgSpanStart"] = legacy.imgSpanStart; j["imgSpanEnd"] = legacy.imgSpanEnd;
    j["curBeam"] = legacy.curBeam; j["beamCount"] = legacy.beamCount;
    j["tempBeamCount"] = legacy.tempBeamCount; j["readNum"] = legacy.readNum;
    QJsonArray gateStart{p.gateStart[0], p.bStart, p.cStart};
    QJsonArray gateWidth{p.aWidth, p.bWidth, p.cWidth};
    QJsonArray gateThreshold{p.aThreshold, p.bThreshold, p.cThreshold};
    j["gateStart"] = gateStart; j["gateWidth"] = gateWidth;
    j["gateThreshold"] = gateThreshold;
    j["gateMeasure"] = QJsonArray{p.aMeasure, p.bMeasure, p.cMeasure};
    j["gateAlarm"] = QJsonArray{p.aAlarm, p.bAlarm, p.cAlarm};
    j["gateTrace"] = QJsonArray{p.aTrace, p.bTrace, p.cTrace};
    j["alarmSound"] = p.alarmSound;
    j["probeDelay"] = p.probeDelay;
    j["dimX"] = p.dimX; j["dimY"] = p.dimY;
    j["offsetX"] = p.offsetX; j["offsetY"] = p.offsetY;
    j["pixelSize"] = p.pixelSize; j["pieceThickness"] = p.pieceThickness;
    j["tfmDGain"] = p.tfmDGain; j["tfmSmooth"] = p.tfmSmooth;
    j["parRestrainH16"] = p.parRestrainH16; j["parRestrainL16"] = p.parRestrainL16;
    QJsonArray acg;
    for (float value : p.acgValue) acg.append(value);
    j["acgValue"] = acg; j["acgSwitch"] = p.acgSwitch; j["tcgSwitch"] = p.tcgSwitch;
    QJsonArray tcgX, tcgRatio, pointX, pointValue;
    for (int i = 0; i < 6; ++i) { tcgX.append(p.tcgX[i]); tcgRatio.append(p.tcgRatio[i]); }
    for (int point = 0; point < 10; ++point)
        for (int beam = 0; beam < MaxBeams; ++beam) {
            pointX.append(p.tcgPointX[point][beam]);
            pointValue.append(p.tcgPointValue[point][beam]);
        }
    j["tcgX"] = tcgX; j["tcgRatio"] = tcgRatio;
    j["tcgCoeff"] = p.tcgValue; j["tcgStart"] = p.tcgStart; j["tcgEnd"] = p.tcgEnd;
    j["tcgPointX"] = pointX; j["tcgPointValue"] = pointValue;
    QJsonArray beamArray;
    for (int i = 0; i < MaxBeams; ++i) {
        const BeamDesc &beam = legacy.beams[i];
        if (beam.x0 == 0 && beam.y0 == 0 && beam.x1 == 0 && beam.y1 == 0) continue;
        QJsonObject value;
        value["i"] = i; value["x0"] = beam.x0; value["y0"] = beam.y0;
        value["x1"] = beam.x1; value["y1"] = beam.y1;
        beamArray.append(value);
    }
    if (!beamArray.isEmpty()) j["beams"] = beamArray;
    return j;
}

static void paramsToLegacy(const PAParams &p, LegacyParaNode &legacy)
{
    LegacyPartNode &d = legacy.part;
    d.dGain = p.dGain; d.aGain = p.aGain;
    d.highVoltage = int16_t(p.highVoltage); d.pulseWidth = int16_t(p.pulseWidth);
    d.prf = int16_t(p.prf); d.range = p.range; d.rectify = uint8_t(p.rectify);
    d.video = uint8_t(p.video); d.filter = uint8_t(p.filter); d.material = uint8_t(p.material);
    d.lVelocity = p.lVelocity; d.sVelocity = p.sVelocity;
    d.gateStart[0] = p.gateStart[0]; d.aWidth = p.gateWidth[0]; d.aThreshold = p.gateThreshold[0];
    d.aAlarm = uint8_t(p.gateAlarm[0]); d.aMeasure = uint8_t(p.gateMeasure[0]);
    d.aTrace = uint8_t(p.gateTrace[0]); d.alarmSound = uint8_t(p.alarmSound);
    d.bStart = p.gateStart[1]; d.bWidth = p.gateWidth[1]; d.bThreshold = p.gateThreshold[1];
    d.bAlarm = uint8_t(p.gateAlarm[1]); d.bMeasure = uint8_t(p.gateMeasure[1]); d.bTrace = uint8_t(p.gateTrace[1]);
    d.cStart = p.gateStart[2]; d.cWidth = p.gateWidth[2]; d.cThreshold = p.gateThreshold[2];
    d.cAlarm = uint8_t(p.gateAlarm[2]); d.cMeasure = uint8_t(p.gateMeasure[2]); d.cTrace = uint8_t(p.gateTrace[2]);
    d.probeType = uint8_t(p.probeType); d.probeCount = int16_t(p.probeCount);
    d.probeFreq = p.probeFreq; d.probePitch = p.probePitch; d.probeDelay = p.probeDelay;
    d.wedgeEnable = uint8_t(p.wedgeEnable); d.wedgeType = uint8_t(p.wedgeType);
    d.wedgeAngle = p.wedgeAngle; d.wedgeVelocity = p.wedgeVelocity; d.wedgeHeight = p.wedgeHeight;
    d.eleStart = int16_t(p.eleStart); d.eleEnd = int16_t(p.eleEnd); d.eleAperture = int16_t(p.eleAperture);
    d.angleFrom = p.angleFrom; d.angleTo = p.angleTo; d.innerR = p.innerR; d.outerR = p.outerR; d.focus = p.focus;
    d.tfmDGain = p.tfmDGain; d.tfmSmooth = int16_t(p.tfmSmooth);
    d.parRestrainH16 = int16_t(p.parRestrainH16); d.parRestrainL16 = int16_t(p.parRestrainL16);
    d.dimX = p.dimX; d.dimY = p.dimY; d.offsetX = p.offsetX; d.offsetY = p.offsetY;
    d.pixelSize = p.pixelSize; d.pieceThickness = p.pieceThickness;
    d.scanType = uint8_t(p.scanType); d.lineX1 = int16_t(p.imgLineX1); d.lineX2 = int16_t(p.imgLineX2);
    d.lineY1 = int16_t(p.imgLineY1); d.lineY2 = int16_t(p.imgLineY2); d.diameter = int16_t(p.diameter);
    d.checkDistance = p.checkDistance; d.aDataLen = uint8_t(p.aDataLen); d.tempCorrect = uint8_t(p.tempCorrect);
    std::copy(std::begin(p.tcgX), std::end(p.tcgX), std::begin(d.tcgX));
    std::copy(std::begin(p.tcgRatio), std::end(p.tcgRatio), std::begin(d.tcgRatio));
    d.acgSwitch = uint8_t(p.acgSwitch); d.tcgSwitch = uint8_t(p.tcgSwitch);
    d.tcgValue = p.tcgCoeff; d.tcgStart = int16_t(p.tcgStart); d.tcgEnd = int16_t(p.tcgEnd);
    std::memcpy(d.acgValue, p.acgValue, sizeof(d.acgValue));
    std::memcpy(d.tcgPointX, p.tcgPointX, sizeof(d.tcgPointX));
    std::memcpy(d.tcgPointValue, p.tcgPointValue, sizeof(d.tcgPointValue));
    legacy.readNum = int16_t(qBound(0, p.readNum, 32767));
    legacy.direction = int16_t(p.direction); legacy.circleDeg = int16_t(p.circleDeg);
    legacy.coderDeg = p.coderDeg; legacy.degPerPoint = p.degPerPoint;
    legacy.imgSpanStart = p.imgSpanStart; legacy.imgSpanEnd = p.imgSpanEnd;
    legacy.curBeam = uint8_t(p.curBeam); legacy.beamCount = int16_t(p.beamCount);
    legacy.tempBeamCount = int16_t(p.tempBeamCount);
    std::memcpy(legacy.beams, p.beams, sizeof(legacy.beams));
}

static bool saveLegacyMfcPar(const QString &path, const PAParams &params)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    auto legacy = std::make_unique<LegacyParaNode>();
    paramsToLegacy(params, *legacy);
    return file.write(reinterpret_cast<const char *>(legacy.get()), sizeof(*legacy)) == sizeof(*legacy);
}

static bool loadLegacyMfcPar(const QString &path, QJsonObject &params)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() != sizeof(LegacyParaNode)) return false;
    auto legacy = std::make_unique<LegacyParaNode>();
    if (file.read(reinterpret_cast<char *>(legacy.get()), sizeof(*legacy)) != sizeof(*legacy)) return false;
    params = legacyParamsToJson(*legacy);
    return true;
}

static bool saveLegacyMfcDat(const QString &path, const QVector<float> &data,
                             int width, int height, const PAParams &params,
                             const QVector<DataPacket> &packets,
                             const QVector<ScanRule> &rules)
{
    if (width != CScanWidth || height <= 0 || height > MaxCScanFrames
            || data.size() < width * height || packets.size() < height)
        return false;
    const int beamCount = packets.isEmpty() ? 0
        : qBound(0, packets.first().beamCount, MaxBeams);
    if (beamCount <= 0) return false;

    auto legacy = std::make_unique<LegacyParaNode>();
    paramsToLegacy(params, *legacy);
    legacy->readNum = int16_t(height);
    legacy->beamCount = int16_t(beamCount);
    legacy->tempBeamCount = int16_t(beamCount);
    for (int line = 0; line < height; ++line)
        for (int column = 0; column < width; ++column)
            legacy->cImage[line][column] = uint8_t(qBound(0,
                qRound(data[line * width + column] * 255.0f), 255));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    if (file.write(reinterpret_cast<const char *>(legacy.get()), sizeof(*legacy)) != sizeof(*legacy))
        return false;
    ScanRule diskRules[MaxBeams] = {};
    std::copy_n(rules.cbegin(), qMin(rules.size(), MaxBeams), diskRules);
    if (file.write(reinterpret_cast<const char *>(diskRules), sizeof(diskRules)) != sizeof(diskRules))
        return false;
    for (int line = 0; line < height; ++line) {
        if (packets[line].beamCount < beamCount) return false;
        for (int beam = 0; beam < beamCount; ++beam) {
            const BeamWaveform &wave = packets[line].beams[beam];
            char raw[412] = {};
            std::memcpy(raw, wave.waveP, WaveSampleCount);
            std::memcpy(raw + 400, &wave.frame, sizeof(wave.frame));
            raw[402] = char(wave.channel);
            std::memcpy(raw + 403, &wave.path0, sizeof(wave.path0)); raw[405] = char(wave.amp0);
            std::memcpy(raw + 406, &wave.path1, sizeof(wave.path1)); raw[408] = char(wave.amp1);
            std::memcpy(raw + 409, &wave.path2, sizeof(wave.path2)); raw[411] = char(wave.amp2);
            if (file.write(raw, sizeof(raw)) != sizeof(raw)) return false;
        }
    }
    return file.error() == QFile::NoError;
}

static QVector<float> loadLegacyMfcDat(QFile &file, int &w, int &h,
                                        QJsonObject &params, QVector<DataPacket> &packets,
                                        QVector<ScanRule> *scanRules)
{
    if (file.size() < qint64(sizeof(LegacyParaNode))) return {};
    file.seek(0);
    auto legacy = std::make_unique<LegacyParaNode>();
    if (file.read(reinterpret_cast<char *>(legacy.get()), sizeof(LegacyParaNode))
            != sizeof(LegacyParaNode)) return {};
    const int lines = qBound(0, int(legacy->readNum), MaxCScanFrames);
    const int beams = qBound(0, int(legacy->tempBeamCount), MaxBeams);
    if (lines <= 0 || beams <= 0) return {};
    const qint64 required = sizeof(LegacyParaNode) + sizeof(ScanRule) * MaxBeams
        + qint64(lines) * beams * 412;
    if (file.size() < required) return {};

    w = CScanWidth; h = lines;
    QVector<float> image(w * h);
    for (int line = 0; line < h; ++line)
        for (int column = 0; column < w; ++column)
            image[line * w + column] = legacy->cImage[line][column] / 255.0f;
    params = legacyParamsToJson(*legacy);

    file.seek(sizeof(LegacyParaNode));
    ScanRule diskRules[MaxBeams] = {};
    if (file.read(reinterpret_cast<char *>(diskRules), sizeof(diskRules)) != sizeof(diskRules))
        return {};
    if (scanRules)
        *scanRules = QVector<ScanRule>(std::begin(diskRules), std::end(diskRules));
    packets.resize(lines);
    for (int line = 0; line < lines; ++line) {
        packets[line].beamCount = beams;
        for (int beam = 0; beam < beams; ++beam) {
            char raw[412];
            if (file.read(raw, sizeof(raw)) != sizeof(raw)) return {};
            BeamWaveform &wave = packets[line].beams[beam];
            std::memcpy(wave.waveP, raw, WaveSampleCount);
            std::memcpy(&wave.frame, raw + 400, sizeof(wave.frame));
            wave.channel = quint8(raw[402]);
            std::memcpy(&wave.path0, raw + 403, sizeof(wave.path0)); wave.amp0 = quint8(raw[405]);
            std::memcpy(&wave.path1, raw + 406, sizeof(wave.path1)); wave.amp1 = quint8(raw[408]);
            std::memcpy(&wave.path2, raw + 409, sizeof(wave.path2)); wave.amp2 = quint8(raw[411]);
        }
        packets[line].frameIndex = packets[line].beams[0].frame;
    }
    return image;
}

static QVector<float> loadCScanFile(const QString &path, int &w, int &h,
                                     QJsonObject &params, QVector<DataPacket> &packets,
                                     QVector<ScanRule> *scanRules = nullptr)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);
    quint32 magic = 0, version = 0, width = 0, height = 0, jsonLen = 0;
    ds >> magic >> version >> width >> height >> jsonLen;
    if (magic != kCScanMagic)
        return loadLegacyMfcDat(file, w, h, params, packets, scanRules);
    if (version < 1 || width == 0 || width > CScanWidth
            || height == 0 || height > MaxCScanFrames || jsonLen > 4 * 1024 * 1024)
        return {};
    const qint64 imageBytes = qint64(width) * height * qint64(sizeof(float));
    if (file.bytesAvailable() < qint64(jsonLen) + imageBytes) return {};

    QByteArray jsonBytes = file.read(jsonLen);
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (doc.isObject()) params = doc.object();

    w = int(width);
    h = int(height);
    QVector<float> data(w * h);
    if (ds.readRawData(reinterpret_cast<char *>(data.data()), int(imageBytes)) != imageBytes)
        return {};
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

    PAParams source;
    source.lVelocity = 6123;
    source.coderDeg = 0.123f;
    source.gateStart[1] = 7.5f;
    source.tcgPointX[2][3] = 77;
    source.tcgPointValue[2][3] = 4.25f;
    const QString parPath = directory.filePath("roundtrip.par");
    if (!saveLegacyMfcPar(parPath, source)) return fail("legacy par save failed");
    QJsonObject loadedParams;
    if (!loadLegacyMfcPar(parPath, loadedParams)) return fail("legacy par load failed");
    if (loadedParams["lVelocity"].toInt() != source.lVelocity
            || qAbs(loadedParams["coderDeg"].toDouble() - source.coderDeg) > 0.0001)
        return fail("legacy par value mismatch");

    const QString datPath = directory.filePath("legacy.dat");
    QFile legacyFile(datPath);
    if (!legacyFile.open(QIODevice::WriteOnly)) return fail("legacy dat create failed");
    auto legacy = std::make_unique<LegacyParaNode>();
    paramsToLegacy(source, *legacy);
    legacy->readNum = 2;
    legacy->tempBeamCount = 2;
    legacy->beamCount = 2;
    legacy->cImage[0][0] = 25;
    legacy->cImage[1][1] = 200;
    legacyFile.write(reinterpret_cast<const char *>(legacy.get()), sizeof(*legacy));
    ScanRule rules[MaxBeams] = {};
    legacyFile.write(reinterpret_cast<const char *>(rules), sizeof(rules));
    for (int line = 0; line < 2; ++line) {
        for (int beam = 0; beam < 2; ++beam) {
            char raw[412] = {};
            raw[0] = char(10 + line * 2 + beam);
            const quint16 frame = quint16(100 + line);
            std::memcpy(raw + 400, &frame, sizeof(frame));
            raw[402] = char(beam);
            raw[405] = char(50 + beam);
            legacyFile.write(raw, sizeof(raw));
        }
    }
    legacyFile.close();
    int width = 0, height = 0;
    QVector<DataPacket> packets;
    QVector<float> image = loadCScanFile(datPath, width, height, loadedParams, packets);
    if (width != CScanWidth || height != 2 || packets.size() != 2
            || packets[1].beams[1].waveP[0] != 13 || packets[0].beams[1].amp0 != 51
            || qAbs(image[CScanWidth + 1] - 200.0f / 255.0f) > 0.0001f)
        return fail("legacy dat decode mismatch");

    CScanEngine ruleEngine;
    PAParams ruleParams;
    ruleParams.scanType = 0;
    ruleParams.angleFrom = 0.0f;
    ruleParams.angleTo = 30.0f;
    ruleParams.range = 100.0f;
    ruleEngine.configure(ruleParams);
    ruleEngine.setRulePositions(QVector<double>{10.0, 20.0});
    const QVector<ScanRule> calculatedRules = ruleEngine.currentScanRules(2);
    if (calculatedRules.size() != 2
            || qAbs(calculatedRules[0].x - 55.0) > 0.001
            || qAbs(calculatedRules[1].x - 95.0) > 0.001
            || qAbs(calculatedRules[1].ang - 30.0) > 0.001)
        return fail("MFC scan rule calculation mismatch");

    const QString exportedLegacyPath = directory.filePath("exported-legacy.dat");
    QVector<ScanRule> exportRules(MaxBeams);
    exportRules[0] = calculatedRules[0];
    exportRules[1] = calculatedRules[1];
    if (!saveLegacyMfcDat(exportedLegacyPath, image, width, height,
                          source, packets, exportRules))
        return fail("legacy dat export failed");
    int exportedWidth = 0, exportedHeight = 0;
    QVector<DataPacket> exportedPackets;
    QJsonObject exportedParams;
    const QVector<float> exportedImage = loadCScanFile(
        exportedLegacyPath, exportedWidth, exportedHeight,
        exportedParams, exportedPackets);
    if (exportedWidth != width || exportedHeight != height
            || exportedImage != image || exportedPackets.size() != packets.size()
            || exportedPackets[1].beams[1].waveP[0] != packets[1].beams[1].waveP[0])
        return fail("legacy dat export roundtrip mismatch");

    const QString padtPath = directory.filePath("roundtrip.padt");
    QVector<float> padtImage{0.1f, 0.2f, 0.3f, 0.4f};
    if (!saveCScanFile(padtPath, padtImage, 2, 2, loadedParams, packets))
        return fail("PADT save failed");
    int padtWidth = 0, padtHeight = 0;
    QVector<DataPacket> padtPackets;
    QJsonObject padtParams;
    const QVector<float> restored = loadCScanFile(
        padtPath, padtWidth, padtHeight, padtParams, padtPackets);
    if (padtWidth != 2 || padtHeight != 2 || restored != padtImage
            || padtPackets.size() != packets.size()
            || padtPackets[1].beams[1].waveP[0] != 13)
        return fail("PADT roundtrip mismatch");
    return true;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle("相控阵检测系统");
    // 默认尺寸在 main.cpp 中根据屏幕自适应设置，此处仅设最小值
    setMinimumSize(960, 600);
    setUpdatesEnabled(false);  // 批量构建，启动时避免中间重绘

    auto *shell = new QWidget(this);
    auto *root = new QVBoxLayout(shell);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    auto *header = new QFrame(shell);
    header->setObjectName("TopHeader");
    header->setMinimumHeight(48);
    header->setMaximumHeight(60);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 14, 0);
    headerLayout->setSpacing(8);

    auto *logo = new QLabel("◆", header);
    logo->setObjectName("LogoMark");
    logo->setFixedSize(28, 28);
    auto *title = new QLabel("相控阵检测系统", header);
    title->setObjectName("AppTitle");
    title->setMinimumWidth(0);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *version = new QLabel("V1.0.0", header);
    version->setObjectName("VersionLabel");
    version->setMinimumWidth(0);
    version->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // 连接模式选择
    auto *modeLabel = new QLabel("连接模式:", header);
    modeLabel->setObjectName("HeaderInfo");
    modeLabel->setMinimumWidth(0);
    modeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_modeCombo = new QComboBox(header);
    m_modeCombo->addItem("无线 (WIFI)", static_cast<int>(ConnectionMode::Wireless));
    m_modeCombo->addItem("有线 (网线)", static_cast<int>(ConnectionMode::Wired));
    m_modeCombo->setStyleSheet(
        "QComboBox{background:#0c2135;color:#d5e9f5;border:1px solid #1d3d58;padding:2px 8px;min-width:80px;}"
        "QComboBox::drop-down{border:0;}"
        "QComboBox::down-arrow{image:none;}"
        "QComboBox QAbstractItemView{background:#0c2135;color:#d5e9f5;selection-background:#0a72d6;}"
    );

    m_deviceLabel = new QLabel("● 设备连接： 未连接", header);
    m_deviceLabel->setObjectName("DeviceOk");
    m_deviceLabel->setMinimumWidth(0);
    m_deviceLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_ipLabel = new QLabel("IP: 192.168.0.51", header);
    m_ipLabel->setObjectName("HeaderInfo");
    m_ipLabel->setMinimumWidth(0);
    m_ipLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_temperatureLabel = new QLabel("温度: --", header);
    m_pcBatteryLabel = new QLabel("PC电量: --", header);
    m_paBatteryLabel = new QLabel("PA电量: --", header);
    for (QLabel *label : {m_temperatureLabel, m_pcBatteryLabel, m_paBatteryLabel}) {
        label->setObjectName("HeaderInfo");
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    headerLayout->addWidget(logo);
    headerLayout->addWidget(title);
    headerLayout->addWidget(version);
    headerLayout->addStretch(2);
    headerLayout->addWidget(modeLabel);
    headerLayout->addSpacing(2);
    headerLayout->addWidget(m_modeCombo);
    headerLayout->addSpacing(6);
    headerLayout->addWidget(m_deviceLabel);
    headerLayout->addSpacing(6);
    headerLayout->addWidget(m_ipLabel);
    headerLayout->addSpacing(6);
    headerLayout->addWidget(m_temperatureLabel);
    headerLayout->addWidget(m_pcBatteryLabel);
    headerLayout->addWidget(m_paBatteryLabel);
    updatePcBattery();

    // ────── 操作按钮 ──────
    auto makeBtn = [](const QString &text, const QString &bg, QWidget *p) {
        auto *b = new QPushButton(text, p);
        b->setFixedHeight(30);
        b->setMinimumWidth(72);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        b->setStyleSheet(QString(
            "QPushButton{background:%1;color:white;border:1px solid %2;border-radius:4px;"
            "padding:0 12px;font-size:13px;font-weight:600;}"
            "QPushButton:hover{background:%3;}"
            "QPushButton:disabled{background:#10222f;color:#4a6070;border-color:#1e3444;}")
            .arg(bg).arg(bg).arg(bg));
        return b;
    };

    m_connectBtn = makeBtn("连接设备", "#0a6e3b", header);
    m_acquireBtn = makeBtn("开始采集", "#0652a2", header);

    headerLayout->addSpacing(8);
    headerLayout->addWidget(m_connectBtn);
    headerLayout->addWidget(m_acquireBtn);

    root->addWidget(header);

    m_homePage = new HomePage;
    m_paramPage = new ParamPage;
    m_measurePage = new MeasurePage;

    // 水平布局：左侧参数面板 | 主页 | 右侧测量面板
    // 用 QHBoxLayout 替代 QSplitter，确保各面板边框完整可见
    auto *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);
    mainLayout->addWidget(m_paramPage);
    mainLayout->addWidget(m_homePage, 1);
    mainLayout->addWidget(m_measurePage);

    root->addLayout(mainLayout, 1);
    setCentralWidget(shell);

    m_driver = new CTSPA22SDriver(this);
    m_cScanThread = new QThread(this);
    m_cScanThread->setObjectName("CScanImagingThread");
    m_cScanEngine = new CScanEngine;
    m_cScanEngine->moveToThread(m_cScanThread);
    connect(m_cScanThread, &QThread::finished, m_cScanEngine, &QObject::deleteLater);
    m_cScanThread->start();
    m_homePage->setDriver(m_driver);
    m_paramPage->setDriver(m_driver);
    const bool paramsLoaded = m_paramPage->initializeParams();
    m_homePage->configureCScanView(m_paramPage->params());
    connect(m_paramPage, &ParamPage::legacyParamsLoadRequested, this,
            [this](const QString &path) { loadParamsFile(path); });
    connect(m_paramPage, &ParamPage::legacyParamsSaveRequested, this,
            [this](const QString &path) {
        statusBar()->showMessage(saveLegacyMfcPar(path, m_paramPage->params())
            ? "MFC 参数已保存: " + path : "MFC 参数保存失败");
    });

    // ── MeasurePage 信号 → MainWindow ──
    connect(m_measurePage, &MeasurePage::exitRequested, this, &MainWindow::close);
    connect(m_measurePage, &MeasurePage::powerOffAndExitRequested, this, [this] {
        if (m_driver && m_driver->isConnected()) {
            m_driver->powerOff();
        }
        close();
    });
    connect(m_measurePage, &MeasurePage::screenshotRequested, this, [this] {
        const QString dir = QCoreApplication::applicationDirPath() + "/screenshots";
        QDir().mkpath(dir);
        const QString suggested = dir + "/PA_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
        const QString path = QFileDialog::getSaveFileName(
            this, "保存截图", suggested, "PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg)");
        if (path.isEmpty()) return;
        statusBar()->showMessage(grab().save(path) ? "截图已保存: " + path : "截图保存失败");
    });
    connect(m_measurePage, &MeasurePage::freezeChanged, this, [this](bool frozen) {
        statusBar()->showMessage(frozen ? "画面已冻结" : "画面已解冻");
        if (m_homePage)
            m_homePage->setFrozen(frozen);
    });
    connect(m_measurePage, &MeasurePage::loadParamsRequested, this, [this] {
        QString paramsPath = QCoreApplication::applicationDirPath() + "/params";
        QDir().mkpath(paramsPath);
        QString filePath = QFileDialog::getOpenFileName(
            this, "调用参数", paramsPath,
            "参数文件 (*.json *.ini *.param *.par);;所有文件 (*)");
        if (!filePath.isEmpty()) loadParamsFile(filePath);
    });

    wireDriverSignals();

    // A扫闸门拖拽 → 更新 ParamPage 控件 + 下发硬件
    connect(m_homePage, &HomePage::gateDragged,
            m_paramPage, &ParamPage::onGateDragged);

    // A扫 Ctrl+点击 → 切换声束
    connect(m_homePage, &HomePage::beamChangeRequested, this, [this](int beam) {
        m_paramPage->setBeamNo(beam);
    });

    // 参数页闸门变化 → 主页A扫闸门显示
    connect(m_paramPage, &ParamPage::gateParamsChanged, this, [this] {
        static const QColor gateColors[3] = {
            QColor(255, 30, 30),    // A 红
            QColor(255, 200, 0),    // B 黄
            QColor(200, 50, 255),   // C 紫
        };
        for (int g = 0; g < 3; ++g) {
            bool enabled; float start, width, threshold;
            m_paramPage->getGateParams(g, enabled, start, width, threshold);
            m_homePage->setGateParams(g, enabled, start, width, threshold, gateColors[g]);
        }
        // 同步当前选中闸门（拖拽时用）
        m_homePage->setActiveGate(m_paramPage->activeGate());
    });

    // C扫扫描按钮信号（来自 ParamPage 成像子页）
    connect(m_paramPage, &ParamPage::scanStarted, this, [this] {
        if (m_calibrating || m_encoderCalibrating || AppState::instance()->replayState()) {
            statusBar()->showMessage("请先退出校准或回放状态");
            m_paramPage->finishScan();
            return;
        }
        m_homePage->configureCScanView(m_paramPage->params());
        m_cScanEngine->configure(m_paramPage->params());
        m_cScanEngine->start();
        if (m_driver && m_driver->isConnected()) {
            m_driver->resetEncoder(0);
            m_driver->startAcquisition();
        }
    });
    connect(m_paramPage, &ParamPage::scanStopped, this, [this] {
        m_cScanEngine->stop();
        if (m_driver)
            m_driver->stopAcquisition();
    });

    // 声束号 / 增益变化 → 右侧测量面板读数
    connect(m_paramPage, &ParamPage::beamInfoChanged,
            m_measurePage, &MeasurePage::updateBeamInfo);

    // C扫保存数据（ParamPage 已选好路径，MainWindow 执行实际写入）
    connect(m_paramPage, &ParamPage::saveDataRequested, this, [this](const QString &path) {
        if (!m_homePage || !m_paramPage) return;
        int w = 0, h = 0;
        QVector<float> data = m_homePage->getCScanData(w, h);
        if (data.isEmpty() || w <= 0 || h <= 0) {
            statusBar()->showMessage("无C扫数据可保存");
            return;
        }
        if (path.endsWith(".csv", Qt::CaseInsensitive)) {
            if (saveMfcCsv(path, m_paramPage->params(), m_cScanEngine->archivedPackets()))
                statusBar()->showMessage(QString("MFC CSV data saved: %1").arg(path));
            else
                statusBar()->showMessage("Failed to save MFC CSV data");
            return;
        }
        QJsonObject paramsJson = m_paramPage->serializeParams();
        if (saveCScanFile(path, data, w, h, paramsJson,
                          m_cScanEngine->archivedPackets())) {
            statusBar()->showMessage(QString("C扫数据已保存: %1 (%2×%3)")
                .arg(path).arg(w).arg(h));
        } else {
            statusBar()->showMessage("保存C扫数据失败");
        }
    });

    // C扫回放数据
    connect(m_paramPage, &ParamPage::replayDataRequested, this, [this](const QString &path) {
        if (!m_homePage || !m_paramPage) return;
        if (m_calibrating || m_encoderCalibrating || m_cScanEngine->isScanning()) {
            statusBar()->showMessage("扫查或校准期间不能进入回放");
            return;
        }
        int w = 0, h = 0;
        QJsonObject paramsJson;
        QVector<DataPacket> packets;
        QVector<ScanRule> loadedRules;
        QVector<float> data = loadCScanFile(path, w, h, paramsJson, packets, &loadedRules);
        if (data.isEmpty() || w <= 0 || h <= 0) {
            statusBar()->showMessage("加载C扫数据失败");
            return;
        }
        m_homePage->setCScanReplayData(data, w, h, true);
        m_cScanEngine->setArchivedPackets(packets);
        if (!loadedRules.isEmpty()) m_cScanEngine->setScanRules(loadedRules);
        AppState::instance()->setReplayState(true);
        AppState::instance()->setReplayCurPos(0);
        // 恢复参数（静默加载，不触发硬件下发）
        m_paramPage->deserializeParams(paramsJson);
        m_paramPage->syncUiFromParams();
        m_homePage->configureCScanView(m_paramPage->params());
        statusBar()->showMessage(QString("C扫回放: %1 (%2×%3)")
            .arg(path).arg(w).arg(h));
    });
    connect(m_paramPage, &ParamPage::saveLegacyDataRequested, this, [this](const QString &path) {
        int width = 0, height = 0;
        const QVector<float> data = m_homePage->getCScanData(width, height);
        const QVector<DataPacket> packets = m_cScanEngine->archivedPackets();
        const int beamCount = packets.isEmpty() ? m_paramPage->params().beamCount
                                                : packets.first().beamCount;
        const QVector<ScanRule> rules = m_cScanEngine->currentScanRules(beamCount);
        if (saveLegacyMfcDat(path, data, width, height,
                             m_paramPage->params(), packets, rules))
            statusBar()->showMessage("MFC兼容数据已保存: " + path);
        else
            statusBar()->showMessage("MFC兼容数据保存失败");
    });
    connect(m_homePage, &HomePage::cScanPositionSelected, this,
            [this](int line, int) {
        const auto &packets = m_cScanEngine->archivedPackets();
        if (line < 0 || line >= packets.size()) return;
        AppState::instance()->setReplayCurPos(line);
        m_homePage->showReplayPacket(packets[line], line,
            m_paramPage->params().curBeam, m_paramPage->params().rectify);
    });
    connect(m_homePage, &HomePage::cScanAnalysisRectChanged,
            m_paramPage, &ParamPage::setAnalysisRect);
    connect(m_paramPage, &ParamPage::cScanViewParamsChanged, this, [this] {
        m_homePage->configureCScanView(m_paramPage->params());
    });
    connect(m_paramPage, &ParamPage::calibrationRequested, this, [this](int item) {
        auto *ct = qobject_cast<CTSPA22SDriver*>(m_driver->asQObject());
        if (!ct || !m_driver->isConnected()) {
            statusBar()->showMessage("校准需要先连接设备");
            return;
        }
        if (m_cScanEngine->isScanning() || AppState::instance()->replayState()) {
            statusBar()->showMessage("请先退出扫查或回放状态");
            return;
        }
        if (!m_calibrating) {
            m_calibrating = true;
            m_calibrationItem = item;
            const PAParams &p = m_paramPage->params();
            if (item == 2 || item == 3) {
                const bool useFifty = QMessageBox::question(
                    this, "校准参考线", "默认参考线为80%，是否改为50%？",
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
                m_calibrationTargetPercent = useFifty ? 50 : 80;
                m_homePage->setAScanCalibrationGuide(true, m_calibrationTargetPercent);
            } else {
                m_homePage->setAScanCalibrationGuide(false);
            }
            const int centerBeam = p.scanType == 0 ? 63
                : (p.scanType == 1 ? (p.probeCount - p.eleAperture + 1) / 2
                                   : p.probeCount / 2);
            if (item < 3) m_paramPage->setBeamNo(qBound(0, centerBeam, MaxBeams - 1));
            if (item == 2) ct->setACG(false, p);
            if (item == 3) ct->setTCG(false, p);
            statusBar()->showMessage("校准已开始，取得稳定回波后再次点击完成");
            return;
        }
        if (!m_hasLatestPacket || m_latestPacket.beamCount <= 0) {
            statusBar()->showMessage("尚未收到有效采集数据");
            return;
        }

        const PAParams &p = m_paramPage->params();
        const int beam = qBound(0, p.curBeam, m_latestPacket.beamCount - 1);
        const BeamWaveform &wave = m_latestPacket.beams[beam];
        auto pathMm = [&p](quint16 path) {
            return p.gateTrace[2] ? path * p.range / WaveSampleCount
                                  : path * S22_SP * p.lVelocity / 2000000.0;
        };
        if (m_calibrationItem == 0) {
            const double difference = pathMm(wave.path1) - pathMm(wave.path0);
            if (difference > 1.0)
                m_paramPage->setCalibratedVelocity(qRound(p.realDistance * p.lVelocity / difference));
            ct->setVelocity(m_paramPage->params().lVelocity);
            ct->setRange(m_paramPage->params().range);
        } else if (m_calibrationItem == 1) {
            const float delay = float((pathMm(wave.path0) - p.realDistance)
                                * 2000.0 / p.lVelocity + p.probeDelay);
            m_paramPage->setCalibratedProbeDelay(delay);
            if (p.scanType < 3) ct->setBeamDelay(); else ct->setCommonRDelay();
        } else if (m_calibrationItem == 2) {
            QVector<float> values(m_latestPacket.beamCount, 1.0f);
            for (int i = 0; i < m_latestPacket.beamCount; ++i) {
                const int amplitude = m_latestPacket.beams[i].amp0;
                values[i] = amplitude > 0
                    ? qBound(0.0f, m_calibrationTargetPercent * 2.5f / amplitude, 256.0f)
                    : 256.0f;
            }
            m_paramPage->setCalibratedACG(values);
            ct->setACG(true, m_paramPage->params());
        } else if (m_calibrationItem == 3) {
            ct->setTCG(true, p);
        }
        m_calibrating = false;
        m_calibrationItem = -1;
        m_homePage->setAScanCalibrationGuide(false);
        statusBar()->showMessage("校准完成并已应用");
    });
    connect(m_paramPage, &ParamPage::encoderCalibrationRequested, this, [this] {
        if (!m_driver || !m_driver->isConnected()) {
            statusBar()->showMessage("编码器校准需要先连接设备");
            return;
        }
        if (m_cScanEngine->isScanning() || AppState::instance()->replayState() || m_calibrating) {
            statusBar()->showMessage("请先退出扫查、回放或其他校准状态");
            return;
        }
        const int position = int(AppState::instance()->encoderCount());
        if (!m_encoderCalibrating) {
            m_driver->resetEncoder(0);
            m_encoderCalibrationStart = 0;
            m_encoderCalibrating = true;
            statusBar()->showMessage("编码器校准已开始，请移动指定距离后再次点击");
        } else {
            const int pulses = qAbs(position - m_encoderCalibrationStart);
            if (pulses > 0)
                m_paramPage->setCalibratedCoderDeg(m_paramPage->params().checkDistance / pulses);
            m_encoderCalibrating = false;
            statusBar()->showMessage(QString("编码器精度：%1 mm/p")
                .arg(m_paramPage->params().coderDeg, 0, 'f', 4));
        }
    });
    connect(m_paramPage, &ParamPage::cScanPageRequested, this, [this] {
        const int count = m_cScanEngine->archivedPackets().size();
        if (count <= 0) return;
        const int maximumStart = qMax(0, count - 925);
        int pageStart = m_homePage->cScanPageStart() + 925;
        if (pageStart > maximumStart) pageStart = 0;
        m_homePage->setCScanPageStart(pageStart);
        const int line = qBound(0, pageStart + m_paramPage->params().anaLineX1, count - 1);
        AppState::instance()->setReplayCurPos(line);
        m_homePage->showReplayPacket(m_cScanEngine->archivedPackets()[line], line,
            m_paramPage->params().curBeam, m_paramPage->params().rectify);
    });
    connect(m_paramPage, &ParamPage::exitReplayRequested, this, [this] {
        AppState::instance()->setReplayState(false);
        AppState::instance()->setReplayCurPos(0);
        m_homePage->setCScanReplayMode(false);
        m_homePage->setCScanPageStart(0);
        m_homePage->selectCScanLine(-1);
        statusBar()->showMessage("已退出 C 扫回放");
    });

    // ────── 连接按钮 ──────
    connect(m_connectBtn, &QPushButton::clicked, this, [this] {
        if (!m_driver) return;

        if (m_driver->isConnected()) {
            // 断开
            m_driver->stopAcquisition();
            m_driver->disconnectDevice();
        } else {
            // 连接
            auto mode = static_cast<ConnectionMode>(
                m_modeCombo->currentData().toInt());
            m_driver->connectDevice(mode);
        }
    });

    // ────── 采集按钮 ──────
    connect(m_acquireBtn, &QPushButton::clicked, this, [this] {
        if (!m_driver || !m_driver->isConnected()) return;

        bool acquiring = m_acquireBtn->property("acquiring").toBool();
        if (acquiring) {
            m_driver->stopAcquisition();
            m_acquireBtn->setText("开始采集");
            m_acquireBtn->setStyleSheet(m_acquireBtn->styleSheet().replace("#0652a2", "#0652a2"));
        } else {
            m_driver->startAcquisition();
            m_acquireBtn->setText("停止采集");
            m_acquireBtn->setStyleSheet(
                m_acquireBtn->styleSheet().replace("#0652a2", "#c2590a"));
        }
        m_acquireBtn->setProperty("acquiring", !acquiring);
    });

    // 初始状态
    m_acquireBtn->setEnabled(false);
    m_acquireBtn->setProperty("acquiring", false);

    // 切换连接模式 → 自动断开+重连
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int /*idx*/) {
        auto mode = static_cast<ConnectionMode>(
            m_modeCombo->currentData().toInt());
        AppState::instance()->setConnectionMode(static_cast<int>(mode));

        // 更新 IP 显示
        const char *ip = (mode == ConnectionMode::Wireless)
                         ? CTSPA22SDriver::DefaultWifiIP
                         : CTSPA22SDriver::DefaultWiredIP;
        m_ipLabel->setText(QString("IP: %1").arg(ip));

        // 如果当前连接中，断开后用新模式重连
        if (m_driver && m_driver->isConnected()) {
            m_driver->stopAcquisition();
            m_driver->disconnectDevice();
            m_driver->connectDevice(mode);
        }
    });

    statusBar()->showMessage("系统就绪");
    if (!paramsLoaded)
        statusBar()->showMessage("默认参数文件加载失败，已使用程序内置参数");

    setStyleSheet(R"(
        QMainWindow { background:#06101a; }
        #TopHeader {
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #020812, stop:.45 #06131f, stop:1 #020812);
            border-bottom:1px solid #112b40;
        }
        #LogoMark { color:#168cff; font-size:24px; font-weight:900; }
        #AppTitle { color:#f2fbff; font-size:20px; font-weight:700; }
        #VersionLabel { color:#b6c9d6; font-size:13px; padding-left:8px; }
        #DeviceOk { color:#27ff49; font-size:13px; }
        #HeaderInfo { color:#d2e0eb; font-size:13px; }
        QStatusBar { background:#06101a; color:#9dcfe8; border-top:1px solid #102a3d; }
        #MainSplitter::handle {
            background:#08131d;
            border:0;
        }
    )");

    setUpdatesEnabled(true);
}

void MainWindow::wireDriverSignals()
{
    if (!m_driver) return;

    QObject *qobj = m_driver->asQObject();

    // ── CTSPA22SDriver 信号全连接 ──
    if (auto *ct = qobject_cast<CTSPA22SDriver*>(qobj)) {
        connect(ct, &CTSPA22SDriver::connectionChanged, this, [this](bool ok) {
            AppState::instance()->setConnected(ok);
            m_deviceLabel->setText(ok ? QString::fromUtf8("\u25CF 设备连接： 已连接")
                                      : QString::fromUtf8("\u25CF 设备连接： 未连接"));

            if (ok) {
                m_connectBtn->setText("断开设备");
                m_connectBtn->setStyleSheet(m_connectBtn->styleSheet().replace("#0a6e3b", "#8b2020"));
                m_acquireBtn->setEnabled(true);
                m_acquireBtn->setText("开始采集");
                m_acquireBtn->setStyleSheet(m_acquireBtn->styleSheet().replace("#c2590a", "#0652a2"));
                m_acquireBtn->setProperty("acquiring", false);
                m_paramPage->applyCurrentParams();
            } else {
                m_connectBtn->setText("连接设备");
                m_connectBtn->setStyleSheet(m_connectBtn->styleSheet().replace("#8b2020", "#0a6e3b"));
                m_acquireBtn->setEnabled(false);
                m_acquireBtn->setText("开始采集");
                m_acquireBtn->setStyleSheet(m_acquireBtn->styleSheet().replace("#c2590a", "#0652a2"));
                m_acquireBtn->setProperty("acquiring", false);
            }
        });
        connect(ct, &CTSPA22SDriver::statusChanged, this, [this](const QString &s) {
            statusBar()->showMessage(s);
        });
        connect(ct, &CTSPA22SDriver::errorOccurred, this, [this](const QString &e) {
            AppState::instance()->setConnected(false);
            statusBar()->showMessage(QString::fromUtf8("\u9519\u8BEF\uFF1A") + e);
        });
        connect(ct, &CTSPA22SDriver::temperatureReceived, this, [this](double t) {
            AppState::instance()->setTemperature(static_cast<float>(t));
            m_temperatureLabel->setText(QString("温度: %1 °C").arg(t, 0, 'f', 1));
            updatePcBattery();
        });
        connect(ct, &CTSPA22SDriver::voltageReceived, this, [this](double v) {
            AppState::instance()->setInputVoltage(static_cast<float>(v));
            const int percent = qBound(0, qRound((v - 9.2) / 2.3 * 100.0), 100);
            m_paBatteryLabel->setText(QString("PA电量: %1%").arg(percent));
            updatePcBattery();
        });
        connect(ct, &CTSPA22SDriver::encoderPositionChanged, this, [](int pos) {
            AppState::instance()->setEncoderCount(static_cast<uint32_t>(pos));
        });
        connect(ct, &CTSPA22SDriver::dataPacketReady,
                m_cScanEngine, &CScanEngine::processPacket);
        connect(ct, &CTSPA22SDriver::scanRulePositionsReady,
                m_cScanEngine, &CScanEngine::setRulePositions);
        connect(ct, &CTSPA22SDriver::scanRulePositionsReady, this,
                [this](const QVector<double> &positions) {
            m_scanRulePositions = positions.mid(0, MaxBeams);
        });
        connect(ct, &CTSPA22SDriver::dataPacketReady, this, [this](const DataPacket &packet) {
            m_latestPacket = packet;
            m_hasLatestPacket = packet.beamCount > 0;
            if (!m_hasLatestPacket || !m_measurePage || !m_paramPage) return;

            const PAParams &params = m_paramPage->params();
            const int beam = qBound(0, params.curBeam, packet.beamCount - 1);
            const BeamWaveform &wave = packet.beams[beam];
            double angle = params.angle;
            if (params.scanType == 0) {
                const double t = packet.beamCount > 1
                    ? static_cast<double>(beam) / (packet.beamCount - 1) : 0.0;
                angle = params.angleFrom + (params.angleTo - params.angleFrom) * t;
            }

            double horizontalOffset = 0.0;
            if (params.scanType == 0 && params.wedgeEnable != 0
                    && m_scanRulePositions.size() >= packet.beamCount) {
                const int centerBeam = qBound(0, 63, packet.beamCount - 1);
                horizontalOffset = m_scanRulePositions[beam]
                    - m_scanRulePositions[centerBeam];
            }
            auto soundPathMm = [&params](quint16 path) {
                return params.gateTrace[2]
                    ? path * params.range / WaveSampleCount
                    : path * S22_SP * params.lVelocity / 2000000.0;
            };
            m_measurePage->updateGateReadings(
                'A', qMin(100.0, wave.amp0 / 2.5), soundPathMm(wave.path0),
                angle, horizontalOffset);
            m_measurePage->updateGateReadings(
                'B', qMin(100.0, wave.amp1 / 2.5), soundPathMm(wave.path1),
                angle, horizontalOffset);

            // ── 蜂鸣报警 ──
            if (params.alarmSound != 0) {
                bool triggered = false;
                const int sound = params.alarmSound;  // 1=A, 2=B, 3=AB
                const int threshA = qRound(params.gateThreshold[0] * 2.5);
                const int threshB = qRound(params.gateThreshold[1] * 2.5);
                for (int b = 0; b < packet.beamCount && !triggered; ++b) {
                    if ((sound == 1 || sound == 3) && packet.beams[b].amp0 > threshA)
                        triggered = true;
                    if ((sound == 2 || sound == 3) && packet.beams[b].amp1 > threshB)
                        triggered = true;
                }
                if (triggered) {
#ifdef Q_OS_WIN
                    Beep(2000, 500);
#else
                    QApplication::beep();
#endif
                }
            }
        });
        connect(m_cScanEngine, &CScanEngine::imageUpdated, this,
                [this](const QVector<float> &img, int w, int h) {
            m_homePage->setCScanData(img, w, h);
            m_homePage->setCScanImageSpan(m_cScanEngine->imgSpanStart(),
                                          m_cScanEngine->imgSpanEnd());
        });
        connect(m_cScanEngine, &CScanEngine::metricsChanged,
                m_homePage, &HomePage::updateCScanMetrics);
        connect(m_cScanEngine, &CScanEngine::scanCompleted, this, [this] {
            if (m_driver) m_driver->stopAcquisition();
            AppState::instance()->setStartState(false);
            AppState::instance()->setReplayState(true);
            m_homePage->setCScanReplayMode(true);
            m_paramPage->finishScan();
            statusBar()->showMessage("C scan completed");
        });
        // 闸门读数 → 右侧测量面板
        return;
    }
}

void MainWindow::updatePcBattery()
{
#ifdef Q_OS_WIN
    SYSTEM_POWER_STATUS status = {};
    if (GetSystemPowerStatus(&status)
            && status.BatteryLifePercent != 255
            && !(status.BatteryFlag & 128)) {
        m_pcBatteryLabel->setText(
            QString("PC电量: %1%").arg(qBound(0, int(status.BatteryLifePercent), 100)));
        return;
    }
#endif
    m_pcBatteryLabel->setText("PC电量: --");
}

MainWindow::~MainWindow()
{
    // 在 QWidget 析构链启动之前，主动断开仪器连接
    // 否则 QTcpSocket 会在 HomePage→QTabWidget→MainWindow 析构链中被意外回收
    if (m_driver) {
        m_driver->stopAcquisition();
        m_driver->disconnectDevice();
        // 解除 HomePage 对 Driver 信号的引用，防止析构期间信号触发已销毁的 Widget
        disconnect(m_driver->asQObject(), nullptr, m_homePage, nullptr);
        delete m_driver->asQObject();
        m_driver = nullptr;
    }
    if (m_cScanThread) {
        m_cScanThread->quit();
        m_cScanThread->wait();
        m_cScanEngine = nullptr;
    }
}

bool MainWindow::loadParamsFile(const QString &path)
{
    QJsonObject params;
    if (path.endsWith(".par", Qt::CaseInsensitive)) {
        if (!loadLegacyMfcPar(path, params)) {
            statusBar()->showMessage("MFC 参数文件格式无效");
            return false;
        }
    } else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) {
            statusBar()->showMessage("参数文件格式无效");
            return false;
        }
        params = document.object();
    }
    m_paramPage->deserializeParams(params);
    m_paramPage->syncUiFromParams();
    m_homePage->configureCScanView(m_paramPage->params());
    if (m_driver && m_driver->isConnected()) m_paramPage->applyCurrentParams();
    statusBar()->showMessage("已加载参数: " + path);
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 正常关闭路径: 先断仪器，再走默认关闭流程
    if (m_driver && m_driver->isConnected()) {
        m_driver->stopAcquisition();
        m_driver->disconnectDevice();
    }
    event->accept();
}
