#include "Track.h"

#include <srs.h>

#include <glm/gtx/io.hpp>

#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>

namespace nucleus {
namespace gpx {

    TrackPoint parse_trackpoint(QXmlStreamReader& xmlReader)
    {
        TrackPoint point;

        double lat = xmlReader.attributes().value("lat").toDouble();
        double lon = xmlReader.attributes().value("lon").toDouble();

        point.x = lat, point.z = lon;

#if 1
        while (!xmlReader.atEnd() && !xmlReader.hasError()) {
            QXmlStreamReader::TokenType token = xmlReader.readNext();

            if (token == QXmlStreamReader::StartElement) {
                if (xmlReader.name() == QString("ele")) {
                    point.y = xmlReader.readElementText().toDouble();
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

        unsigned track_segment = 0;

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
                    TrackSegment& current_segment = gpx->track.back();
                    current_segment.push_back(track_point);

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
} // namespace nucleus
