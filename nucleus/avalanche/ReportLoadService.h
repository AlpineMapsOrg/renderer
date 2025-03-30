#pragma once
#ifndef REPORTLOADSERVICE_H
#define REPORTLOADSERVICE_H
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <extern/tl_expected/include/tl/expected.hpp>
#include <unordered_set>

namespace avalanche::eaws {

// Relevant data from a CAAML json provided by avalanche services
struct DangerRatingCAAML {
public:
    QString main_value = "";
    int lower_bound = INT_MAX;
    int upper_bound = INT_MIN;
    QString valid_time_period = "";
    bool operator==(const avalanche::eaws::DangerRatingCAAML& rhs) const = default;
};

// Contains a list of regions and the altitude dependent ratings valid for these regions
struct BulletinItemCAAML {
public:
    std::unordered_set<QString> regions_ids;
    std::vector<DangerRatingCAAML> danger_ratings;
    bool operator==(const avalanche::eaws::BulletinItemCAAML& rhs) const = default;
};

// Contains rating for one region as obtained from TU Wien server
struct ReportTUWien {
    QString region_id = "";
    QString start_time = "";
    QString end_time = "";
    int border = INT_MAX;
    int rating_hi = -1;
    int rating_lo = -1;
    int unfavorable = 0;
    bool operator==(const avalanche::eaws::ReportTUWien& rhs) const = default;
};

// Loads a Bulletinn from the server and converts it to custom struct
class ReportLoadService : public QObject {
    Q_OBJECT

public slots:
    void load_CAAML(const QString& url) const;
    void load_tu_wien(const QDate& date) const;
    void load_latest_TU_Wien() const;

signals:
    void load_CAAML_finished(tl::expected<std::vector<BulletinItemCAAML>, QString> data_from_server) const;
    void load_latest_TU_Wien_finished(tl::expected<std::vector<ReportTUWien>, QString> data_from_server) const;

public:
    // QNetworkAccessManager m_network_manager;
    const QString m_url_latest_report = "https://static.avalanche.report/bulletins/latest/EUREGIO_de_CAAMLv6.json";
    const QString m_url_custom_report = "https://static.avalanche.report/eaws_bulletins/${date}/${date}-${region}.json";
    bool operator==(const avalanche::eaws::ReportLoadService& rhs) { return this->m_url_latest_report == rhs.m_url_latest_report; }
};
} // namespace avalanche::eaws

#endif // REPORTLOADSERVICE_H
