#include "UIntIdManager.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <extern/tl_expected/include/tl/expected.hpp>

namespace nucleus::avalanche {

UIntIdManager::UIntIdManager()
    : m_network_manager(new QNetworkAccessManager(this))
{
    // intern_id = 0 means "no region"
    region_id_to_internal_id[QString("")] = 0;
    internal_id_to_region_id[0] = QString("");
    assert(max_internal_id == 0);
}

QDate UIntIdManager::get_date() const { return date_of_currently_selected_report; }
void UIntIdManager::set_date(const QDate& input_date) { date_of_currently_selected_report = input_date; }

void UIntIdManager::load_all_regions_from_server()
{
    // Get list of all current eaws regions to complete this conversion service

    // Build url of server with file name
    QUrl qurl(m_url_regions);
    QNetworkRequest request(qurl);
    request.setTransferTimeout(int(8000));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    request.setAttribute(QNetworkRequest::UseCredentialsAttribute, false);
#endif

    // Make a GET request to the provided url
    QNetworkReply* reply = m_network_manager->get(request);

    // Process the reply
    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        // Check if Network Error occured
        if (reply->error() != QNetworkReply::NoError)
            emit loaded_all_regions(tl::unexpected(QString("ERROR: Network error from https://regions.avalanches.org/micro-regions.geojson")));

        // Read the response data
        QByteArray data = reply->readAll();
        reply->deleteLater();

        // Convert data to Json
        QJsonParseError parse_error;
        QJsonDocument json_document = QJsonDocument::fromJson(data, &parse_error);

        // Check for parsing error
        if (parse_error.error != QJsonParseError::NoError)
            emit loaded_all_regions(tl::unexpected(QString("ERROR: Parse Error wile parsing JSON with EAWS region ids.")));

        // Check for empty json
        else if (json_document.isEmpty() || json_document.isNull())
            emit loaded_all_regions(tl::unexpected(QString("ERROR: Empty JSON with EAWS region ids.")));

        // Check if key with reports is correct
        else if (!json_document.isObject())
            emit loaded_all_regions(tl::unexpected(QString("ERROR: JSON with EAWS region ids is not JSON Object")));
        QJsonObject obj = json_document.object();
        if (!obj.contains("features"))
            emit loaded_all_regions(tl::unexpected(QString("ERROR: JSON with EAWS region ids is not JSON Object.")));
        if (!obj["features"].isArray())
            emit loaded_all_regions(tl::unexpected(QString("ERROR: JSON with EAWS region ids does not contain expected array")));

        // No errors: Go through all regions and register them with the UIntIdManager
        QJsonArray array = obj["features"].toArray();
        int number_of_regions = 0;
        for (const QJsonValue& jsonValue_region : array) {
            QJsonObject jsonObject_properties = jsonValue_region.toObject()["properties"].toObject();
            QString region_id = jsonValue_region.toObject()["properties"].toObject()["id"].toString();
            convert_region_id_to_internal_id(region_id);
            number_of_regions++;
        }

        // This is heard by the ReportManager::load_all_regions_from_server which loads all reports and can assign them to correct uint id.
        emit loaded_all_regions(number_of_regions);
    });
}

uint UIntIdManager::convert_region_id_to_internal_id(const QString& region_id)
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

QString UIntIdManager::convert_internal_id_to_region_id(const uint& internal_id) const
{
    auto entry = internal_id_to_region_id.find(internal_id);
    if (entry == internal_id_to_region_id.end())
        return QString("");
    else
        return entry->second;
}

QColor UIntIdManager::convert_region_id_to_color(const QString& region_id, QImage::Format color_format)
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

uint UIntIdManager::convert_color_to_internal_id(const QColor& color, const QImage::Format& color_format)
{
    return this->convert_region_id_to_internal_id(this->convert_color_to_region_id(color, color_format));
}

QColor UIntIdManager::convert_internal_id_to_color(const uint& internal_id, const QImage::Format& color_format)
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
