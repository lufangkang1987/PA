#pragma once
#include "DataTypes.h"
#include "PAParams.h"
#include <QJsonObject>
#include <QString>
#include <QVector>

bool saveCScanFile(const QString &path, const QVector<float> &data,
                   int w, int h, const QJsonObject &params,
                   const QVector<DataPacket> &packets,
                   const QVector<ScanRule> &scanRules = {});

QVector<float> loadCScanFile(const QString &path, int &w, int &h,
                             QJsonObject &params, QVector<DataPacket> &packets,
                             QVector<ScanRule> *scanRules = nullptr);

