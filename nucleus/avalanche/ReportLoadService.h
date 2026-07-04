#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <extern/tl_expected/include/tl/expected.hpp>
#include <nucleus/avalanche/eaws.h>
#include <unordered_set>
class QNetworkAccessManager;

namespace nucleus::avalanche {
class UIntIdManager;

// Relevant data from a CAAML json provided by avalanche services
struct DangerRatingCAAML {
public:
    QString main_value = "";
    int lower_bound = INT_MAX;
    int upper_bound = INT_MIN;
    QString valid_time_period = "";
    bool operator==(const DangerRatingCAAML& rhs) const = default;
};

// Contains a list of regions and the altitude dependent ratings valid for these regions
struct BulletinItemCAAML {
public:
    std::unordered_set<QString> regions_ids;
    std::vector<DangerRatingCAAML> danger_ratings;
    bool operator==(const BulletinItemCAAML& rhs) const = default;
};

// Contains rating for one region as obtained from TU Wien server
struct ReportTUWien {
    QString region_id = "";
    QString start_time = "";
    QString end_time = "";
    int border = INT_MAX;
    int rating_hi = -1;
    int rating_lo = -1;
    int unfavorable = -1;
    bool operator==(const ReportTUWien& rhs) const = default;
};

// Loads a Bulletinn from the server and converts it to custom struct
class ReportLoadService : public QObject {
    Q_OBJECT
private:
    std::shared_ptr<QNetworkAccessManager> m_network_manager;
    std::shared_ptr<UIntIdManager> m_uint_id_manager;

public:
    ReportLoadService(std::shared_ptr<UIntIdManager> m_uint_id_manager); // Constructor creates a new NetworkManager with id manager it obtains from context
public slots:
    void load_from_tu_wien(const QDate& date) const;

signals:
    void load_from_TU_Wien_finished(const nucleus::avalanche::UboEawsReports& ubo) const;

public:
    // QNetworkAccessManager m_network_manager;
    const QString m_url_latest_report = "https://static.avalanche.report/bulletins/latest/EUREGIO_de_CAAMLv6.json";
    const QString m_url_custom_report = "https://static.avalanche.report/eaws_bulletins/${date}/${date}-${region}.json";

    bool operator==(const ReportLoadService& rhs) { return this->m_url_latest_report == rhs.m_url_latest_report; }
};
} // namespace nucleus::avalanche
