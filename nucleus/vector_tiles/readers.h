#ifndef READER_H
#define READER_H
#include "nucleus/avalanche/eaws.h"
#include <QDebug>
#include <QFile>
#include <extern/tl_expected/include/tl/expected.hpp>
#include <mapbox/vector_tile.hpp>
#include <string>
#include <vector>

namespace vector_tile::reader {
/* Reads all EAWS regions stored in a provided vector tile
 * Returns a vector of structs, each containing the name, geometry and "alt-id", "start_date", "end_date" if applicable.
 * The mvt file is expected to contain three layers: "micro-regions", "micro-regions_elevation", "outline".
 * The first one is relevant for this class.
 * The parsed layer "micro-regions" contains several features, each feature representing one micro region.
 * Every feature contains at least one poperty and one geometry, the latter one holding the vertices of the region's boundary polygon.
 * Every Property has an id. There must be at least one poperty with its id holding the name-string of the region (e.g. "FR-64").
 * Further properties can exist with ids (all string) "alt-id", "start_date", "end_date".
 * If a region has the property with id "end_date" this region is outdated and was substituted by other regions.
 * These old regions are kept in the data set to allow processing of older bulletins that were using these regions.
 *
 * @param inputData : An array holding the data read froma vector tile (usually obtained by reading a from a mvt file).
 */
tl::expected<std::vector<avalanche::eaws::Region>, QString> eaws_regions(const QByteArray& input_data);
} // namespace vector_tile::reader
#endif
