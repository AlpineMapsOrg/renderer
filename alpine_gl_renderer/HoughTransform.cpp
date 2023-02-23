#include "HoughTransform.h"
#include <QDebug>

static uint16_t get_idx(int x, int y);

HoughTransform::HoughTransform() :
    sun_normals()
{
    clear_space();
}

void HoughTransform::clear_space() {
    memset(space, 0, sizeof(space));
}

glm::ivec2 HoughTransform::generate_heatmap(const long long space[AZIMUTH_SIZE][ALTITUDE_MAX], const QString& name) {
    QImage heatmap(AZIMUTH_SIZE, ALTITUDE_MAX, QImage::Format::Format_Grayscale8);
    glm::ivec2 max(0.f, 0.f);
    int max_value = -1;
    int n = 0;

    for (int x = 0; x < AZIMUTH_SIZE; x++) {
        for (int y = 0; y < ALTITUDE_MAX; y++) {
            if (space[x][y] > max_value) {
                max.x = x;
                max.y = y;

                max_value = space[x][y];
                n = 1;
            } else if (space[x][y] == max_value) {
                n++;
            }
        }
    }
    max.x += AZIMUTH_BEGIN;

    qDebug() << "max_value = " << max_value << " - n = " << n;

    for (int x = 0; x < AZIMUTH_SIZE; x++) {
        for (int y = 0; y < ALTITUDE_MAX; y++) {
            int value = max_value > 0 ? (space[x][y] * 256) / max_value : 0;

            heatmap.setPixel(x, y, qRgba(value, value, value, value));
        }
    }

    heatmap.save(name, "PNG");
    return max;
}

glm::ivec2 HoughTransform::generate_space_heatmap(const QString& name) {
    return generate_heatmap(space, name);
}

void HoughTransform::add_sun_normals(const QImage& shadow_map, const Raster<uint16_t>& height_map, double data_spacing) {
    std::vector<std::vector<glm::vec3>> normals = calculate_normals(height_map, data_spacing);

    for (int x=0; x < 256; x++) {
        const uchar* scanline = reinterpret_cast<const uchar*>(shadow_map.constScanLine(x));
        for (int y=0; y < 256; y++) {
            if (scanline[y] < 128) {
                glm::ivec2 image_coords(x, y);
                glm::vec3 normal = interpolate_normal(image_coords, glm::vec2(255, 255), normals);

                glm::ivec2 parameter = glm::round(vector_to_parameter(normal));

                sun_normals.emplace(parameter);

                if (parameter.x < 0 || parameter.x > 360) {
                    qDebug() << "parameter = [" << parameter.x << "|" << parameter.y << "] -- normal = [" << normal.x << "|" << normal.y << "|" << normal.z << "]";
                }


            }
        }
    }
}

void HoughTransform::clear_sun_normals() {
    sun_normals.clear();
}

std::vector<std::vector<glm::vec3>> HoughTransform::calculate_normals(const Raster<uint16_t>& height_map, double data_spacing) {
    std::vector<std::vector<glm::vec3>> ret;

    for (int x=0; x < height_map.height(); x++) {
        ret.push_back(std::vector<glm::vec3>());


        for (int y=0; y < height_map.width(); y++) {
            uint16_t my_height = height_map.buffer()[get_idx(x,y)];

            glm::vec3 normal(0.f, 0.f, 0.f);

            for (int i=0; i<4; i++) {
                glm::vec3 vecs[2];

                for (int k = 0; k<2; k++) {
                    vecs[k] = glm::vec3(0.f);

                    int x_cpy = x;
                    int y_cpy = y;

                    switch((i + k) % 4) {
                    case 0: x_cpy += 1; vecs[k].x = data_spacing; break;
                    case 1: y_cpy -= 1; vecs[k].y = -data_spacing; break;
                    case 2: x_cpy -= 1; vecs[k].x = -data_spacing; break;
                    case 3: y_cpy += 1; vecs[k].y = data_spacing; break;
                    }

                    if (x_cpy < 0 || x_cpy >= height_map.height() ||
                        y_cpy < 0 || y_cpy >= height_map.height()) {
                        continue;
                    }

                    vecs[k].z = height_map.buffer()[get_idx(x_cpy, y_cpy)] - my_height;
                }

                normal += glm::normalize(glm::cross(vecs[1], vecs[0]));
            }
            normal = glm::normalize(normal);

            ret[x].push_back(normal);
        }
    }

    return ret;
}

glm::vec3 HoughTransform::interpolate_normal(const glm::vec2& coords, const glm::vec2& coords_max, const std::vector<std::vector<glm::vec3>>& normals) {
    glm::ivec2 max = glm::ivec2(normals.size() - 1, normals[0].size() - 1);
    glm::vec2 normal_coords = glm::vec2((coords.x * max.x) / coords_max.x, (coords.y * max.y) / coords_max.y);

    glm::ivec2 normal_floor = glm::floor(normal_coords);
    glm::vec2 normal_fract = glm::fract(normal_coords);

    glm::vec3 top;
    glm::vec3 bot;

    if (glm::abs(normal_coords.x - max.x) < 0.001) {
        if (glm::abs(normal_coords.y - max.y) < 0.001) {
            return glm::normalize(normals[normal_floor.x][normal_floor.y]);
        }
        top = glm::normalize(normals[normal_floor.x][normal_floor.y]);
        bot = glm::normalize(normals[normal_floor.x][normal_floor.y + 1]);
    } else {
        if (glm::abs(normal_coords.y - max.y) < 0.001) {
            top = glm::normalize(glm::mix(normals[normal_floor.x][normal_floor.y], normals[normal_floor.x + 1][normal_floor.y], normal_fract.x));
            bot = top;
        } else {
            top = glm::normalize(glm::mix(normals[normal_floor.x][normal_floor.y], normals[normal_floor.x + 1][normal_floor.y], normal_fract.x));
            bot = glm::normalize(glm::mix(normals[normal_floor.x][normal_floor.y + 1], normals[normal_floor.x + 1][normal_floor.y + 1], normal_fract.x));
        }
    }

    return glm::normalize(glm::mix(top, bot, normal_fract.y));
}
//parameter in DEGREES
glm::vec3 HoughTransform::parameter_to_vector(const glm::vec2& parameter) {
    double azimuth = parameter.x;
    double altitude = parameter.y;

    return glm::vec3(
                glm::sin(glm::radians(static_cast<float>(azimuth))) * glm::sin(glm::radians(static_cast<float>(altitude))),
                glm::cos(glm::radians(static_cast<float>(azimuth))) * glm::sin(glm::radians(static_cast<float>(altitude))),
                glm::cos(glm::radians(static_cast<float>(altitude))));

}

//parameter in DEGREES
glm::vec2 HoughTransform::vector_to_parameter(const glm::vec3& vector) {
    glm::vec3 normal = glm::normalize(vector);

    double altitude = glm::degrees(glm::acos(normal.z));
    double azimuth = glm::degrees(glm::acos(glm::max<double>(-1.0, glm::min<double>(1.0, normal.y / glm::sin(glm::radians(altitude))))));

    if (normal.x < 0) {
        azimuth = 360 - azimuth;
    }

    return glm::vec2(azimuth, altitude);
}

glm::vec2 HoughTransform::get_most_likely_sun_position(const QImage& shadow_map, const Raster<uint16_t>& height_map, const QString& heatmap_name, double data_spacing) {

    long long local_space[AZIMUTH_SIZE][ALTITUDE_MAX];

    memset(local_space, 0, sizeof(local_space));

    std::vector<std::vector<glm::vec3>> normals = calculate_normals(height_map, data_spacing);

    for (int x=0; x < 256; x++) {
        const uchar* scanline = reinterpret_cast<const uchar*>(shadow_map.constScanLine(x));

        for (int y=0; y < 256; y++) {
            glm::ivec2 image_coords(x, y);
            glm::vec3 normal = interpolate_normal(image_coords, glm::vec2(255, 255), normals);

            //qDebug() << "normal = [" << normal.x << "|" << normal.y << "|" << normal.z << "]";

            //double altidude = glm::degrees(glm::atan(normal.x, normal.y));
            //double azimuth = glm::degrees(glm::atan(glm::sqrt(normal.x * normal.x + normal.y * normal.y), normal.z));

            //shadow_map.
            //QRgb pixel_value = qRed(scanline[y]);
            uchar pixel_value = scanline[y];
            bool in_shadow = pixel_value > 128 && !sun_normals.contains(glm::round(vector_to_parameter(normal)));

            //qDebug() << "[" << x << "|" << y << "] -> " << "[" << qRed(pixel_value) << "|" << qGreen(pixel_value) << "|" <<  qBlue(pixel_value) << "]";

            for (int theta = AZIMUTH_BEGIN; theta < AZIMUTH_END; theta++) {
                for (int phi = 0; phi < ALTITUDE_MAX; phi++) {
                    glm::vec3 vec = parameter_to_vector(glm::vec2(theta, phi));
               //    if (x == 1 && y == 1) {
               //         qDebug() << vec.x << "|" << vec.y << "|" << vec.z;
               //     }

                    if (in_shadow == (glm::dot(normal, vec) < 0)) {
                        local_space[theta - AZIMUTH_BEGIN][phi] += 1;
                        space[theta - AZIMUTH_BEGIN][phi] += 1;
                    }
                }
            }
        }
    }

    return glm::vec2(generate_heatmap(local_space, heatmap_name));
}

uint16_t get_idx(int x, int y) {
    return x * 65 + y;
}
