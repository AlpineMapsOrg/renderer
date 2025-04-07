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
void ReportLoadService::load_CAAML(const QString& url) const
{
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setTransferTimeout(int(8000));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    request.setAttribute(QNetworkRequest::UseCredentialsAttribute, false);
#endif
    // Make a GET request to the provided url
    QNetworkAccessManager network_manager;
    QNetworkReply* reply = network_manager.get(request);

    // Process the reply
    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        // Error message is emitted in case something goes wrong
        QString error_message("ERROR: ");

        // Check if Network Error occured
        if (reply->error() != QNetworkReply::NoError) {
            emit this->load_CAAML_finished(tl::unexpected(error_message));
        }

        // Read the response data
        QByteArray data = reply->readAll();
    });

    // Read the json from file for now
    QString filePath = "app\\eaws\\EUREGIO_de_CAAMLv6.json";
    QFile jsonFile(filePath);
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    QByteArray data = jsonFile.readAll();
    jsonFile.close();
    QString error_message("ERROR: ");

    // Convert data to Json
    QJsonParseError parse_error;
    QJsonDocument json_document = QJsonDocument::fromJson(data, &parse_error);

    // Check for parsing error
    if (parse_error.error != QJsonParseError::NoError) {
        error_message.append("Parse error = ").append(parse_error.errorString());
        emit this->load_CAAML_finished(tl::unexpected(error_message));
        return;
    }

    // Check for empty json
    if (json_document.isEmpty() || json_document.isNull()) {
        error_message.append("Empty or Null json.");
        emit this->load_CAAML_finished(tl::unexpected(error_message));
        return;
    }

    // Check if key with reports is correct
    if (!json_document.isObject()) {
        error_message.append("jsonDocument does not contain json object.");
        emit this->load_CAAML_finished(tl::unexpected(error_message));
        return;
    }
    QJsonObject json_object = json_document.object();
    if (!json_object.contains("bulletins")) {
        error_message.append("json object does not contain key \"bulletins\"");
        emit this->load_CAAML_finished(tl::unexpected(error_message));
        return;
    }

    // Check if array is containen in json
    if (!json_object["bulletins"].isArray()) {
        error_message.append("json object[\"bulletins\"] does not contain array");
        emit this->load_CAAML_finished(tl::unexpected(error_message));
        return;
    }
    QJsonArray jsonArray_bulletin = json_object["bulletins"].toArray();

    // Parse Array: Every item in the array is a json object with 2 relevant keys:
    // "regions" = an array of region ids (each regions id is saved in a json object together with the name of the region)
    // "dangerRatings" = an array of json objects. each objects contains a rating and whihc altitue range it is valid for
    std::vector<BulletinItemCAAML> eawsBulletin;
    for (const QJsonValue& jsonValue_bulletin_item : jsonArray_bulletin) {
        // prepare an itemthat goes inot the bulletin
        BulletinItemCAAML bulletin_item;

        // Check if Json cotains bulletin items
        if (!jsonValue_bulletin_item.isObject()) {
            error_message.append("json object[\"bulletins\"] is array of other type than json object");
            emit this->load_CAAML_finished(tl::unexpected(error_message));
            return;
        }

        // Check if bulletin items have correct form
        QJsonObject jsonObject_bulletin_item = jsonValue_bulletin_item.toObject();
        if (!jsonObject_bulletin_item.contains("regions") || !jsonObject_bulletin_item.contains("dangerRatings"))
        {
            error_message.append("bulletin item does not contain key \"regions\" or \"dangerRatings");
            emit this->load_CAAML_finished(tl::unexpected(error_message));
            return;
        }
        if (!jsonObject_bulletin_item["regions"].isArray() || !jsonObject_bulletin_item["dangerRatings"].isArray()) {
            error_message.append("bulletinItem[\"regions\"] or bulletinItem[\"dangerRatings\"] is not array");
            emit this->load_CAAML_finished(tl::unexpected(error_message));
            return;
        }

        // Parse danger ratings and write each to a new to struct
        QJsonArray jsonArray_danger_ratings = jsonObject_bulletin_item["dangerRatings"].toArray();
        for (const QJsonValue& jsonValue_danger_rating : jsonArray_danger_ratings) {
            QJsonObject jsonObject_danger_rating = jsonValue_danger_rating.toObject();
            DangerRatingCAAML rating;

            // Get danger level
            if (jsonObject_danger_rating.contains("mainValue"))
                rating.main_value = jsonObject_danger_rating["mainValue"].toString();

            // Get time period for which rating is valid
            if (jsonObject_danger_rating.contains("validTimePeriod"))
                rating.valid_time_period = jsonObject_danger_rating["validTimePeriod"].toString();

            // Get upper and lower bound between which rating is valid
            if (jsonObject_danger_rating.contains("elevation")) {
                QJsonObject jsonObject_elevation = jsonObject_danger_rating["elevation"].toObject();
                if (jsonObject_elevation.contains("upperBound"))
                    rating.upper_bound = jsonObject_elevation["upperBound"].toInt();
                else
                    rating.upper_bound = INT_MAX;
                if (jsonObject_elevation.contains("lowerBound"))
                    rating.lower_bound = jsonObject_elevation["lowerBound"].toInt();
                else
                    rating.lower_bound = INT_MIN;
            }

            // add current rating to bulletin item
            bulletin_item.danger_ratings.push_back(rating);
        }

        // Parse regions for which danger ratings in this jsonObject_bulletin_item are valid
        QJsonArray jsonArray_regions = jsonObject_bulletin_item["regions"].toArray();
        for (const QJsonValue& jsonValue_in_regions_array : jsonArray_regions) {
            if (!jsonValue_in_regions_array.isObject()) {
                error_message.append("jsonValue in regions array is not jsonObject");
                emit this->load_CAAML_finished(tl::unexpected(error_message));
                return;
            }
            QJsonObject jsonObject_region_info = jsonValue_in_regions_array.toObject();
            if (!jsonObject_region_info.contains("regionID") || !jsonObject_region_info["regionID"].isString()) {
                error_message.append("key \"regionID\" does not exist or does not contain string");
                emit this->load_CAAML_finished(tl::unexpected(error_message));
                return;
            }
            bulletin_item.regions_ids.insert(jsonObject_region_info["regionID"].toString());
        }

        // add bulletin item to eaws bulletin
        eawsBulletin.push_back(bulletin_item);
    }
    emit this->load_CAAML_finished(tl::expected<std::vector<BulletinItemCAAML>, QString>(eawsBulletin));
    return;
}

void ReportLoadService::load_tu_wien(const QDate& date) const
{
    // TODO: Convert date to url
    QString date_string = date.toString();
    QUrl qurl(QString("https://alpinemaps.cg.tuwien.ac.at/avalanche-reports/get-current-report?date=2024-02-20"));
    QNetworkRequest request(qurl);
    request.setTransferTimeout(int(8000));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    request.setAttribute(QNetworkRequest::UseCredentialsAttribute, false);
#endif

    // Make a GET request to the provided url
    QNetworkAccessManager network_manager;
    QNetworkReply* reply = network_manager.get(request);

    // Process the reply
    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        // Error message is emitted in case something goes wrong
        QString error_message("ERROR: ");

        // Check if Network Error occured
        if (reply->error() != QNetworkReply::NoError) {
            emit this->load_TU_Wien_finished(tl::unexpected(error_message));
        }

        // Read the response data
        QByteArray data = reply->readAll();

        // Convert data to Json
        QJsonParseError parse_error;
        QJsonDocument json_document = QJsonDocument::fromJson(data, &parse_error);

        // Check for parsing error
        if (parse_error.error != QJsonParseError::NoError) {
            error_message.append("Parse error = ").append(parse_error.errorString());
            emit this->load_TU_Wien_finished(tl::unexpected(error_message));
            return;
        }

        // Check for empty json
        if (json_document.isEmpty() || json_document.isNull()) {
            error_message.append("Empty or Null json.");
            emit this->load_TU_Wien_finished(tl::unexpected(error_message));
            return;
        }

        // Check if key with reports is correct
        if (!json_document.isObject()) {
            error_message.append("jsonDocument does not contain json object.");
            emit this->load_TU_Wien_finished(tl::unexpected(error_message));
            return;
        }
        QJsonObject json_object = json_document.object();
        if (!json_object.contains("report")) {
            error_message.append("json object does not contain key \"report\"");
            emit this->load_TU_Wien_finished(tl::unexpected(error_message));
            return;
        }

        // Check if array is contained in json
        if (!json_object["report"].isArray()) {
            error_message.append("json object[\"report\"] does not contain array");
            emit this->load_TU_Wien_finished(tl::unexpected(error_message));
            return;
        }
        QJsonArray jsonArray_report = json_object["report"].toArray();

        // parse array containing report for each region
        std::vector<ReportTUWien> region_ratings;
        for (const QJsonValue& jsonValue_region_rating : jsonArray_report) {
            // prepare an itemthat goes inot the bulletin
            ReportTUWien region_rating;

            // Check if Json array contains json objects
            if (!jsonValue_region_rating.isObject()) {
                error_message.append("json object[\"report\"] is array of other type than json object");
                emit this->load_TU_Wien_finished(tl::unexpected(error_message));
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
            if (jsonObject_region_rating.contains("unfavorable"))
                region_rating.unfavorable = jsonObject_region_rating["unfavorable"].toInt();

            // Write struct to vector to be returned
            region_ratings.push_back(region_rating);
        }

        // Emit ratings
        emit this->load_TU_Wien_finished(tl::expected<std::vector<ReportTUWien>, QString>(region_ratings));
    });
}

// Function only used until get from server works
void ReportLoadService::load_report_from_file() const
{
    // Read the json from file for now
    QString filePath = "app\\eaws\\report_tu_wien.json";
    QFile jsonFile(filePath);
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    QByteArray data = jsonFile.readAll();
    jsonFile.close();
    QString error_message("ERROR: ");

    // Convert data to Json
    QJsonParseError parse_error;
    QJsonDocument json_document = QJsonDocument::fromJson(data, &parse_error);

    // Check for parsing error
    if (parse_error.error != QJsonParseError::NoError) {
        error_message.append("Parse error = ").append(parse_error.errorString());
        emit this->load_TU_Wien_finished(tl::unexpected(error_message));
        return;
    }

    // Check for empty json
    if (json_document.isEmpty() || json_document.isNull()) {
        error_message.append("Empty or Null json.");
        emit this->load_TU_Wien_finished(tl::unexpected(error_message));
        return;
    }

    // Check if json doc is array
    if (!json_document.isArray()) {
        error_message.append("jsonDocument does not contain array.");
        emit this->load_TU_Wien_finished(tl::unexpected(error_message));
        return;
    }
    QJsonArray jsonArray = json_document.array();

    // parse array containing report for each region
    std::vector<ReportTUWien> region_ratings;
    for (const QJsonValue& jsonValue_region_rating : jsonArray) {
        // prepare an item that goes into the bulletin
        ReportTUWien region_rating;

        // Check if Json array contains json objects
        if (!jsonValue_region_rating.isObject()) {
            error_message.append("json object is array of other type than json object");
            emit this->load_TU_Wien_finished(tl::unexpected(error_message));
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
    emit this->load_TU_Wien_finished(tl::expected<std::vector<ReportTUWien>, QString>(region_ratings));
}

void ReportLoadService::load_latest_TU_Wien() const { load_report_from_file(); }
/*
bool operator==(const avalanche::eaws::DangerRating& lhs, const avalanche::eaws::DangerRating& rhs)
{
    return lhs.main_value == rhs.main_value && lhs.lower_bound == rhs.lower_bound && lhs.upper_bound == rhs.upper_bound && lhs.valid_time_period == rhs.valid_time_period;
}

bool operator==(const avalanche::eaws::BulletinItem& lhs, const avalanche::eaws::BulletinItem& rhs)
{
    // if different amount danger ratings return false
    if (lhs.danger_ratings.size() != rhs.danger_ratings.size())
        return false;

    // compare all danger ratings
    for (uint i = 0; i < lhs.danger_ratings.size(); i++) {
        if (lhs.danger_ratings[i] != rhs.danger_ratings[i])
            return false;
    }

    // compare region ids
    if (!(lhs.regions_ids == rhs.regions_ids))
        return false;

    // all comparisons passed, return true
    return true;
}
*/

} // namespace avalanche::eaws
