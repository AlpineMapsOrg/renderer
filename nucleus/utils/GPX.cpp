/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2024 Jakob Maier
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "GPX.h"

#include <srs.h>

#include <glm/gtx/io.hpp>

#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>
#include <QDateTime>

namespace nucleus {
namespace gpx {

    std::unique_ptr<Gpx> parse(QXmlStreamReader& xmlReader)
    {
        auto gpx = std::make_unique<Gpx>();

        while (!xmlReader.atEnd() && !xmlReader.hasError()) {
            QXmlStreamReader::TokenType token = xmlReader.readNext();

            if (token == QXmlStreamReader::StartElement) {
                // Handle the start element

                QStringView name = xmlReader.name();

                if (name == QString("trk")) {
                    // nothing to do here
                } else if (name == QString("trkpt")) {
                    TrackPoint point;
                    double latitude = xmlReader.attributes().value("lat").toDouble();
                    double longitude = xmlReader.attributes().value("lon").toDouble();

                    point.latitude = latitude;
                    point.longitude = longitude;

                    gpx->add_new_point(point);

                } else if (name == QString("ele")) {
                    TrackPoint& point = gpx->last_point();
                    point.elevation = xmlReader.readElementText().toDouble();

                } else if (name == QString("time")) {
                    QString iso_date_string = xmlReader.readElementText();
                    QDateTime date_time = QDateTime::fromString(iso_date_string, Qt::ISODate);

                    if (date_time.isValid()) {
                        // check is needed, as "time" can exist in different than a trackpoint context
                        if (gpx->track.size() > 0 && gpx->track.back().size() > 0) {
                            TrackPoint& point = gpx->track.back().back();
                            point.timestamp = date_time;
                        }
                    } else {
                        qDebug() << "Failed to parse date " << iso_date_string;
                    }

                } else if (name == QString("trkseg")) {
                    gpx->add_new_segment();

                } else if (name == QString("wpt")) {
                    qDebug() << "'wpt' NOT IMPLEMENTED!\n";
                    return nullptr;

                } else if (name == QString("rte")) {
                    qDebug() << "'rte' NOT IMPLEMENTED!\n";
                    return nullptr;
                }
            } else if (token == QXmlStreamReader::EndElement) {
                // Handle the end element
                // qDebug() << "Found end element"  << xmlReader.name();
            } else if (token == QXmlStreamReader::Characters && !xmlReader.isWhitespace()) {
                // Handle text content
            }
        }

        if (xmlReader.hasError()) {
            // Handle the XML parsing error
            qDebug() << "XML Parsing Error: " << xmlReader.errorString();
            return nullptr;
        }

        return gpx;
    }

    std::unique_ptr<Gpx> parse(const QString& path)
    {
        std::unique_ptr<Gpx> gpx = std::make_unique<Gpx>();

        QFile file(path);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            std::cerr << "Could not open file " << path.toStdString() << std::endl;
            return nullptr;
        }

        QXmlStreamReader xmlReader(&file);

        return parse(xmlReader);
    }
} // namespace gpx

std::vector<glm::vec4> to_world_points(const gpx::Gpx& gpx)
{
    std::vector<glm::vec4> track;

    const QDateTime epoch(QDate(1970, 1, 1).startOfDay());

    for (const gpx::TrackSegment& segment : gpx.track) {
        for (size_t i = 0U; i < segment.size(); ++i) {

            float time_since_epoch = 0;

            if (i > 0) {
                time_since_epoch = static_cast<float>(segment[i - 1].timestamp.msecsTo(segment[i].timestamp));
            }

            auto point = glm::dvec3(segment[i].latitude,segment[i].longitude,segment[i].elevation);
            track.push_back(glm::vec4(srs::lat_long_alt_to_world(point), time_since_epoch));
        }
    }

    return track;
}

std::vector<glm::vec4> to_world_points(const gpx::TrackSegment& segment)
{
    std::vector<glm::vec4> track;

    const QDateTime epoch(QDate(1970, 1, 1).startOfDay());

    for (size_t i = 0U; i < segment.size(); ++i) {

        float time_since_epoch = 0;

        if (i > 0) {
            time_since_epoch = static_cast<float>(segment[i - 1].timestamp.msecsTo(segment[i].timestamp));
        }

        auto point = glm::dvec3(segment[i].latitude,segment[i].longitude,segment[i].elevation);
        track.push_back(glm::vec4(srs::lat_long_alt_to_world(point), time_since_epoch));
    }

    return track;

}

std::vector<glm::vec3> triangle_strip_ribbon(const std::vector<glm::vec3>& points, float width)
{
    std::vector<glm::vec3> ribbon;

    const glm::vec3 offset = glm::vec3(0.0f, 0.0f, width);

    const glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 down = glm::vec3(0.0f, 0.0f, -1.0f);

    for (auto i = 0U; i < points.size() - 1U; i++)
    {
        auto a = points[i];

        ribbon.insert(ribbon.end(), {
            a - offset, down,
            a + offset, up,
        });
    }
    return ribbon;
}

std::vector<glm::vec3> triangles_ribbon(const std::vector<glm::vec4>& points, float width, int index_offset)
{

    float max_delta_time = 0;
    float max_dist = 0;
    float max_speed = 0;

    for (size_t i = 0; i < points.size(); ++i) {
        glm::vec4 point = points[i];
        float delta_time = point.w;
        max_delta_time = glm::max(max_delta_time, delta_time);

        if (i > 0) {
            float dist = glm::distance(points[i], points[i - 1]);
            max_dist = glm::max(max_dist, dist);

            float speed = dist / delta_time;

            max_speed = glm::max(max_speed, speed);
        }
    }

    std::vector<glm::vec3> ribbon;

    const glm::vec3 offset = glm::vec3(0.0f, 0.0f, width);

    for (size_t i = 0; i < points.size() - 1U; i++)
    {
        auto a = glm::vec3(points[i]);
        auto b = glm::vec3(points[i + 1]);
        auto d = glm::normalize(b - a);

        auto up = glm::vec3(0.0f, 0.0f, 1.0f);
        auto down = glm::vec3(0.0f, 0.0f, -1.0f);

        auto start = glm::vec3(1.0f, 0.0f, 0.0f);
        auto end = glm::vec3(-1.0f, 0.0f, 0.0f);

        auto index = glm::vec3(0.0f, static_cast<float>(index_offset + i), 0.0f);

        // triangle 1
        ribbon.insert(ribbon.end(), {
            a + offset, d, up + start + index,    /* metadata,*/
            a - offset, d, down + start + index,  /* metadata,*/
            b - offset, d, down + end + index,    /* metadata,*/
        });

        // triangle 2
        ribbon.insert(ribbon.end(), {
            a + offset, d, up + start + index,  /* metadata, */
            b - offset, d, down + end + index,  /* metadata, */
            b + offset, d, up + end + index,    /* metadata, */
        });
    }
    return ribbon;
}

std::vector<unsigned> ribbon_indices(unsigned point_count)
{
    std::vector<unsigned> indices;

    for (unsigned i = 0; i < point_count; ++i)
    {
        unsigned idx = i * 2;
        // triangle 1
        indices.insert(indices.end(), { idx, idx + 1, idx + 3});

        // triangle 2
        indices.insert(indices.end(), { idx + 3, idx + 2, idx});
    }

    return indices;
}

// 1 dimensional gaussian
float gaussian_1D(float x, float sigma = 1.0f)
{
    return (1.0 / std::sqrt(2 * M_PI * sigma)) * std::exp(-(x * x) / (2 * (sigma * sigma)));
}

void apply_gaussian_filter(std::vector<glm::vec4>& points, float sigma)
{
    const int radius = 2;
    const int kernel_size = (radius * 2) + 1;
    float kernel[kernel_size];
    float kernel_sum = 0.0f;

    // create kernel
    for (int x = -radius; x <= radius; x++)
    {
        kernel[x + radius] = gaussian_1D(static_cast<float>(x), sigma);
        kernel_sum += kernel[x + radius];
    }

    // normalize kernel
    for (int i = 0; i < kernel_size; i++) kernel[i] /= kernel_sum;

    for (int i = radius; i < static_cast<int>(points.size()) - radius; i++)
    {
        glm::vec3 value(0.0f);

        for (int j = -radius; j <= radius; j++)
            value += glm::vec3(points[i + j]) * kernel[j + radius];

        points[i] = glm::vec4(value, points[i].w);
    }
}

void reduce_point_count(std::vector<glm::vec4>& points, float threshold)
{
    std::vector<glm::vec4> old_points = points;

    points.clear();

    for (size_t i = 0; i < old_points.size() - 1; ++i) {
        auto current_point = old_points[i];

        points.push_back(current_point);

        while (glm::distance(glm::vec3(current_point), glm::vec3(old_points[i + 1])) < threshold && i < old_points.size() - 1) {
            ++i;
        }
    }
}

} // namespace nucleus
