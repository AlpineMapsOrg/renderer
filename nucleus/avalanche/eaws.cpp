#include "eaws.h"

std::tuple<std::map<std::string, uint>, std::map<uint, std::string>> avalanche::eaws::create_internal_ids(
    const std::vector<avalanche::eaws::EawsRegion>& eaws_regions)
{
    // Sort Region Ids starting at 1 such that 0 means no region
    std::map<std::string, uint> get_internal_id_from_region_name;
    for (const avalanche::eaws::EawsRegion& region : eaws_regions) {
        get_internal_id_from_region_name[region.id.toStdString()] = 0;
    }
    std::map<uint, std::string> get_region_name_from_internal_id;
    uint i = 1;
    for (auto& x : get_internal_id_from_region_name) {
        x.second = i;
        get_region_name_from_internal_id[i] = x.first;
        i++;
    }

    return std::tie(get_internal_id_from_region_name, get_region_name_from_internal_id);
}
