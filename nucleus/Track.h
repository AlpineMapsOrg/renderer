
#include <QString>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace nucleus {

// https://www.topografix.com/gpx.asp
// https://en.wikipedia.org/wiki/GPS_Exchange_Format
// TODO: handle waypoint and route
namespace gpx {

    using TrackPoint = glm::dvec3; // TODO: time?
    using TrackSegment = std::vector<TrackPoint>;

    struct Gpx {
        std::vector<TrackSegment> track;
    };

    std::unique_ptr<Gpx> parse(const QString& path);

} // namespace gpx

} // namespace nucleus
