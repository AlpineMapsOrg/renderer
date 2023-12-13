#include "GPX.h"

#include <srs.h>

#include <iostream>
#include <glm/gtx/io.hpp>

#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>

namespace nucleus {
namespace gpx {

    TrackPoint parse_trackpoint(QXmlStreamReader& xmlReader)
    {
        TrackPoint point;

        double latitude = xmlReader.attributes().value("lat").toDouble();
        double longitude = xmlReader.attributes().value("lon").toDouble();

        point.x = latitude;
        point.y = longitude;
        point.z = 0;

#if 1
        while (!xmlReader.atEnd() && !xmlReader.hasError()) {
            QXmlStreamReader::TokenType token = xmlReader.readNext();

            if (token == QXmlStreamReader::StartElement) {
                if (xmlReader.name() == QString("ele")) {
                    float elevation = xmlReader.readElementText().toDouble();
                    point.z = elevation;
                }

                break;
            }
        }
#endif

        return point;
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

        while (!xmlReader.atEnd() && !xmlReader.hasError()) {
            QXmlStreamReader::TokenType token = xmlReader.readNext();

            if (token == QXmlStreamReader::StartElement) {
                // Handle the start element
                QStringView name = xmlReader.name();

                if (name == QString("trk")) {
                    // nothing to do here
                } else if (name == QString("trkpt")) {
                    if (gpx->track.size() == 0) {
                        gpx->track.push_back(TrackSegment());
                    }

                    TrackPoint track_point = parse_trackpoint(xmlReader);
                    gpx->track.back().push_back(track_point);

                } else if (name == QString("trkseg")) {
                    gpx->track.push_back(TrackSegment());

                } else if (name == QString("wpt")) {
                    std::cerr << "'wpt' NOT IMPLEMENTED!\n";
                    return nullptr;

                } else if (name == QString("rte")) {
                    std::cerr << "'rte' NOT IMPLEMENTED!\n";
                    return nullptr;
                }
            } else if (token == QXmlStreamReader::EndElement) {
                // Handle the end element
            } else if (token == QXmlStreamReader::Characters && !xmlReader.isWhitespace()) {
                // Handle text content
            }
        }

        if (xmlReader.hasError()) {
            // Handle the XML parsing error
            qDebug() << "XML Parsing Error: " << xmlReader.errorString();
            return nullptr;
        }

        file.close();
        return gpx;
    }
} // namespace gpx

Track::Track(const gpx::Gpx& gpx)
{
    std::vector<glm::dvec3> combined;

    for (const gpx::TrackSegment& segment : gpx.track) {
        combined.insert(combined.end(), segment.begin(), segment.end());
    }

    std::vector<glm::vec3> track(combined.size());

    points.reserve(combined.size());

    for (int i = 0; i < combined.size(); i++) {
        points[i] = glm::vec3(srs::lat_long_alt_to_world(combined[i]));
    }
}

std::vector<glm::vec3> to_world_points(const gpx::Gpx& gpx)
{
    std::vector<glm::dvec3> track;

    for (const gpx::TrackSegment& segment : gpx.track) {
        track.insert(track.end(), segment.begin(), segment.end());
    }

    std::vector<glm::vec3> points(track.size());

    for (int i = 0; i < track.size(); i++) {
        points[i] = glm::vec3(srs::lat_long_alt_to_world(track[i]));
        // std::cout << points[i] << std::endl;
    }

    std::cout << points[0] << std::endl;
    std::cout << points[1] << std::endl;
    return points;
}

std::vector<glm::vec3> to_world_ribbon(const std::vector<glm::vec3>& points, float width)
{
    std::vector<glm::vec3> ribbon;

    const glm::vec3 offset = glm::vec3(0.0f, 0.0f, width);

    for (size_t i = 0; i < points.size() - 1U; i++)
    {
        auto a = points[i];
        auto b = points[i + 1];

        // triangle 1
        ribbon.insert(ribbon.end(), {
            a + offset,
            b - offset,
            a - offset
        });

        // triangle 2
        ribbon.insert(ribbon.end(), {
            a + offset,
            b + offset,
            b - offset
        });
    }
    return ribbon;
}


std::vector<glm::vec3> to_world_ribbon_with_normals(const std::vector<glm::vec3>& points, float width)
{
    std::vector<glm::vec3> ribbon;

    const glm::vec3 offset = glm::vec3(0.0f, 0.0f, width);

    for (size_t i = 0; i < points.size() - 1U; i++)
    {
        auto a = points[i];
        auto b = points[i + 1];

        // tangent is negative for vertices below the original line
        glm::vec3 tangent = glm::normalize(b - a);

#if 0
        if (i ==  0)
        {

        } else if (i == points.size() - 1U)
        {

        }
        else 
        {

        }
#endif

        //auto normal = glm::vec3();

        // triangle 1
        ribbon.insert(ribbon.end(), {
            a + offset,  tangent,
            b - offset, -tangent,
            a - offset, -tangent
        });

        // triangle 2
        ribbon.insert(ribbon.end(), {
            a + offset,  tangent,
            b + offset,  tangent,
            b - offset, -tangent
        });
    }
    return ribbon;


}

} // namespace nucleus
