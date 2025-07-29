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
