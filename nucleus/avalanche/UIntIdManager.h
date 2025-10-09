#pragma once

#include <QDate>
#include <QImage>
#include <QObject>
#include <extern/tl_expected/include/tl/expected.hpp>

class QNetworkAccessManager;
namespace nucleus::avalanche {
// This class handles conversion from region-id strings to internal ids as uint and as color
// querying a region, that the manager does not know, returns 0
// querying an int that is 0 or unknown, returns empty string,
class UIntIdManager : public QObject {
    Q_OBJECT

public:
    const std::vector<QImage::Format> supported_image_formats { QImage::Format_ARGB32 };
    UIntIdManager();
    QColor convert_region_id_to_color(const QString& region_id) const;
    QString convert_color_to_region_id(const QColor& color) const;
    uint convert_region_id_to_internal_id(const QString& color) const;
    QString convert_internal_id_to_region_id(const uint& internal_id) const;
    uint convert_color_to_internal_id(const QColor& color) const;
    QColor convert_internal_id_to_color(const uint& internal_id) const;
    bool contains(const QString& region_id) const;
    std::vector<QString> get_all_registered_region_ids() const;
    void load_all_regions_from_server();

private:
    std::unordered_map<QString, uint> region_id_to_internal_id;
    std::unordered_map<uint, QString> internal_id_to_region_id;
    uint max_internal_id = 0;
    const QString m_url_regions = "https://regions.avalanches.org/micro-regions.geojson";
};
} // namespace nucleus::avalanche
