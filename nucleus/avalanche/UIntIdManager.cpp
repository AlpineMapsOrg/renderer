#include "UIntIdManager.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <extern/tl_expected/include/tl/expected.hpp>

avalanche::eaws::UIntIdManager::UIntIdManager()
{
    // intern_id = 0 means "no region"
    region_id_to_internal_id[QString("")] = 0;
    internal_id_to_region_id[0] = QString("");
    assert(max_internal_id == 0);
}

// Helper function to load latest list of eaws region ids
tl::expected<std::vector<QString>, QString> get_all_eaws_region_ids_from_server()
{
    // Read the json from file for now
    QString filePath = "app\\eaws\\micro-regions.json";
    QFile jsonFile(filePath);
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    QByteArray data = jsonFile.readAll();
    jsonFile.close();

    // Convert data to Json
    QJsonParseError parse_error;
    QJsonDocument json_document = QJsonDocument::fromJson(data, &parse_error);

    // Check for parsing error
    if (parse_error.error != QJsonParseError::NoError)
        return tl::unexpected(QString("ERROR: Parse Error wile parsing JSON with EAWS region ids."));

    // Check for empty json
    if (json_document.isEmpty() || json_document.isNull())
        return tl::unexpected(QString("ERROR: Empty JSON with EAWS region ids."));

    // Check if key with reports is correct
    if (!json_document.isObject())
        return tl::unexpected(QString("ERROR: JSON with EAWS region ids is not JSON Object"));
    QJsonObject obj = json_document.object();
    if (!obj.contains("features"))
        return tl::unexpected(QString("ERROR: JSON with EAWS region ids is not JSON Object."));
    if (!obj["features"].isArray())
        return tl::unexpected(QString("ERROR: JSON with EAWS region ids does not contain expected array"));

    // Go through all regions , only add current regions to return vector
    QJsonArray array = obj["features"].toArray();
    std::vector<QString> regions;
    for (const QJsonValue& jsonValue_region : array) {
        QJsonObject jsonObject_properties = jsonValue_region.toObject()["properties"].toObject();

        // Test if region is old (has an end date that is not null). Current regions have either no start and no end date key or they those keys but but end date is null
        if (jsonObject_properties.contains("end_date")) {
            if (!jsonObject_properties["end_date"].isNull())
                continue;
        } else
            regions.push_back(jsonValue_region.toObject()["properties"].toObject()["id"].toString());
    }

    // return vector withh ids of current regions
    return tl::expected<std::vector<QString>, QString>(regions);
}

void avalanche::eaws::UIntIdManager::load_all_regions_from_server()
{
    // Get list of all current eaws regions to complete this conversion service
    // Read the json from file for now
    tl::expected<std::vector<QString>, QString> result = get_all_eaws_region_ids_from_server();

    // Add all region ids to internal list. the index in the array is the internal uint id that belongs to the eaws region id.
    if (result.has_value()) {
        for (const QString& region_id : result.value()) {
            convert_region_id_to_internal_id(region_id);
        }
    }

    // This is heard by the ReportManager::load_all_regions_from_server which loads allr eprots and can assignthem to correct uint id.
    emit loaded_all_regions();
}

uint avalanche::eaws::UIntIdManager::convert_region_id_to_internal_id(const QString& region_id)
{
    // If Key exists return its values otherwise create it and return created value
    auto entry = region_id_to_internal_id.find(region_id);
    if (entry == region_id_to_internal_id.end()) {
        max_internal_id++;
        region_id_to_internal_id[region_id] = max_internal_id;
        assert(internal_id_to_region_id.find(max_internal_id) == internal_id_to_region_id.end());
        internal_id_to_region_id[max_internal_id] = region_id;
        assert(internal_id_to_region_id.size() == region_id_to_internal_id.size());
        return max_internal_id;
    } else
        return entry->second;
}

QString avalanche::eaws::UIntIdManager::convert_internal_id_to_region_id(const uint& internal_id) const { return internal_id_to_region_id.at(internal_id); }

QColor avalanche::eaws::UIntIdManager::convert_region_id_to_color(const QString& region_id, QImage::Format color_format)
{
    assert(this->checkIfImageFormatSupported(color_format));
    const uint& internal_id = this->convert_region_id_to_internal_id(region_id);
    assert(internal_id != 0);
    uint red = internal_id / 256;
    uint green = internal_id % 256;
    return QColor::fromRgb(red, green, 0);
}

QString avalanche::eaws::UIntIdManager::convert_color_to_region_id(const QColor& color, const QImage::Format& color_format) const
{
    assert(QImage::Format_ARGB32 == color_format);
    uint internal_id = color.red() * 256 + color.green();
    return internal_id_to_region_id.at(internal_id);
}

uint avalanche::eaws::UIntIdManager::convert_color_to_internal_id(const QColor& color, const QImage::Format& color_format)
{
    return this->convert_region_id_to_internal_id(this->convert_color_to_region_id(color, color_format));
}

QColor avalanche::eaws::UIntIdManager::convert_internal_id_to_color(const uint& internal_id, const QImage::Format& color_format)
{
    return this->convert_region_id_to_color(this->convert_internal_id_to_region_id(internal_id), color_format);
}

bool avalanche::eaws::UIntIdManager::checkIfImageFormatSupported(const QImage::Format& color_format) const
{
    for (const auto& supported_format : supported_image_formats) {
        if (color_format == supported_format)
            return true;
    }
    return false;
}

std::vector<QString> avalanche::eaws::UIntIdManager::get_all_registered_region_ids() const
{
    std::vector<QString> region_ids(internal_id_to_region_id.size());
    for (const auto& [internal_id, region_id] : internal_id_to_region_id)
        region_ids[internal_id] = region_id;
    return region_ids;
}
