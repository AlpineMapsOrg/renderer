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

std::vector<glm::vec3> to_world_points(const gpx::Gpx& gpx)
{
    std::vector<glm::dvec3> track;

    for (const gpx::TrackSegment& segment : gpx.track) {
        track.insert(track.end(), segment.begin(), segment.end());
    }

    std::vector<glm::vec3> points(track.size());

    for (auto i = 0U; i < track.size(); i++) {
        points[i] = glm::vec3(srs::lat_long_alt_to_world(track[i]));
    }

    std::cout << points[0] << std::endl;
    std::cout << points[1] << std::endl;
    return points;
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

std::vector<glm::vec3> triangles_ribbon(const std::vector<glm::vec3>& points, float width)
{
    std::vector<glm::vec3> ribbon;

    const glm::vec3 offset = glm::vec3(0.0f, 0.0f, width);

    for (size_t i = 0; i < points.size() - 1U; i++)
    {
        auto a = points[i];
        auto b = points[i + 1];
        auto d = glm::normalize(b - a);

        auto up = glm::vec3(0.0f, 0.0f, 1.0f);
        auto down = glm::vec3(0.0f, 0.0f, -1.0f);

        auto start = glm::vec3(1.0f, 0.0f, 0.0f);
        auto end = glm::vec3(-1.0f, 0.0f, 0.0f);

        auto index = glm::vec3(0.0f, static_cast<float>(i), 0.0f);

        // triangle 1
        ribbon.insert(ribbon.end(), {
            a + offset, d, up + start + index,
            a - offset, d, down + start + index,
            b - offset, d, down + end + index,
        });

        // triangle 2
        ribbon.insert(ribbon.end(), {
            a + offset, d, up + start + index,
            b - offset, d, down + end + index,
            b + offset, d, up + end + index,
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

void apply_gaussian_filter(std::vector<glm::vec3>& points, float sigma)
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
            value += points[i + j] * kernel[j + radius];

        points[i] = value;
    }
}

void reduce_point_count(std::vector<glm::vec3>& points)
{
    std::vector<glm::vec3> old_points = points;

    points.clear();

    const float threshold = 15.0f; // some arbitrary, sensible value

    for (size_t i = 0; i < old_points.size() - 1; ++i) {
        glm::vec3 current_point = old_points[i];

        points.push_back(current_point);

        while (glm::distance(current_point, old_points[i + 1]) < threshold && i < old_points.size() - 1) {
            ++i;
        }
    }
}

} // namespace nucleus
