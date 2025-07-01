#pragma once

#include <QDate>
#include <QImage>
#include <QObject>
#include <extern/tl_expected/include/tl/expected.hpp>

class QNetworkAccessManager;
namespace nucleus::avalanche {
// This class handles conversion from region-id strings to internal ids as uint and as color
class UIntIdManager : public QObject {
    Q_OBJECT

public:
    const std::vector<QImage::Format> supported_image_formats { QImage::Format_ARGB32 };
    UIntIdManager();
    QColor convert_region_id_to_color(const QString& region_id, QImage::Format color_format = QImage::Format_ARGB32);
    QString convert_color_to_region_id(const QColor& color, const QImage::Format& color_format) const;
    uint convert_region_id_to_internal_id(const QString& color);
    QString convert_internal_id_to_region_id(const uint& internal_id) const;
    uint convert_color_to_internal_id(const QColor& color, const QImage::Format& color_format);
    QColor convert_internal_id_to_color(const uint& internal_id, const QImage::Format& color_format);
    bool checkIfImageFormatSupported(const QImage::Format& color_format) const;
    bool contains(const QString& region_id) const;
    std::vector<QString> get_all_registered_region_ids() const;
    void load_all_regions_from_server();
    QDate get_date() const; // returns date_of_currently_selected_report
    void set_date(const QDate& input_date); // sets date_of_currently_selected_report

signals:
    void loaded_all_regions(tl::expected<uint, QString> result) const; // argument:  number of regions or error

private:
    std::unordered_map<QString, uint> region_id_to_internal_id;
    std::unordered_map<uint, QString> internal_id_to_region_id;
    uint max_internal_id = 0;
    const QString m_url_regions = "https://regions.avalanches.org/micro-regions.geojson";
    QDate date_of_currently_selected_report = QDate(2024, 12, 30); // So far we can only load today's report. Later this date should be set by user
    std::shared_ptr<QNetworkAccessManager> m_network_manager;
};
} // namespace nucleus::avalanche
