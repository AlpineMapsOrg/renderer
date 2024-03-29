/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "TextureView.h"

namespace webgpu_engine::raii {

TextureView::TextureView(WGPUTexture texture_handle, const WGPUTextureViewDescriptor& desc)
    : m_texture_view(wgpuTextureCreateView(texture_handle, &desc))
    , m_texture_view_descriptor(desc)
{
}

TextureView::~TextureView() { wgpuTextureViewRelease(m_texture_view); }

WGPUTextureViewDimension TextureView::dimension() const { return m_texture_view_descriptor.dimension; }

WGPUTextureView TextureView::handle() const { return m_texture_view; }

} // namespace webgpu_engine::raii
