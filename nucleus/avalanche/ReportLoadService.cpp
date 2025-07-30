#include "nucleus/avalanche/ReportLoadService.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <extern/tl_expected/include/tl/expected.hpp>
#include <glm/vec4.hpp>
#include <iostream>
#include <nucleus/avalanche/UIntIdManager.h>
#include <nucleus/avalanche/eaws.h>

namespace nucleus::avalanche {

// helper function that encodes reports in an uint vector
nucleus::avalanche::UboEawsReports convertReportsToUbo(
    UboEawsReports& ubo, const std::vector<ReportTUWien>& reports, std::shared_ptr<UIntIdManager> m_uint_id_manager)
{
    // Fill array with initial vectors
    std::fill(ubo.reports, ubo.reports + 1000, glm::ivec4(-2, 0, 0, 0));

    // if reports arrived as expected write them to ubo object
    for (const nucleus::avalanche::ReportTUWien& report : reports) {

        // Correct region id if it has invalid format: That means create new sub regions with same forecast
        std::vector<QString> new_region_ids;
        if (report.region_id.endsWith("-00")) {
            // remove last three digits
            QString parent_region_id = report.region_id;
            parent_region_id = parent_region_id.left(parent_region_id.length() - 3);

            // check if our map contains subregions of current report region and save these as vector
            uint i = 1;
            QString new_region_id = parent_region_id + QString("-01");
            while (m_uint_id_manager->contains(new_region_id)) {
                new_region_ids.push_back(new_region_id);
                i++;
                new_region_id = i < 10 ? parent_region_id + QString("-0") + QString::number(i) : parent_region_id + QString("-") + QString::number(i);
            }

            // If no subregions of report region were found, use report region
            if (new_region_ids.empty()) {
                new_region_ids.push_back(parent_region_id);
            }
        } else {
            new_region_ids.push_back(report.region_id);
        }

        // Write (corrected) region(s) to ubo
        for (QString new_region_id : new_region_ids) {
            uint idx = m_uint_id_manager->convert_region_id_to_internal_id(new_region_id);
            ubo.reports[idx] = glm::ivec4(report.unfavorable, report.border, report.rating_lo, report.rating_hi);
            if (idx == 0) {
                std::cout << "error";
            }
        }
    }
    return ubo;
}

// Constructor: only creates network manager that lives the whole runtime. Ideally the whole app would only use one Manager !
ReportLoadService::ReportLoadService()
    : m_network_manager(new QNetworkAccessManager(this))
    , m_uint_id_manager(std::make_shared<nucleus::avalanche::UIntIdManager>())
{
}

void ReportLoadService::load_from_tu_wien(const QDate& date) const
{
    // Prepare ubo to be returned
    UboEawsReports ubo;
    std::fill(ubo.reports, ubo.reports + 1000, glm::ivec4(-1, 0, 0, 0));

    QString date_string = date.toString("yyyy-MM-dd");
    QUrl qurl(QString("https://alpinemaps.cg.tuwien.ac.at/avalanche-reports/get-current-report?date=" + date_string));
    QNetworkRequest request(qurl);
    request.setTransferTimeout(int(8000));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    request.setAttribute(QNetworkRequest::UseCredentialsAttribute, false);
#endif

    // Make a GET request to the provided url
    QNetworkReply* reply = m_network_manager->get(request);

    // Process the reply
    QObject::connect(reply, &QNetworkReply::finished, [this, ubo, reply]() {
        // Error message is emitted in case something goes wrong
        QString error_message("\nERROR: ");

        // Check if Network Error occured
        if (reply->error() != QNetworkReply::NoError) {
            std::cout << "\nERROR: Eaws Report Load Service has network error.";
            emit this->load_from_TU_Wien_finished(ubo);
            reply->deleteLater();
            return;
        }

        // Read the response data
        QByteArray data = reply->readAll();

        /*
        ////////////////////////////////////////////////////////////////////////////////
        QFile file("app/app/eaws/report_tu_wien.json");
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning("Couldn't open file.");
        }
        QByteArray data = file.readAll();
        ///////////////////////////////////////////////////////////////////////////////////
        */
        // Convert data to Json
        QJsonParseError parse_error;
        QJsonDocument json_document = QJsonDocument::fromJson(data, &parse_error);

        // Check for parsing error
        if (parse_error.error != QJsonParseError::NoError) {
            error_message.append("Parse error = ").append(parse_error.errorString());
            std::cout << error_message.toStdString();
            emit this->load_from_TU_Wien_finished(ubo);
            reply->deleteLater();
            return;
        }

        // Check for empty json
        if (json_document.isEmpty() || json_document.isNull()) {
            error_message.append("Empty or Null json.");
            std::cout << error_message.toStdString();
            emit this->load_from_TU_Wien_finished(ubo);
            reply->deleteLater();
            return;
        }

        // Check if json doc is array
        if (!json_document.isArray()) {
            error_message.append("jsonDocument does not contain array.");
            std::cout << error_message.toStdString();
            emit this->load_from_TU_Wien_finished(ubo);
            reply->deleteLater();
            return;
        }

        // Create json Array and check if empty
        QJsonArray jsonArray = json_document.array();
        if (jsonArray.isEmpty()) {
            error_message.append("Array empty!");
            std::cout << error_message.toStdString();
            emit this->load_from_TU_Wien_finished(ubo);
            reply->deleteLater();
            return;
        }

        // parse array containing report for each region
        std::vector<ReportTUWien> region_ratings;
        for (const QJsonValue& jsonValue_region_rating : jsonArray) {
            // prepare an item that goes into the bulletin
            ReportTUWien region_rating;

            // Check if Json array contains json objects
            if (!jsonValue_region_rating.isObject()) {
                error_message.append("json object is array of other type than json object");
                std::cout << error_message.toStdString();
                emit this->load_from_TU_Wien_finished(ubo);
                reply->deleteLater();
                return;
            }

            // Write regional report to struct
            QJsonObject jsonObject_region_rating = jsonValue_region_rating.toObject();
            if (jsonObject_region_rating.contains("regionCode"))
                region_rating.region_id = jsonObject_region_rating["regionCode"].toString();
            if (jsonObject_region_rating.contains("dangerBorder"))
                region_rating.border = jsonObject_region_rating["dangerBorder"].toInt();
            if (jsonObject_region_rating.contains("dangerRatingHi"))
                region_rating.rating_hi = jsonObject_region_rating["dangerRatingHi"].toInt();
            if (jsonObject_region_rating.contains("dangerRatingLo"))
                region_rating.rating_lo = jsonObject_region_rating["dangerRatingLo"].toInt();
            if (jsonObject_region_rating.contains("startTime"))
                region_rating.start_time = jsonObject_region_rating["startTime"].toString();
            if (jsonObject_region_rating.contains("endTime"))
                region_rating.end_time = jsonObject_region_rating["endTime"].toString();

            // Write struct to vector to be returned
            region_ratings.push_back(region_rating);
        }

        // Convert reports to ubo
        nucleus::avalanche::UboEawsReports ubo = convertReportsToUbo(ubo, region_ratings, m_uint_id_manager);

        // Emit ratings
        emit this->load_from_TU_Wien_finished(ubo);
        reply->deleteLater();
    });
}

} // namespace nucleus::avalanche
