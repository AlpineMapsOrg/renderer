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
    UIntIdManager(const QDate& reference_date);
    QColor convert_region_id_to_color(const QString& region_id);
    QString convert_color_to_region_id(const QColor& color) const;
    uint convert_region_id_to_internal_id(const QString& color);
    QString convert_internal_id_to_region_id(const uint& internal_id) const;
    uint convert_color_to_internal_id(const QColor& color) const;
    bool contains(const QString& region_id) const;
    std::vector<QString> get_all_registered_region_ids() const;
    QDate get_reference_date() const { return m_reference_date; }

private:
    std::unordered_map<QString, uint> m_region_id_to_internal_id;
    std::unordered_map<uint, QString> m_internal_id_to_region_id;
    uint m_max_internal_id = 0;
    QDate m_reference_date;
};
} // namespace nucleus::avalanche
