#include "ParamPage.h"
#include "ParamPageUiHelpers.h"
#include "ParameterDispatcher.h"
#include "TcgParamPage.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <QCoreApplication>
#include <QDir>
#include <QListWidget>
#include <QStackedWidget>
#include <cmath>

// ═══════════════════════════════════════════════════════════
// 参数序列化 / 反序列化
// ═══════════════════════════════════════════════════════════

static double roundedJsonNumber(float value, int decimals)
{
    const double scale = std::pow(10.0, decimals);
    return std::round(static_cast<double>(value) * scale) / scale;
}

static QJsonArray floatsToJson(const float *arr, int n, int decimals = 6)
{
    QJsonArray a;
    for (int i = 0; i < n; ++i) a.append(roundedJsonNumber(arr[i], decimals));
    return a;
}

static void jsonToFloats(const QJsonArray &a, float *arr, int n)
{
    for (int i = 0; i < n && i < a.size(); ++i)
        arr[i] = static_cast<float>(a[i].toDouble());
}

static QJsonArray intsToJson(const int *arr, int n)
{
    QJsonArray a;
    for (int i = 0; i < n; ++i) a.append(arr[i]);
    return a;
}

static void jsonToInts(const QJsonArray &a, int *arr, int n)
{
    for (int i = 0; i < n && i < a.size(); ++i)
        arr[i] = a[i].toInt();
}

static QString enumText(int index, const QStringList &items)
{
    return index >= 0 && index < items.size() ? items[index] : QString::number(index);
}

static int enumIndex(const QJsonValue &value, const QStringList &items, int fallback)
{
    if (value.isDouble()) return value.toInt(fallback); // Legacy index format.
    const int index = items.indexOf(value.toString());
    return index >= 0 ? index : fallback;
}

static QJsonArray enumsToJson(const int *values, int count, const QStringList &items)
{
    QJsonArray result;
    for (int i = 0; i < count; ++i) result.append(enumText(values[i], items));
    return result;
}

static void jsonToEnums(const QJsonArray &array, int *values, int count,
                        const QStringList &items)
{
    for (int i = 0; i < count && i < array.size(); ++i)
        values[i] = enumIndex(array[i], items, values[i]);
}

static const QStringList &voltItems()           { static const QStringList s = {"110 V", "40 V", "20 V"}; return s; }
static const QStringList &tempCorrectItems()    { static const QStringList s = {QString::fromUtf8("关"), QString::fromUtf8("开")}; return s; }
static const QStringList &aDataLenItems()       { static const QStringList s = {QString::fromUtf8("100 点"), QString::fromUtf8("200 点"), QString::fromUtf8("400 点")}; return s; }
static const QStringList &rectifyItems()        { static const QStringList s = {QString::fromUtf8("全波"), QString::fromUtf8("正半波"), QString::fromUtf8("负半波")}; return s; }
static const QStringList &videoItems()          { static const QStringList s = {QString::fromUtf8("无"), "1", "2", "3", "4", QString::fromUtf8("平滑")}; return s; }
static const QStringList &gateSelectItems()     { static const QStringList s = {QString::fromUtf8("A 闸门"), QString::fromUtf8("B 闸门"), QString::fromUtf8("C 闸门")}; return s; }
static const QStringList &gateMeasureItems()    { static const QStringList s = {QString::fromUtf8("峰值"), QString::fromUtf8("前沿")}; return s; }
static const QStringList &onOffItems()          { static const QStringList s = {QString::fromUtf8("关"), QString::fromUtf8("开")}; return s; }
static const QStringList &alarmSoundItems()     { static const QStringList s = {QString::fromUtf8("关"), QString::fromUtf8("A 门"), QString::fromUtf8("B 门"), QString::fromUtf8("AB 门")}; return s; }
static const QStringList &probeTypeItems()      { static const QStringList s = {QString::fromUtf8("自定义"), "2.5L16", "5.0S64"}; return s; }
static const QStringList &wedgeEnableItems()    { static const QStringList s = {QString::fromUtf8("否"), QString::fromUtf8("是")}; return s; }
static const QStringList &wedgeTypeItems()      { static const QStringList s = {QString::fromUtf8("自定义"), "GW-PA"}; return s; }
static const QStringList &materialItems()       { static const QStringList s = {QString::fromUtf8("钢纵波"), QString::fromUtf8("钢横波")}; return s; }
static const QStringList &scanTypeItems()       { static const QStringList s = {QString::fromUtf8("S 扫"), QString::fromUtf8("L 扫")}; return s; }
static const QStringList &directionItems()      { static const QStringList s = {QString::fromUtf8("正向"), QString::fromUtf8("反向")}; return s; }
static const QStringList &calibItemItems()      { static const QStringList s = {QString::fromUtf8("声速"), QString::fromUtf8("声束延迟"), "ACG", "TCG"}; return s; }
static const QStringList &calibEnableItems()    { static const QStringList s = {QString::fromUtf8("关闭"), "ACG"}; return s; }

static const QStringList &filterTexts()
{
    static const QStringList items = {
        "0.5-20.0 MHz", "0.5-15.0 MHz", "0.5-10.0 MHz", "0.5-5.0 MHz",
        "1.0-20.0 MHz", "3.0-20.0 MHz", "5.0-20.0 MHz", "7.0-20.0 MHz",
        "10.0-20.0 MHz", "1.0 MHz", "2.5 MHz", "4.0 MHz",
        "5.0 MHz", "7.5 MHz", "10.0 MHz", "15.0 MHz"
    };
    return items;
}

static QJsonArray shortsToJson(const short *arr, int n)
{
    QJsonArray a;
    for (int i = 0; i < n; ++i) a.append(static_cast<int>(arr[i]));
    return a;
}

static void jsonToShorts(const QJsonArray &a, short *arr, int n)
{
    for (int i = 0; i < n && i < a.size(); ++i)
        arr[i] = static_cast<short>(a[i].toInt());
}

// ═══════════════════════════════════════════════════════════
// PA 参数字段声明表 — X-Macro 模式
// 每个字段声明一次，在 serialize / deserialize / _comments
// 三种上下文中定义不同宏语义，自动展开
// 新增字段只需在此表添加一行
// ═══════════════════════════════════════════════════════════
//
// 宏类型:
//   M_INT   (key, sub, field, comment)                 — int ↔ JSON int
//   M_FLOAT (key, sub, field, dec, comment)            — float ↔ JSON rounded(dec)
//   M_ENUM  (key, sub, field, itemsFn, comment)        — int enum ↔ JSON text
//   M_FARR  (key, sub, field, cnt, dec, comment)       — float[cnt] ↔ JSON array
//   M_SFARR (key, sub, field, cnt, dec, comment)       — float2D ↔ JSON array (reinterpret_cast)
//   M_SSARR (key, sub, field, cnt, comment)            — short2D ↔ JSON array (reinterpret_cast)
//   M_EARR  (key, sub, field, cnt, itemsFn, comment)   — int[cnt] ↔ JSON enum array

#define PA_PARAM_FIELDS(M) \
    /* ── 发射 ── */ \
    M_ENUM("highVoltage",   tx, highVoltage,   voltItems,          "发射电压档位") \
    M_INT("pulseWidth",     tx, pulseWidth,                         "发射脉冲宽度") \
    M_INT("prf",            tx, prf,                                "脉冲重复频率") \
    M_FLOAT("range",        tx, range, 1,                           "A扫检测范围") \
    M_ENUM("tempCorrect",   tx, tempCorrect,   tempCorrectItems,   "温度补偿开关") \
    M_ENUM("aDataLen",      tx, aDataLen,      aDataLenItems,      "A扫数据长度") \
    /* ── 接收 ── */ \
    M_FLOAT("aGain",        rx, aGain, 1,                           "模拟增益") \
    M_FLOAT("dGain",        rx, dGain, 1,                           "数字增益") \
    M_INT("curBeam",        rx, curBeam,                            "当前声束编号") \
    M_ENUM("rectify",       rx, rectify,        rectifyItems,      "检波方式") \
    M_ENUM("filter",        rx, filter,          filterTexts,      "滤波档位") \
    M_ENUM("video",         rx, video,           videoItems,        "视频滤波开关") \
    /* ── 闸门 ── */ \
    M_ENUM("gateSelect",    gate, gateSelect,    gateSelectItems,   "当前选中的闸门，0=A、1=B、2=C") \
    M_FARR("gateStart",     gate, gateStart, 3, 1,                 "A/B/C闸门起始位置数组") \
    M_FARR("gateWidth",     gate, gateWidth, 3, 1,                 "A/B/C闸门宽度数组") \
    M_FARR("gateThreshold", gate, gateThreshold, 3, 1,            "A/B/C闸门阈值数组") \
    M_EARR("gateMeasure",   gate, gateMeasure, 3, gateMeasureItems, "A/B/C闸门测量方式数组") \
    M_ENUM("alarmSound",    gate, alarmSound,    alarmSoundItems,  "报警声音开关") \
    /* ── 探头 ── */ \
    M_ENUM("probeType",     probe, probeType,    probeTypeItems,   "探头类型") \
    M_FLOAT("probeFreq",    probe, probeFreq, 1,                   "探头中心频率") \
    M_INT("probeCount",     probe, probeCount,                      "探头阵元数量") \
    M_FLOAT("probePitch",   probe, probePitch, 2,                   "探头阵元间距") \
    /* ── 楔块 ── */ \
    M_ENUM("wedgeEnable",   wedge, wedgeEnable,  wedgeEnableItems, "楔块启用开关") \
    M_ENUM("wedgeType",     wedge, wedgeType,    wedgeTypeItems,   "楔块类型") \
    M_FLOAT("wedgeAngle",   wedge, wedgeAngle, 1,                   "楔块角度") \
    M_INT("wedgeVelocity",  wedge, wedgeVelocity,                   "楔块声速") \
    M_FLOAT("wedgeHeight",  wedge, wedgeHeight, 1,                  "楔块高度") \
    /* ── 工件 ── */ \
    M_ENUM("material",      wp, material,         materialItems,   "工件材料类型") \
    M_INT("lVelocity",      wp, lVelocity,                          "工件纵波声速") \
    M_ENUM("traceEnable",   wp, traceEnable,      wedgeEnableItems, "轨迹显示开关") \
    /* ── 扫查 ── */ \
    M_ENUM("scanType",      scan, scanType,       scanTypeItems,    "扫描类型") \
    M_INT("eleStart",       scan, eleStart,                          "起始阵元") \
    M_INT("eleEnd",         scan, eleEnd,                            "结束阵元") \
    M_INT("eleAperture",    scan, eleAperture,                       "有效孔径阵元数") \
    M_FLOAT("angleFrom",    scan, angleFrom, 0,                      "扫描起始角度") \
    M_FLOAT("angleTo",      scan, angleTo, 0,                        "扫描终止角度") \
    M_FLOAT("angle",        scan, angle, 0,                          "固定扫描角度") \
    M_FLOAT("focus",        scan, focus, 1,                          "聚焦深度") \
    /* ── 成像 ── */ \
    M_INT("imgLineX1",      img, imgLineX1,                          "C扫成像X方向起始线") \
    M_INT("imgLineX2",      img, imgLineX2,                          "C扫成像X方向终止线") \
    M_INT("imgLineY1",      img, imgLineY1,                          "C扫成像Y方向起始线") \
    M_INT("imgLineY2",      img, imgLineY2,                          "C扫成像Y方向终止线") \
    M_FLOAT("degPerPoint",  img, degPerPoint, 1,                     "C扫每采样点对应角度") \
    /* ── 编码器 ── */ \
    M_ENUM("direction",     enc, direction,       directionItems,   "编码器运动方向") \
    M_FLOAT("coderDeg",     enc, coderDeg, 3,                        "编码器每点角度") \
    M_FLOAT("checkDistance", enc, checkDistance, 1,                   "检测距离") \
    /* ── 校准/TCG ── */ \
    M_ENUM("calibItem",     tcg, calibItem,       calibItemItems,   "当前校准项目") \
    M_FLOAT("realDistance",  tcg, realDistance, 1,                    "校准实际距离") \
    M_FLOAT("beamDelay",    tcg, beamDelay, 1,                       "声束延迟") \
    M_FLOAT("tcgCoeff",     tcg, tcgCoeff, 3,                        "TCG补偿系数") \
    M_ENUM("calibEnable",   tcg, calibEnable,     calibEnableItems, "校准启用开关") \
    M_INT("tcgStart",       tcg, tcgStart,                           "TCG起始点") \
    M_INT("tcgEnd",         tcg, tcgEnd,                             "TCG终止点") \
    M_ENUM("acgSwitch",     tcg, acgSwitch,       onOffItems,       "ACG开关") \
    M_ENUM("tcgSwitch",     tcg, tcgSwitch,       onOffItems,       "TCG开关") \
    M_FARR("tcgX",          tcg, tcgX, 6, 6,                        "TCG控制点位置数组") \
    M_FARR("tcgRatio",      tcg, tcgRatio, 6, 6,                    "TCG控制点增益比例数组") \
    M_FARR("acgValue",      tcg, acgValue, 128, 6,                  "各声束ACG值数组") \
    M_SSARR("tcgPointX",    tcg, tcgPointX, 10*128,                 "各声束TCG点位置数组") \
    M_SFARR("tcgPointValue", tcg, tcgPointValue, 10*128, 6,         "各声束TCG点增益数组") \
    /* ── MFC 补齐 ── */ \
    M_INT("sVelocity",      wp, sVelocity,                          "工件横波声速") \
    M_INT("diameter",       wp, diameter,                           "工件直径") \
    M_EARR("gateAlarm",     gate, gateAlarm, 3, onOffItems,        "A/B/C闸门报警方式数组") \
    M_EARR("gateTrace",     gate, gateTrace, 3, onOffItems,        "A/B/C闸门跟踪方式数组") \
    M_FLOAT("probeDelay",   probe, probeDelay, 3,                   "探头延迟") \
    /* ── TFM ── */ \
    M_FLOAT("dimX",         tfm, dimX, 3,                           "TFM成像X尺寸") \
    M_FLOAT("dimY",         tfm, dimY, 3,                           "TFM成像Y尺寸") \
    M_FLOAT("offsetX",      tfm, offsetX, 3,                        "TFM成像X偏移") \
    M_FLOAT("offsetY",      tfm, offsetY, 3,                        "TFM成像Y偏移") \
    M_FLOAT("pixelSize",    tfm, pixelSize, 3,                      "TFM像素尺寸") \
    M_FLOAT("pieceThickness", tfm, pieceThickness, 3,               "工件厚度") \
    M_FLOAT("tfmDGain",     tfm, tfmDGain, 3,                       "TFM数字增益") \
    M_INT("tfmSmooth",      tfm, tfmSmooth,                         "TFM平滑参数") \
    M_INT("parRestrainH16", tfm, parRestrainH16,                    "TFM高16位抑制参数") \
    M_INT("parRestrainL16", tfm, parRestrainL16,                    "TFM低16位抑制参数") \
    M_INT("circleDeg",      enc, circleDeg,                          "编码器一周计数") \
    M_FLOAT("imgSpanStart",  img, imgSpanStart, 3,                  "C扫成像跨度起点") \
    M_FLOAT("imgSpanEnd",    img, imgSpanEnd, 3,                    "C扫成像跨度终点") \
    /* ── 全局 ── */ \
    M_INT("readNum",        global, readNum,                         "全局读取序号") \
    M_INT("beamCount",      global, beamCount,                       "有效声束数量") \
    M_INT("tempBeamCount",  global, tempBeamCount,                   "临时声束数量")
QJsonObject ParamPage::serializeParams() const
{
    const auto &p = m_params;
    QJsonObject j;

    // ── X-Macro 展开: 序列化所有 PA_PARAM_FIELDS 字段 ──
#define M_INT(key, sub, field, _comment)             j[key] = p.sub.field;
#define M_FLOAT(key, sub, field, dec, _comment)      j[key] = roundedJsonNumber(p.sub.field, dec);
#define M_ENUM(key, sub, field, itemsFn, _comment)   j[key] = enumText(p.sub.field, itemsFn());
#define M_FARR(key, sub, field, cnt, dec, _comment)  j[key] = floatsToJson(p.sub.field, cnt, dec);
#define M_SFARR(key, sub, field, cnt, dec, _comment) j[key] = floatsToJson(reinterpret_cast<const float*>(p.sub.field), cnt, dec);
#define M_SSARR(key, sub, field, cnt, _comment)      j[key] = shortsToJson(reinterpret_cast<const short*>(p.sub.field), cnt);
#define M_EARR(key, sub, field, cnt, itemsFn, _comment) j[key] = enumsToJson(p.sub.field, cnt, itemsFn());

    PA_PARAM_FIELDS(M)

#undef M_INT
#undef M_FLOAT
#undef M_ENUM
#undef M_FARR
#undef M_SFARR
#undef M_SSARR
#undef M_EARR

    // 声束描述符（仅保存非零项以减少文件体积）
    {
        QJsonArray beamArr;
        for (int i = 0; i < 128; ++i) {
            const auto &b = p.global.beams[i];
            if (b.x0 == 0 && b.y0 == 0 && b.x1 == 0 && b.y1 == 0) continue;
            QJsonObject bo;
            bo["i"]  = i;
            bo["x0"] = b.x0; bo["y0"] = b.y0;
            bo["x1"] = b.x1; bo["y1"] = b.y1;
            beamArr.append(bo);
        }
        if (!beamArr.isEmpty())
            j["beams"] = beamArr;
    }

    // 参数注释字典 — 从 PA_PARAM_FIELDS 自动生成
    QJsonObject comments;
#define M_INT(key, _sub, _field, comment)                comments[key] = QString::fromUtf8(comment);
#define M_FLOAT(key, _sub, _field, _dec, comment)        comments[key] = QString::fromUtf8(comment);
#define M_ENUM(key, _sub, _field, _itemsFn, comment)     comments[key] = QString::fromUtf8(comment);
#define M_FARR(key, _sub, _field, _cnt, _dec, comment)   comments[key] = QString::fromUtf8(comment);
#define M_SFARR(key, _sub, _field, _cnt, _dec, comment)  comments[key] = QString::fromUtf8(comment);
#define M_SSARR(key, _sub, _field, _cnt, comment)        comments[key] = QString::fromUtf8(comment);
#define M_EARR(key, _sub, _field, _cnt, _itemsFn, comment) comments[key] = QString::fromUtf8(comment);

    PA_PARAM_FIELDS(M)

#undef M_INT
#undef M_FLOAT
#undef M_ENUM
#undef M_FARR
#undef M_SFARR
#undef M_SSARR
#undef M_EARR

    comments["beams"] = QString::fromUtf8("非零声束几何描述，i为编号，x0/y0和x1/y1为端点");
    j["_comments"] = comments;

    return j;
}

void ParamPage::deserializeParams(const QJsonObject &j)
{
    auto &p = m_params;

    // ── X-Macro 展开: 反序列化所有 PA_PARAM_FIELDS 字段 ──
#define M_INT(key, sub, field, _comment)             if (j.contains(key)) p.sub.field = j[key].toInt();
#define M_FLOAT(key, sub, field, dec, _comment)      if (j.contains(key)) p.sub.field = static_cast<float>(j[key].toDouble());
#define M_ENUM(key, sub, field, itemsFn, _comment)   if (j.contains(key)) p.sub.field = enumIndex(j[key], itemsFn(), p.sub.field);
#define M_FARR(key, sub, field, cnt, dec, _comment)  if (j.contains(key)) jsonToFloats(j[key].toArray(), p.sub.field, cnt);
#define M_SFARR(key, sub, field, cnt, dec, _comment) if (j.contains(key)) jsonToFloats(j[key].toArray(), reinterpret_cast<float*>(p.sub.field), cnt);
#define M_SSARR(key, sub, field, cnt, _comment)      if (j.contains(key)) jsonToShorts(j[key].toArray(), reinterpret_cast<short*>(p.sub.field), cnt);
#define M_EARR(key, sub, field, cnt, itemsFn, _comment) if (j.contains(key)) jsonToEnums(j[key].toArray(), p.sub.field, cnt, itemsFn());

    PA_PARAM_FIELDS(M)

#undef M_INT
#undef M_FLOAT
#undef M_ENUM
#undef M_FARR
#undef M_SFARR
#undef M_SSARR
#undef M_EARR

    // 声束描述符
    if (j.contains("beams")) {
        QJsonArray arr = j["beams"].toArray();
        for (const auto &v : arr) {
            QJsonObject bo = v.toObject();
            int i = bo["i"].toInt();
            if (i < 0 || i >= 128) continue;
            p.global.beams[i].x0 = bo["x0"].toInt();
            p.global.beams[i].y0 = bo["y0"].toInt();
            p.global.beams[i].x1 = bo["x1"].toInt();
            p.global.beams[i].y1 = bo["y1"].toInt();
        }
    }
}

// ═══════════════════════════════════════════════════════════
// 从 m_params 回写所有 UI 控件
// ═══════════════════════════════════════════════════════════

void ParamPage::syncUiFromParams()
{
    const auto &p = m_params;

    // 批量阻断信号，避免触发硬件下发
    QSignalBlocker b1(m_voltCombo);
    QSignalBlocker b2(m_pulseWidthSpin);
    QSignalBlocker b3(m_prfSpin);
    QSignalBlocker b4(m_rangeSpin);
    QSignalBlocker b5(m_tempCorrectCombo);
    QSignalBlocker b6(m_aDataLenCombo);
    QSignalBlocker b7(m_aGainSpin);
    QSignalBlocker b8(m_dGainSpin);
    QSignalBlocker b9(m_beamNoSpin);
    QSignalBlocker ba(m_rectifyCombo);
    QSignalBlocker bb(m_filterCombo);
    QSignalBlocker bc(m_videoCombo);
    QSignalBlocker bd(m_gateSelCombo);
    QSignalBlocker be(m_gateStartSpin);
    QSignalBlocker bf(m_gateWidthSpin);
    QSignalBlocker bg(m_gateThreshSpin);
    QSignalBlocker bh(m_gateMeasureCombo);
    QSignalBlocker bi(m_gateAlarmCombo);
    QSignalBlocker bj(m_gateTraceCombo);
    QSignalBlocker bk(m_alarmSoundCombo);
    QSignalBlocker bl(m_probeTypeCombo);
    QSignalBlocker bm(m_probeFreqSpin);
    QSignalBlocker bn(m_probeCountSpin);
    QSignalBlocker bo(m_probePitchSpin);
    QSignalBlocker bp(m_wedgeEnableCombo);
    QSignalBlocker bq(m_wedgeTypeCombo);
    QSignalBlocker br(m_wedgeAngleSpin);
    QSignalBlocker bs(m_wedgeVelSpin);
    QSignalBlocker bt(m_wedgeHeightSpin);
    QSignalBlocker bu(m_materialCombo);
    QSignalBlocker bv(m_lVelSpin);
    QSignalBlocker bw(m_traceEnableCombo);

    // ── 发射 ──
    m_voltCombo->setCurrentIndex(p.tx.highVoltage);
    m_pulseWidthSpin->setValue(p.tx.pulseWidth);
    m_prfSpin->setValue(p.tx.prf);
    m_rangeSpin->setValue(static_cast<double>(p.tx.range));
    m_tempCorrectCombo->setCurrentIndex(p.tx.tempCorrect);
    m_aDataLenCombo->setCurrentIndex(p.tx.aDataLen);

    // ── 接收 ──
    m_aGainSpin->setValue(static_cast<double>(p.rx.aGain));
    m_dGainSpin->setValue(static_cast<double>(p.rx.dGain));
    m_beamNoSpin->setValue(p.rx.curBeam + 1);
    m_rectifyCombo->setCurrentIndex(p.rx.rectify);
    m_filterCombo->setCurrentIndex(p.rx.filter);
    m_videoCombo->setCurrentIndex(p.rx.video);

    // ── 闸门 ──
    m_gateSelCombo->setCurrentIndex(p.gate.gateSelect);
    m_gateStartSpin->setValue(static_cast<double>(p.gate.gateStart[p.gate.gateSelect]));
    m_gateWidthSpin->setValue(static_cast<double>(p.gate.gateWidth[p.gate.gateSelect]));
    m_gateThreshSpin->setValue(static_cast<double>(p.gate.gateThreshold[p.gate.gateSelect]));
    m_gateMeasureCombo->setCurrentIndex(p.gate.gateMeasure[p.gate.gateSelect]);
    m_gateAlarmCombo->setCurrentIndex(p.gate.gateAlarm[p.gate.gateSelect]);
    m_gateTraceCombo->setCurrentIndex(p.gate.gateTrace[p.gate.gateSelect]);
    m_alarmSoundCombo->setCurrentIndex(p.gate.alarmSound);

    // ── 探头 ──
    m_probeTypeCombo->setCurrentIndex(p.probe.probeType);
    m_probeFreqSpin->setValue(static_cast<double>(p.probe.probeFreq));
    m_probeCountSpin->setValue(p.probe.probeCount);
    m_probePitchSpin->setValue(static_cast<double>(p.probe.probePitch));

    // ── 楔块 ──
    m_wedgeEnableCombo->setCurrentIndex(p.wedge.wedgeEnable);
    m_wedgeTypeCombo->setCurrentIndex(p.wedge.wedgeType);
    m_wedgeAngleSpin->setValue(static_cast<double>(p.wedge.wedgeAngle));
    m_wedgeVelSpin->setValue(p.wedge.wedgeVelocity);
    m_wedgeHeightSpin->setValue(static_cast<double>(p.wedge.wedgeHeight));

    // ── 工件 ──
    m_materialCombo->setCurrentIndex(p.wp.material);
    m_lVelSpin->setValue(p.wp.lVelocity);
    m_traceEnableCombo->setCurrentIndex(p.wp.traceEnable);

    // ── 扫查（重建动态槽位控件）──
    if (m_scanTypeCombo) {
        m_scanTypeCombo->blockSignals(true);
        m_scanTypeCombo->setCurrentIndex(p.scan.scanType);
        m_scanTypeCombo->blockSignals(false);
    }
    onScanTypeChanged(p.scan.scanType);

    if (m_tcgPage)
        m_tcgPage->syncFromParams();

    // 通知闸门同步
    emit gateParamsChanged();
}

// ── 文件目录辅助 ──
static QString paramsDir()  { QString d = QCoreApplication::applicationDirPath() + "/params";  QDir().mkpath(d); return d; }
static QString dataDir()    { QString d = QCoreApplication::applicationDirPath() + "/data";    QDir().mkpath(d); return d; }

bool ParamPage::initializeParams()
{
    const QString path = paramsDir() + "/default.json";
    QFile file(path);

    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly))
            return false;

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            return false;

        deserializeParams(document.object());
        syncUiFromParams();
        return true;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(serializeParams()).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}

void ParamPage::onSaveParams()
{
    // Commit the visible gate editor before serializing all three gate records.
    const int gate = qBound(0, m_params.gate.gateSelect, 2);
    m_params.gate.gateStart[gate] = static_cast<float>(m_gateStartSpin->value());
    m_params.gate.gateWidth[gate] = static_cast<float>(m_gateWidthSpin->value());
    m_params.gate.gateThreshold[gate] = static_cast<float>(m_gateThreshSpin->value());
    m_params.gate.gateMeasure[gate] = m_gateMeasureCombo->currentIndex();
    m_params.gate.gateAlarm[gate] = m_gateAlarmCombo->currentIndex();
    m_params.gate.gateTrace[gate] = m_gateTraceCombo->currentIndex();

    QString path = QFileDialog::getSaveFileName(
        this, QString::fromUtf8("保存参数"), paramsDir(),
        QString::fromUtf8("参数文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;

    QJsonDocument doc(serializeParams());
    file.write(doc.toJson(QJsonDocument::Indented));
}

void ParamPage::onLoadParams()
{
    QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("调用参数"), paramsDir(),
        QString::fromUtf8("参数文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return;
    if (!doc.isObject()) return;

    deserializeParams(doc.object());
    syncUiFromParams();
    if (m_dispatcher && m_dispatcher->isConnected())
        applyCurrentParams();

    // 加载后自动展开参数面板，确保用户能看到更新后的值
    if (!m_stack->isVisible()) {
        m_activeRow = 0;  // 发射页
        m_nav->setCurrentRow(0);
        m_stack->setCurrentIndex(0);
        m_stack->show();
        setFixedWidth(kExpandedWidth);
        updateGeometry();
    }
    updateCScanButtons();
}

void ParamPage::onSaveData()
{
    QString path = QFileDialog::getSaveFileName(
        this, QString::fromUtf8("保存C扫数据"), dataDir(),
        QString::fromUtf8("PA数据 (*.dat);;所有文件 (*)"));
    if (path.isEmpty()) return;
    emit saveDataRequested(path);
}

void ParamPage::onReplayData()
{
    QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("回放C扫数据"), dataDir(),
        QString::fromUtf8("C扫数据 (*.dat);;所有文件 (*)"));
    if (path.isEmpty()) return;
    emit replayDataRequested(path);
}

// ──────────────────────────────────────────────
// 主UI构建
// ──────────────────────────────────────────────

