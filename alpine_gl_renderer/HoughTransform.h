#ifndef HOUGHTRANSFORM_H
#define HOUGHTRANSFORM_H

#include <QImage>
#include <set>

#include <glm/vec2.hpp>
#include "nucleus/Raster.h"


struct IvecCompare {
    bool operator() (const glm::ivec2& lhs, const glm::ivec2& rhs) const {
        if (lhs.x != rhs.x) {
            return lhs.x < rhs.x;
        }
        return lhs.y < rhs.y;
    }
};

class HoughTransform
{
    constexpr static int AZIMUTH_BEGIN = 90;
    constexpr static int AZIMUTH_END = 270;
    constexpr static int AZIMUTH_SIZE = AZIMUTH_END - AZIMUTH_BEGIN;
    constexpr static int ALTITUDE_MAX = 90;

    long long space[AZIMUTH_SIZE][ALTITUDE_MAX];

    std::set<glm::ivec2, IvecCompare> sun_normals;

public:
    HoughTransform();

    //@brief does the hough transform for the given shadow and height map
    //
    //@side effects: addes all results to its internal hough space, and stores the hough space for this photo to an image specified by heatmap_full_path
    //@return returns the most likely sun position for one orthofoto given the ortofotos shadow and height maps
    glm::vec2 get_most_likely_sun_position(const QImage& shadow_map, const Raster<uint16_t>& height_map, const QString& heatmap_full_path, double data_spacing);

    glm::ivec2 generate_heatmap(const long long space[AZIMUTH_SIZE][ALTITUDE_MAX], const QString& name);

    //adds all normals which should be considered as 'in the sun' in the next get_most_likely_sun_position invocation
    void add_sun_normals(const QImage& shadow_map, const Raster<uint16_t>& height_map, double data_spacing);
    void clear_sun_normals();

    //generates a heatmap from the internal hough space
    glm::ivec2 generate_space_heatmap(const QString& name);
    void clear_space();

    glm::vec3 parameter_to_vector(const glm::vec2& parameter);
    glm::vec2 vector_to_parameter(const glm::vec3& vector);

private:
    std::vector<std::vector<glm::vec3>> calculate_normals(const Raster<uint16_t>& height_map, double data_spacing);
    glm::vec3 interpolate_normal(const glm::vec2& coords, const glm::vec2& coords_max, const std::vector<std::vector<glm::vec3>>& normals);
};

#endif // HOUGHTRANSFORM_H
