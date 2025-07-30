#include "UIntIdManager.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <extern/tl_expected/include/tl/expected.hpp>
#include <iostream>

namespace nucleus::avalanche {
UIntIdManager::UIntIdManager()
{
    // intern_id = 0 means "no region"
    region_id_to_internal_id[QString("")] = 0;
    internal_id_to_region_id[0] = QString("");
    assert(max_internal_id == 0);

    // Load names of all eaws regions (past and present) from file where
    // int id is order of listing in file
    /*
    QString filePath = "app\\app\\eaws\\eawsRegionNames.json";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cout << "\nERROR: Parse Error wile parsing JSON with EAWS region ids.";
    }
    QByteArray jsonData = file.readAll();
    file.close();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        std::cout << ("Invalid JSON file");
    }
    QJsonObject obj = doc.object();
    uint i = 1;
    for (const QString& key : obj.keys()) {
        region_id_to_internal_id[key] = i;
        internal_id_to_region_id[i] = key;
        max_internal_id = i;
        i++;
    }
    */

    // Only load regions that match current report server
    QDate referenceDate(2025, 7, 1);

    // Go through all geojson files with regions for austria
    for (int x = 2; x <= 8; x++) {

        // Read file
        QString filePath((std::string("app\\app\\eaws\\AT-0") + std::to_string(x) + std::string("_micro-regions.geojson.json")).c_str());
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Could not open file:" << filePath;
            return;
        }
        QByteArray data = file.readAll();

        // Parse json
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << parseError.errorString();
            return;
        }

        // Go through all regions in json and get its id . add id to manager if it was valid a reference date (must be same date as files on server)
        QJsonObject root = doc.object();
        QJsonArray features = root["features"].toArray();
        for (const QJsonValue& featureVal : features) {
            QJsonObject featureObj = featureVal.toObject();
            QJsonObject properties = featureObj["properties"].toObject();

            // read id start and end date (if dates exist)
            QString id = properties["id"].toString();
            QString startStr = properties.contains("start_date") ? properties["start_date"].toString() : QString();
            QString endStr = properties.contains("end_date") ? properties["end_date"].toString() : QString();

            // If start date feature did not exist: treat is as very old start date
            QDate startDate = QDate::fromString(startStr, Qt::ISODate);
            if (!startDate.isValid()) {
                startDate = QDate(2000, 1, 1); // Default to very old date
            }

            // If end date feature does not exist treat it as end date = null which means valid until further notice
            QDate endDate = QDate::fromString(endStr, Qt::ISODate);

            // Apply filtering: only regions that were valid at reference date are added to our manager
            if (startDate.isValid() && startDate < referenceDate && (!endDate.isValid() || endDate > referenceDate)) {
                this->region_id_to_internal_id[id] = region_id_to_internal_id.size();
                this->internal_id_to_region_id[internal_id_to_region_id.size()] = id;
                max_internal_id = region_id_to_internal_id.size() - 1;
            }
        }
    }
}

uint UIntIdManager::convert_region_id_to_internal_id(const QString& region_id) const
{
    // If Key exists return its value, otherwise return 0
    auto entry = region_id_to_internal_id.find(region_id);
    if (entry == region_id_to_internal_id.end())
        return 0;
    else
        return entry->second;
}

QString UIntIdManager::convert_internal_id_to_region_id(const uint& internal_id) const
{
    auto entry = internal_id_to_region_id.find(internal_id);
    if (entry == internal_id_to_region_id.end())
        return QString("");
    else
        return entry->second;
}

QColor UIntIdManager::convert_region_id_to_color(const QString& region_id, QImage::Format color_format) const
{
    assert(this->checkIfImageFormatSupported(color_format));
    const uint& internal_id = this->convert_region_id_to_internal_id(region_id);
    uint red = internal_id / 256;
    uint green = internal_id % 256;
    return QColor::fromRgb(red, green, 0);
}

QString UIntIdManager::convert_color_to_region_id(const QColor& color, const QImage::Format& color_format) const
{
    assert(QImage::Format_ARGB32 == color_format);
    uint internal_id = color.red() * 256 + color.green();
    auto entry = internal_id_to_region_id.find(internal_id);
    if (entry == internal_id_to_region_id.end())
        return QString("");
    return internal_id_to_region_id.at(internal_id);
}

uint UIntIdManager::convert_color_to_internal_id(const QColor& color, const QImage::Format& color_format) const
{
    return this->convert_region_id_to_internal_id(this->convert_color_to_region_id(color, color_format));
}

QColor UIntIdManager::convert_internal_id_to_color(const uint& internal_id, const QImage::Format& color_format) const
{
    return this->convert_region_id_to_color(this->convert_internal_id_to_region_id(internal_id), color_format);
}

bool UIntIdManager::checkIfImageFormatSupported(const QImage::Format& color_format) const
{
    for (const auto& supported_format : supported_image_formats) {
        if (color_format == supported_format)
            return true;
    }
    return false;
}

std::vector<QString> UIntIdManager::get_all_registered_region_ids() const
{
    std::vector<QString> region_ids(internal_id_to_region_id.size());
    for (const auto& [internal_id, region_id] : internal_id_to_region_id)
        region_ids[internal_id] = region_id;
    return region_ids;
}

bool UIntIdManager::contains(const QString& region_id) const { return region_id_to_internal_id.contains(region_id); }

} // namespace nucleus::avalanche
