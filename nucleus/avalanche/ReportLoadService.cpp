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
namespace avalanche::eaws {

// Constructor: only creates network manager that lives the whole runtime. Ideally the whole app would only use one Manager !
ReportLoadService::ReportLoadService()
    : m_network_manager(new QNetworkAccessManager(this))
{
}

void ReportLoadService::load_from_tu_wien(const QDate& date) const
{
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
    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        // Error message is emitted in case something goes wrong
        QString error_message("ERROR: ");

        // Check if Network Error occured
        if (reply->error() != QNetworkReply::NoError) {
            emit this->load_from_TU_Wien_finished(tl::unexpected(error_message));
        }

        // Read the response data
        QByteArray data = reply->readAll();

        // Convert data to Json
        QJsonParseError parse_error;
        QJsonDocument json_document = QJsonDocument::fromJson(data, &parse_error);

        // Check for parsing error
        if (parse_error.error != QJsonParseError::NoError) {
            error_message.append("Parse error = ").append(parse_error.errorString());
            emit this->load_from_TU_Wien_finished(tl::unexpected(error_message));
            return;
        }

        // Check for empty json
        if (json_document.isEmpty() || json_document.isNull()) {
            error_message.append("Empty or Null json.");
            emit this->load_from_TU_Wien_finished(tl::unexpected(error_message));
            return;
        }

        // Check if json doc is array
        if (!json_document.isArray()) {
            error_message.append("jsonDocument does not contain array.");
            emit this->load_from_TU_Wien_finished(tl::unexpected(error_message));
            return;
        }

        // Create json Array and check if empty
        QJsonArray jsonArray = json_document.array();
        if (jsonArray.isEmpty()) {
            error_message.append("Array empty!");
            emit this->load_from_TU_Wien_finished(tl::unexpected(error_message));
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
                emit this->load_from_TU_Wien_finished(tl::unexpected(error_message));
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

        // Emit ratings
        emit this->load_from_TU_Wien_finished(tl::expected<std::vector<ReportTUWien>, QString>(region_ratings));
    });
}

} // namespace avalanche::eaws
