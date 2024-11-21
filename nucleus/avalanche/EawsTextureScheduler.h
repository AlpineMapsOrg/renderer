/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include "nucleus/avalanche/eaws.h"
#include "nucleus/tile/TextureScheduler.h"
// #include "types.h"
namespace avalanche::eaws {

class EawsTextureScheduler : public nucleus::tile::TextureScheduler {
    Q_OBJECT
public:
    EawsTextureScheduler(std::string name, unsigned texture_resolution, QObject* parent = nullptr);
    ~EawsTextureScheduler();

protected:
    void transform_and_emit(const std::vector<nucleus::tile::DataQuad>& new_quads, const std::vector<nucleus::tile::Id>& deleted_quads);

private:
    nucleus::utils::ColourTexture::Format m_compression_algorithm = nucleus::utils::ColourTexture::Format::DXT1;
    nucleus::Raster<glm::u8vec4> m_default_raster;
    avalanche::eaws::UIntIdManager m_internal_id_manager;
};

} // namespace avalanche::eaws
