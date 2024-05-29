#ifndef EAWS_H
#define EAWS_H
#include <QDate>
#include <QString>
#include <glm/glm.hpp>
#include <optional>
namespace avalanche::eaws {
struct EawsRegion {
public:
    QString id = ""; // The id is the name of the region e.g. "AT-05-18"
    std::optional<QString> id_alt = std::nullopt; // So far it is not clear what id_alt of an EAWS Region is
    std::optional<QDate> start_date = std::nullopt; // The day the region was defined
    std::optional<QDate> end_date = std::nullopt; // Only for outdated regions: The day the regions expired
    std::vector<glm::ivec2> vertices_in_local_coordinates; // The vertices of the region's bounding polygon with respect to tile resolution
    static constexpr glm::ivec2 resolution = glm::vec2(4096, 4096); // Tile resolution
};

// Returns a map of tuples (internal_region_id, region_name)
std::tuple<std::map<std::string, uint>, std::map<uint, std::string>> create_internal_ids(const std::vector<avalanche::eaws::EawsRegion>& eaws_regions);

} // namespace avalanche::eaws
#endif // EAWS_H
