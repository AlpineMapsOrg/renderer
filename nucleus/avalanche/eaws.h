#ifndef EAWS_H
#define EAWS_H
#include <QString>
#include <glm/glm.hpp>
namespace avalanche::eaws {
struct EawsRegion {
public:
    QString id = ""; // The id is the name of the region e.g. "AT-05-18"
    QString id_alt = ""; // So far it is not clear what id_alt of an EAWS Region is
    QString start_date = ""; // The day the region was defined
    QString end_date = ""; // Only for outdated regions: The day the regions expired
    std::vector<glm::ivec2> verticesInLocalCoordinates; // The vertices of the region's bounding polygon
};
} // namespace avalanche::eaws
#endif // EAWS_H
